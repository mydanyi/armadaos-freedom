#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 lich.wang and the armadaos-freedom contributors
"""Unit tests for armada-pen-touch.py.

Standard library only. These exercise the configuration gate, device lookup
against a fake sysfs tree, the pure pen -> touch translation and the uinput
struct layouts. They do not open any input device.
"""

import importlib.util
import os
import struct
import subprocess
import tempfile
import unittest

_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_HERE)
_SYSTEM = os.path.join(_ROOT, "system_files", "usr")
_MODULE_PATH = os.path.join(_SYSTEM, "libexec", "armada", "armada-pen-touch.py")
_SERVICE_PATH = os.path.join(
    _SYSTEM, "lib", "systemd", "user", "armada-pen-touch.service"
)
_DROPIN_PATH = os.path.join(
    _SYSTEM,
    "lib",
    "systemd",
    "user",
    "gamescope-session-plus@steam.service.d",
    "40-armada-pen-touch.conf",
)
_DEVICES = os.path.join(_SYSTEM, "lib", "armada", "devices")
_DEVICE_ENV = os.path.join(_SYSTEM, "libexec", "armada", "device-env")

_spec = importlib.util.spec_from_file_location("pen_touch", _MODULE_PATH)
mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mod)

ABS, KEY, SYN = mod.EV_ABS, mod.EV_KEY, mod.EV_SYN


def report():
    return (SYN, mod.SYN_REPORT, 0)


def feed(translator, events):
    frames = []
    for event in events:
        frames.extend(translator.feed(*event))
    return frames


def values(frame, ev_type, code):
    return [v for t, c, v in frame if t == ev_type and c == code]


class ConfigTest(unittest.TestCase):
    def test_unset_or_blank_is_off(self):
        self.assertIsNone(mod.configured_pen({}))
        self.assertIsNone(mod.configured_pen({mod.CONFIG_KEY: ""}))
        self.assertIsNone(mod.configured_pen({mod.CONFIG_KEY: "  "}))

    def test_name_is_taken_verbatim(self):
        self.assertEqual(
            mod.configured_pen({mod.CONFIG_KEY: "NVTCapacitivePen"}),
            "NVTCapacitivePen",
        )

    def test_shell_env_parses_exports(self):
        def runner(argv, **_kwargs):
            self.assertEqual(argv[0], "bash")
            return subprocess.CompletedProcess(
                argv, 0, stdout="ARMADA_A=1\nARMADA_B=x=y\nnoise\n"
            )

        self.assertEqual(
            mod.shell_env("/nonexistent", runner=runner),
            {"ARMADA_A": "1", "ARMADA_B": "x=y"},
        )


class FindEventNodeTest(unittest.TestCase):
    def make_sysfs(self, devices):
        root = tempfile.mkdtemp()
        self.addCleanup(lambda: subprocess.run(["rm", "-rf", root], check=False))
        for entry, name in devices.items():
            os.makedirs(os.path.join(root, entry, "device"))
            with open(os.path.join(root, entry, "device", "name"), "w") as fh:
                fh.write(name + "\n")
        return root

    def test_exact_name_match(self):
        root = self.make_sysfs(
            {
                "event3": "NVTCapacitiveTouchScreen",
                "event4": "NVTCapacitivePen",
                "event7": "NVTCapacitivePen extra",
            }
        )
        self.assertEqual(
            mod.find_event_node("NVTCapacitivePen", sysfs=root, devdir="/dev/x"),
            "/dev/x/event4",
        )

    def test_missing_device_or_sysfs(self):
        root = self.make_sysfs({"event3": "NVTCapacitiveTouchScreen"})
        self.assertIsNone(mod.find_event_node("NVTCapacitivePen", sysfs=root))
        self.assertIsNone(
            mod.find_event_node("NVTCapacitivePen", sysfs=root + "/missing")
        )

    def test_ignores_non_event_entries(self):
        root = self.make_sysfs({"input4": "NVTCapacitivePen"})
        self.assertIsNone(mod.find_event_node("NVTCapacitivePen", sysfs=root))


