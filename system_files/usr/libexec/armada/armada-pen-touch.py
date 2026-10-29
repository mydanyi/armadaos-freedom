#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 lich.wang and the armadaos-freedom contributors
"""Game Mode pen -> one-finger touch bridge.

gamescope opens tablet devices but drops tablet-tool events, and InputPlumber
(the shipped 0.81.0 and upstream main 0.83.1) has no stylus source driver, so
a built-in pen does nothing in Game Mode. This helper replays the pen tip as a single touch on a
virtual touchscreen, so the pen works wherever a finger does.

Scope (deliberately narrow):

* Only runs where the device profile names the pen evdev in
  ``ARMADA_GAMEMODE_PEN_TOUCH`` (exact device name). Empty, the default,
  makes ``--supported`` fail, so the unit's ExecCondition skips it.
* Only runs inside the Game Mode session (the user unit is bound to
  ``gamescope-session-plus@steam.service``). The desktop session keeps the
  pen as a tablet with pressure and tilt.
* Tip down/up and position are forwarded. Hover, pressure, tilt and the barrel
  buttons are dropped.
* The pen is grabbed (EVIOCGRAB) while the bridge runs, so nothing sees both
  the tablet and the touch. The grab is attempted while the tip is up; events
  queued before the grab are discarded and the touch state is then read back
  from the device.
* When the pen node goes away (driver reload) or cannot be opened or grabbed
  yet, the bridge lifts any held touch, removes the virtual device and waits
  for the pen to come back.
"""

import errno
import os
import select
import shlex
import signal
import struct
import subprocess
import sys
import time

try:
    import fcntl
except ImportError:  # pragma: no cover - non-Linux hosts cannot use the ioctl
    fcntl = None

CONFIG_KEY = "ARMADA_GAMEMODE_PEN_TOUCH"
VIRTUAL_NAME = "Armada pen touch"
RESCAN_SEC = 2.0
# Pen-node errors that mean "not there or not usable yet": the node vanished,
# udev has not set its group yet, or something else holds the grab.
TRANSIENT_ERRNOS = frozenset(
    {errno.ENODEV, errno.ENOENT, errno.EIO, errno.EACCES, errno.EPERM, errno.EBUSY}
)

EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
SYN_REPORT, SYN_DROPPED = 0, 3
BTN_TOUCH = 0x14A
KEY_MAX = 0x2FF
ABS_X, ABS_Y = 0x00, 0x01
ABS_MT_SLOT = 0x2F
ABS_MT_POSITION_X, ABS_MT_POSITION_Y = 0x35, 0x36
ABS_MT_TRACKING_ID = 0x39
INPUT_PROP_DIRECT = 0x01
BUS_VIRTUAL = 0x06
TRACKING_ID_MAX = 0xFFFF

# struct input_event on 64-bit: struct timeval (2 x long) + u16 + u16 + s32.
EVENT = struct.Struct("qqHHi")


def _ioc(direction, nr, size):
    return (direction << 30) | (size << 16) | (ord("E") << 8) | nr