class TranslatorTest(unittest.TestCase):
    def setUp(self):
        self.t = mod.PenTouchTranslator()

    def test_hover_produces_nothing(self):
        frames = feed(
            self.t,
            [(ABS, mod.ABS_X, 100), (ABS, mod.ABS_Y, 200), report()] * 3,
        )
        self.assertEqual(frames, [])

    def test_tap_is_one_down_and_one_up(self):
        frames = feed(
            self.t,
            [
                (ABS, mod.ABS_X, 100),
                (ABS, mod.ABS_Y, 200),
                (KEY, mod.BTN_TOUCH, 1),
                report(),
                (KEY, mod.BTN_TOUCH, 0),
                report(),
            ],
        )
        self.assertEqual(len(frames), 2)
        down, up = frames
        self.assertEqual(values(down, ABS, mod.ABS_MT_TRACKING_ID), [1])
        self.assertEqual(values(down, ABS, mod.ABS_MT_POSITION_X), [100])
        self.assertEqual(values(down, ABS, mod.ABS_MT_POSITION_Y), [200])
        self.assertEqual(values(down, KEY, mod.BTN_TOUCH), [1])
        self.assertEqual(down[0], (ABS, mod.ABS_MT_SLOT, 0))
        self.assertEqual(values(up, ABS, mod.ABS_MT_TRACKING_ID), [-1])
        self.assertEqual(values(up, KEY, mod.BTN_TOUCH), [0])

    def test_drag_moves_without_new_contact(self):
        frames = feed(
            self.t,
            [
                (KEY, mod.BTN_TOUCH, 1),
                (ABS, mod.ABS_X, 10),
                report(),
                (ABS, mod.ABS_X, 20),
                report(),
                (ABS, mod.ABS_Y, 30),
                report(),
            ],
        )
        self.assertEqual(len(frames), 3)
        for move in frames[1:]:
            self.assertEqual(values(move, ABS, mod.ABS_MT_TRACKING_ID), [])
            self.assertEqual(values(move, KEY, mod.BTN_TOUCH), [])
        self.assertEqual(values(frames[2], ABS, mod.ABS_MT_POSITION_X), [20])
        self.assertEqual(values(frames[2], ABS, mod.ABS_MT_POSITION_Y), [30])

    def test_new_contact_gets_new_tracking_id(self):
        tap = [(KEY, mod.BTN_TOUCH, 1), report(), (KEY, mod.BTN_TOUCH, 0), report()]
        frames = feed(self.t, tap + tap)
        ids = [
            v
            for frame in frames
            for v in values(frame, ABS, mod.ABS_MT_TRACKING_ID)
            if v >= 0
        ]
        self.assertEqual(ids, [1, 2])

    def test_tracking_id_wraps(self):
        self.t.tracking_id = mod.TRACKING_ID_MAX
        frames = feed(self.t, [(KEY, mod.BTN_TOUCH, 1), report()])
        self.assertEqual(values(frames[0], ABS, mod.ABS_MT_TRACKING_ID), [0])

    def test_pressure_tilt_and_buttons_are_dropped(self):
        frames = feed(
            self.t,
            [
                (KEY, mod.BTN_TOUCH, 1),
                (ABS, 0x18, 3000),  # ABS_PRESSURE
                (ABS, 0x1A, 40),  # ABS_TILT_X
                (KEY, 0x14B, 1),  # BTN_STYLUS
                report(),
            ],
        )
        codes = {(t, c) for frame in frames for t, c, _v in frame}
        self.assertNotIn((ABS, 0x18), codes)
        self.assertNotIn((ABS, 0x1A), codes)
        self.assertNotIn((KEY, 0x14B), codes)

    def test_release_lifts_only_a_held_touch(self):
        self.assertEqual(self.t.release(), [])
        feed(self.t, [(KEY, mod.BTN_TOUCH, 1), report()])
        frames = self.t.release()
        self.assertEqual(len(frames), 1)
        self.assertEqual(values(frames[0], ABS, mod.ABS_MT_TRACKING_ID), [-1])
        self.assertEqual(self.t.release(), [])

    def test_syn_dropped_discards_until_report_then_resyncs(self):
        feed(self.t, [(KEY, mod.BTN_TOUCH, 1), (ABS, mod.ABS_X, 5), report()])
        frames = feed(
            self.t,
            [
                (SYN, mod.SYN_DROPPED, 0),
                (KEY, mod.BTN_TOUCH, 0),
                (ABS, mod.ABS_X, 99),
                report(),
            ],
        )
        self.assertEqual(frames, [])
        self.assertTrue(self.t.needs_resync)
        self.assertEqual(self.t.x, 5)
        frames = self.t.resync(False, 77, 88)
        self.assertFalse(self.t.needs_resync)
        self.assertEqual(len(frames), 1)
        self.assertEqual(values(frames[0], ABS, mod.ABS_MT_TRACKING_ID), [-1])

    def test_resync_while_still_down_only_moves(self):
        feed(self.t, [(KEY, mod.BTN_TOUCH, 1), report()])
        feed(self.t, [(SYN, mod.SYN_DROPPED, 0), report()])
        frames = self.t.resync(True, 7, 8)
        self.assertEqual(len(frames), 1)
        self.assertEqual(values(frames[0], ABS, mod.ABS_MT_TRACKING_ID), [])
        self.assertEqual(values(frames[0], ABS, mod.ABS_MT_POSITION_X), [7])


class FakeTouch:
    def __init__(self):
        self.frames = []

    def send(self, frames):
        self.frames.extend(frames)


def pack(events):
    return b"".join(mod.EVENT.pack(0, 0, t, c, v) for t, c, v in events)


class ForwardTest(unittest.TestCase):
    def test_resync_happens_before_later_events_in_the_batch(self):
        translator = mod.PenTouchTranslator()
        touch = FakeTouch()
        original = mod.device_state
        mod.device_state = lambda _fd: (True, 40, 50)
        self.addCleanup(setattr, mod, "device_state", original)
        data = pack(
            [
                (SYN, mod.SYN_DROPPED, 0),
                report(),
                (ABS, mod.ABS_X, 41),
                report(),
            ]
        )
        mod.forward(data, translator, touch, fd=None)
        self.assertEqual(len(touch.frames), 2)
        self.assertEqual(values(touch.frames[0], ABS, mod.ABS_MT_TRACKING_ID), [1])
        self.assertEqual(values(touch.frames[0], ABS, mod.ABS_MT_POSITION_X), [40])
        self.assertEqual(values(touch.frames[1], ABS, mod.ABS_MT_POSITION_X), [41])
        self.assertEqual(values(touch.frames[1], ABS, mod.ABS_MT_POSITION_Y), [50])


class RunLoopTest(unittest.TestCase):
    def setUp(self):
        self.logs = []
        original = mod.log
        mod.log = self.logs.append
        self.addCleanup(setattr, mod, "log", original)

    def test_transient_errors_retry_and_log_once(self):
        calls = []
        outcomes = [
            OSError(mod.errno.EACCES, "Permission denied"),
            OSError(mod.errno.EACCES, "Permission denied"),
            OSError(mod.errno.ENODEV, "No such device"),
            mod.Stop(),
        ]

        def bridge(path):
            calls.append(path)
            raise outcomes.pop(0)

        sleeps = []
        result = mod.run(
            "Pen", bridge_fn=bridge, find=lambda _n: "/dev/x", sleep=sleeps.append
        )
        self.assertEqual(result, 0)
        self.assertEqual(len(calls), 4)
        self.assertEqual(len(sleeps), 3)
        self.assertEqual(sum("Permission denied" in m for m in self.logs), 1)
        self.assertEqual(sum("No such device" in m for m in self.logs), 1)

    def test_missing_device_waits(self):
        finds = [None, None, mod.Stop()]

        def find(_name):
            outcome = finds.pop(0)
            if isinstance(outcome, BaseException):
                raise outcome
            return outcome

        self.assertEqual(mod.run("Pen", find=find, sleep=lambda _s: None), 0)
        self.assertEqual(sum("waiting for" in m for m in self.logs), 1)

    def test_other_errors_are_not_swallowed(self):
        def bridge(_path):
            raise OSError(mod.errno.EINVAL, "Invalid argument")

        with self.assertRaises(OSError):
            mod.run("Pen", bridge_fn=bridge, find=lambda _n: "/dev/x", sleep=lambda _s: None)