_IOC_WRITE, _IOC_READ = 1, 2
EVIOCGRAB = _ioc(_IOC_WRITE, 0x90, 4)
EVIOCGKEY = _ioc(_IOC_READ, 0x18, (KEY_MAX + 7) // 8)


def eviocgabs(code):
    return _ioc(_IOC_READ, 0x40 + code, 24)


def _uioc(direction, nr, size):
    return (direction << 30) | (size << 16) | (ord("U") << 8) | nr


UI_DEV_CREATE = _uioc(0, 1, 0)
UI_DEV_DESTROY = _uioc(0, 2, 0)
UI_DEV_SETUP = _uioc(_IOC_WRITE, 3, 92)
UI_ABS_SETUP = _uioc(_IOC_WRITE, 4, 28)
UI_SET_EVBIT = _uioc(_IOC_WRITE, 100, 4)
UI_SET_KEYBIT = _uioc(_IOC_WRITE, 101, 4)
UI_SET_ABSBIT = _uioc(_IOC_WRITE, 103, 4)
UI_SET_PROPBIT = _uioc(_IOC_WRITE, 110, 4)


def log(message):
    print(f"armada-pen-touch: {message}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# Configuration and discovery
# ---------------------------------------------------------------------------


def shell_env(helper=None, runner=subprocess.run):
    """Return the ARMADA_* variables the device-env helper exports."""
    helper = helper or os.environ.get(
        "ARMADA_DEVICE_ENV", "/usr/libexec/armada/device-env"
    )
    command = (
        f'eval "$({shlex.quote(helper)})"; '
        'for name in "${!ARMADA_@}"; do printf "%s=%s\\n" "$name" "${!name}"; done'
    )
    proc = runner(
        ["bash", "-c", command],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    env = {}
    for line in (proc.stdout or "").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    return env


def configured_pen(env):
    """The pen device name from the profile, or None when the bridge is off."""
    name = env.get(CONFIG_KEY, "").strip()
    return name or None


def find_event_node(name, sysfs="/sys/class/input", devdir="/dev/input"):
    """Return /dev/input/eventN of the device whose name is exactly `name`."""
    try:
        entries = sorted(os.listdir(sysfs))
    except OSError:
        return None
    for entry in entries:
        if not entry.startswith("event"):
            continue
        try:
            with open(os.path.join(sysfs, entry, "device", "name")) as fh:
                dev_name = fh.read().rstrip("\n")
        except OSError:
            continue
        if dev_name == name:
            return os.path.join(devdir, entry)
    return None


# ---------------------------------------------------------------------------
# Event translation (pure)
# ---------------------------------------------------------------------------


class PenTouchTranslator:
    """Turn pen evdev events into single-slot multitouch frames.

    ``feed`` returns a list of frames; each frame is a list of
    ``(type, code, value)`` tuples to be written followed by SYN_REPORT.
    After SYN_DROPPED everything up to the next SYN_REPORT is discarded and
    ``needs_resync`` is set; the caller then reads the device state and calls
    ``resync``.
    """

    def __init__(self):
        self.x = 0
        self.y = 0
        self.down = False
        self.sent_down = False
        self.tracking_id = 0
        self.dropping = False
        self.needs_resync = False

    def feed(self, ev_type, code, value):
        if ev_type == EV_SYN and code == SYN_DROPPED:
            self.dropping = True
            return []
        if self.dropping:
            if ev_type == EV_SYN and code == SYN_REPORT:
                self.dropping = False
                self.needs_resync = True
            return []
        if ev_type == EV_ABS and code == ABS_X:
            self.x = value
        elif ev_type == EV_ABS and code == ABS_Y:
            self.y = value
        elif ev_type == EV_KEY and code == BTN_TOUCH:
            self.down = bool(value)
        elif ev_type == EV_SYN and code == SYN_REPORT:
            return self._frame()
        return []

    def resync(self, down, x, y):
        self.needs_resync = False
        self.down, self.x, self.y = bool(down), x, y
        return self._frame()

    def release(self):
        """Frames that lift a held touch (used on shutdown or device loss)."""
        self.down = False
        return self._frame()

    def _frame(self):
        position = [
            (EV_ABS, ABS_MT_SLOT, 0),
            (EV_ABS, ABS_MT_POSITION_X, self.x),
            (EV_ABS, ABS_MT_POSITION_Y, self.y),
            (EV_ABS, ABS_X, self.x),
            (EV_ABS, ABS_Y, self.y),
        ]
        if self.down and not self.sent_down:
            self.tracking_id = (self.tracking_id + 1) & TRACKING_ID_MAX
            self.sent_down = True
            return [
                [(EV_ABS, ABS_MT_SLOT, 0), (EV_ABS, ABS_MT_TRACKING_ID, self.tracking_id)]
                + position[1:]
                + [(EV_KEY, BTN_TOUCH, 1)]
            ]
        if self.down:
            return [position]
        if self.sent_down:
            self.sent_down = False
            return [
                [
                    (EV_ABS, ABS_MT_SLOT, 0),
                    (EV_ABS, ABS_MT_TRACKING_ID, -1),
                    (EV_KEY, BTN_TOUCH, 0),
                ]
            ]
        return []


# ---------------------------------------------------------------------------
# Device I/O
# ---------------------------------------------------------------------------


def read_absinfo(fd, code):
    """(value, minimum, maximum, fuzz, flat, resolution) of one axis."""
    buf = bytearray(24)
    fcntl.ioctl(fd, eviocgabs(code), buf)
    return struct.unpack("6i", buf)


def key_is_down(fd, code):
    buf = bytearray((KEY_MAX + 7) // 8)
    fcntl.ioctl(fd, EVIOCGKEY, buf)
    return bool(buf[code // 8] & (1 << (code % 8)))


def encode_frame(frame):
    events = [EVENT.pack(0, 0, t, c, v) for t, c, v in frame]
    events.append(EVENT.pack(0, 0, EV_SYN, SYN_REPORT, 0))
    return b"".join(events)


def uinput_abs_setup(code, info):
    _value, minimum, maximum, fuzz, flat, resolution = info
    return struct.pack("HH6i", code, 0, 0, minimum, maximum, fuzz, flat, resolution)


def uinput_dev_setup(name):
    encoded = name.encode()[:79]
    return struct.pack("HHHH80sI", BUS_VIRTUAL, 0, 0, 1, encoded, 0)


class UinputTouch:
    """A one-slot direct touchscreen with the pen's axis ranges."""

    def __init__(self, xinfo, yinfo, path="/dev/uinput"):
        self.fd = os.open(path, os.O_WRONLY | os.O_NONBLOCK | os.O_CLOEXEC)
        try:
            fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
            fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_ABS)
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, BTN_TOUCH)
            fcntl.ioctl(self.fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT)
            axes = {
                ABS_X: xinfo,
                ABS_Y: yinfo,
                ABS_MT_POSITION_X: xinfo,
                ABS_MT_POSITION_Y: yinfo,
                ABS_MT_SLOT: (0, 0, 0, 0, 0, 0),
                ABS_MT_TRACKING_ID: (0, 0, TRACKING_ID_MAX, 0, 0, 0),
            }
            for code, info in axes.items():
                fcntl.ioctl(self.fd, UI_SET_ABSBIT, code)
                fcntl.ioctl(self.fd, UI_ABS_SETUP, uinput_abs_setup(code, info))
            fcntl.ioctl(self.fd, UI_DEV_SETUP, uinput_dev_setup(VIRTUAL_NAME))
            fcntl.ioctl(self.fd, UI_DEV_CREATE)
        except OSError:
            os.close(self.fd)
            raise

    def send(self, frames):
        for frame in frames:
            os.write(self.fd, encode_frame(frame))

    def close(self):
        try:
            fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        except OSError:
            pass
        os.close(self.fd)


class Stop(Exception):
    """Raised from the signal handler to leave the event loop cleanly."""


class UinputError(Exception):
    """The virtual touchscreen could not be created; not a pen-node problem."""


def wait_for_tip_up(fd):
    """Return once the pen tip is up, so the grab does not start mid-stroke."""
    while key_is_down(fd, BTN_TOUCH):
        time.sleep(0.05)


def drain(fd):
    """Discard events queued before the grab."""
    while True:
        try:
            if not os.read(fd, EVENT.size * 64):
                return
        except BlockingIOError:
            return


def device_state(fd):
    return (
        key_is_down(fd, BTN_TOUCH),
        read_absinfo(fd, ABS_X)[0],
        read_absinfo(fd, ABS_Y)[0],
    )


def forward(data, translator, touch, fd):
    """Feed one read batch, resyncing as soon as a drop has ended."""
    for offset in range(0, len(data) - EVENT.size + 1, EVENT.size):
        _sec, _usec, ev_type, code, value = EVENT.unpack_from(data, offset)
        touch.send(translator.feed(ev_type, code, value))
        if translator.needs_resync:
            touch.send(translator.resync(*device_state(fd)))


def bridge(path):
    """Forward one pen node until it goes away. Raises Stop on shutdown."""
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK | os.O_CLOEXEC)
    touch = None
    grabbed = False
    translator = PenTouchTranslator()
    try:
        xinfo = read_absinfo(fd, ABS_X)
        yinfo = read_absinfo(fd, ABS_Y)
        try:
            touch = UinputTouch(xinfo, yinfo)
        except OSError as exc:
            raise UinputError(f"/dev/uinput: {exc.strerror}") from exc
        wait_for_tip_up(fd)
        fcntl.ioctl(fd, EVIOCGRAB, 1)
        grabbed = True
        drain(fd)
        touch.send(translator.resync(*device_state(fd)))
        log(f"{path}: forwarding the pen tip as touch ({xinfo[2]}x{yinfo[2]})")
        while True:
            select.select([fd], [], [])
            try:
                data = os.read(fd, EVENT.size * 64)
            except BlockingIOError:
                continue
            if not data:
                return
            forward(data, translator, touch, fd)
    finally:
        if touch is not None:
            try:
                touch.send(translator.release())
            except OSError:
                pass
            touch.close()
        if grabbed:
            try:
                fcntl.ioctl(fd, EVIOCGRAB, 0)
            except OSError:
                pass
        os.close(fd)


def _raise_stop(_signum, _frame):
    raise Stop()


def run(pen, bridge_fn=None, find=None, sleep=time.sleep):
    """Bridge the named pen whenever it is present. Returns on Stop."""
    bridge_fn = bridge_fn or bridge
    find = find or find_event_node
    last_problem = None
    try:
        while True:
            path = find(pen)
            if path is None:
                problem = "missing"
                if problem != last_problem:
                    log(f"waiting for input device {pen!r}")
            else:
                try:
                    bridge_fn(path)
                    problem = "eof"
                    log(f"{path}: end of stream")
                except OSError as exc:
                    if exc.errno not in TRANSIENT_ERRNOS:
                        raise
                    problem = exc.errno
                    if problem != last_problem:
                        log(f"{path}: {exc.strerror}; retrying")
            last_problem = problem
            sleep(RESCAN_SEC)
    except Stop:
        return 0


def main(argv=None, env=None):
    argv = sys.argv[1:] if argv is None else argv
    try:
        signal.signal(signal.SIGTERM, _raise_stop)
        signal.signal(signal.SIGINT, _raise_stop)
        pen = configured_pen(shell_env() if env is None else env)
        if argv == ["--supported"]:
            return 0 if pen else 1
        if argv:
            log("usage: armada-pen-touch.py [--supported]")
            return 2
        if pen is None:
            log(f"{CONFIG_KEY} is not set for this device; nothing to do")
            return 0
        return run(pen)
    except Stop:
        return 0
    except UinputError as exc:
        log(str(exc))
        return 1


if __name__ == "__main__":
    sys.exit(main())