class MainTest(unittest.TestCase):
    def setUp(self):
        original_log, original_run = mod.log, mod.run
        mod.log = lambda _m: None
        self.ran = []
        mod.run = lambda pen: self.ran.append(pen) or 0
        self.addCleanup(setattr, mod, "log", original_log)
        self.addCleanup(setattr, mod, "run", original_run)

    def test_supported_reflects_the_profile(self):
        self.assertEqual(mod.main(["--supported"], env={mod.CONFIG_KEY: "Pen"}), 0)
        self.assertEqual(mod.main(["--supported"], env={}), 1)
        self.assertEqual(self.ran, [])

    def test_unconfigured_device_does_not_run(self):
        self.assertEqual(mod.main([], env={}), 0)
        self.assertEqual(self.ran, [])

    def test_configured_device_runs_the_bridge(self):
        self.assertEqual(mod.main([], env={mod.CONFIG_KEY: "Pen"}), 0)
        self.assertEqual(self.ran, ["Pen"])

    def test_unknown_argument(self):
        self.assertEqual(mod.main(["--bogus"], env={}), 2)


class LayoutTest(unittest.TestCase):
    def test_ioctl_numbers(self):
        self.assertEqual(mod.EVIOCGRAB, 0x40044590)
        self.assertEqual(mod.eviocgabs(mod.ABS_X), 0x80184540)
        self.assertEqual(mod.EVIOCGKEY, 0x80604518)
        self.assertEqual(mod.UI_DEV_CREATE, 0x5501)
        self.assertEqual(mod.UI_DEV_DESTROY, 0x5502)
        self.assertEqual(mod.UI_DEV_SETUP, 0x405C5503)
        self.assertEqual(mod.UI_ABS_SETUP, 0x401C5504)
        self.assertEqual(mod.UI_SET_EVBIT, 0x40045564)
        self.assertEqual(mod.UI_SET_KEYBIT, 0x40045565)
        self.assertEqual(mod.UI_SET_ABSBIT, 0x40045567)
        self.assertEqual(mod.UI_SET_PROPBIT, 0x4004556E)

    def test_struct_sizes(self):
        self.assertEqual(mod.EVENT.size, 24)
        self.assertEqual(len(mod.uinput_dev_setup(mod.VIRTUAL_NAME)), 92)
        setup = mod.uinput_abs_setup(mod.ABS_X, (0, 0, 32000, 0, 0, 117))
        self.assertEqual(len(setup), 28)
        self.assertEqual(struct.unpack("HH6i", setup)[3:], (0, 32000, 0, 0, 117))

    def test_frame_encoding_ends_with_syn_report(self):
        data = mod.encode_frame([(KEY, mod.BTN_TOUCH, 1)])
        self.assertEqual(len(data), 2 * mod.EVENT.size)
        self.assertEqual(
            mod.EVENT.unpack_from(data, mod.EVENT.size)[2:],
            (SYN, mod.SYN_REPORT, 0),
        )


class UnitAndProfileTest(unittest.TestCase):
    def read(self, path):
        with open(path) as fh:
            return fh.read()

    def test_user_unit_is_bound_to_game_mode(self):
        unit = self.read(_SERVICE_PATH)
        self.assertIn("BindsTo=gamescope-session-plus@steam.service", unit)
        self.assertIn("PartOf=gamescope-session-plus@steam.service", unit)
        self.assertIn(
            "ExecCondition=/usr/bin/python3 /usr/libexec/armada/armada-pen-touch.py --supported",
            unit,
        )
        self.assertIn("ExecStart=/usr/bin/python3 /usr/libexec/armada/armada-pen-touch.py\n", unit)
        self.assertNotIn("[Install]", unit)
        self.assertIn("Wants=armada-pen-touch.service", self.read(_DROPIN_PATH))

    def test_only_tb710fu_enables_the_bridge(self):
        enabled = {}
        for name in sorted(os.listdir(_DEVICES)):
            text = self.read(os.path.join(_DEVICES, name))
            for line in text.splitlines():
                if line.startswith(mod.CONFIG_KEY + "="):
                    value = line.split("=", 1)[1].strip("'\"")
                    if value:
                        enabled[name] = value
        self.assertEqual(enabled, {"lenovo-tb710fu.conf": "NVTCapacitivePen"})
        self.assertIn(mod.CONFIG_KEY + "=\n", self.read(os.path.join(_DEVICES, "defaults.conf")))


if __name__ == "__main__":
    unittest.main()
