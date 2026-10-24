#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mirror SensorProxy lux readings into the IIO interface Steam understands."""

import logging
import math
import subprocess
from pathlib import Path

LOG = logging.getLogger("armada-als-bridge")
SENSOR_NAME = "net.hadess.SensorProxy"
SENSOR_PATH = "/net/hadess/SensorProxy"
LUX_PARAM = Path("/sys/module/als_bridge/parameters/lux")
REFRESH_SECONDS = 5
CALL_TIMEOUT_MS = 5000


def lux_value(value):
    """Keep invalid samples out of sysfs and within IIO_VAL_INT's range."""
    try:
        value = float(value)
    except (TypeError, ValueError, OverflowError):
        return None
    if not math.isfinite(value) or value < 0:
        return None
    return int(round(min(value, 2**31 - 1)))


class LightBridge:
    def __init__(self, sensor, parameter=LUX_PARAM, load_module=None):
        self.sensor = sensor
        self.parameter = parameter
        self.load_module = load_module or self.modprobe
        self.claimed = False
        self.last_error = None

    @staticmethod
    def modprobe():
        subprocess.run(["modprobe", "als-bridge"], check=True, timeout=10)

    def refresh(self):
        try:
            props = self.sensor.properties()
            if not props.get("HasAmbientLight"):
                return True
            if not self.claimed:
                self.sensor.claim()
                self.claimed = True
                props = self.sensor.properties()
            # Units may become available only after ClaimLight starts polling.
            if not props.get("HasAmbientLight") or props.get("LightLevelUnit") != "lux":
                return True
            value = lux_value(props.get("LightLevel"))
            if value is None:
                return True
            # Do not create an ALS on machines without a real light source.
            if not self.parameter.exists():
                self.load_module()
            self.parameter.write_text(f"{value}\n")
            self.last_error = None
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            if str(error) != self.last_error:
                LOG.warning("Light sensor update unavailable: %s", error)
                self.last_error = str(error)
        return True

    def owner_changed(self, owner):
        self.claimed = False
        if owner:
            self.refresh()

    def close(self):
        if self.claimed:
            try:
                self.sensor.release()
            except RuntimeError:
                pass


def main():
    # Import here so the state machine can be tested without a system bus or GI.
    import gi

    gi.require_version("Gio", "2.0")
    from gi.repository import Gio, GLib

    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)

    class SensorProxy:
        def call(self, interface, method, parameters=None):
            try:
                return bus.call_sync(
                    SENSOR_NAME, SENSOR_PATH, interface, method, parameters,
                    None, Gio.DBusCallFlags.NONE, CALL_TIMEOUT_MS, None,
                )
            except GLib.Error as error:
                raise RuntimeError(str(error)) from error

        def properties(self):
            return self.call(
                "org.freedesktop.DBus.Properties", "GetAll",
                GLib.Variant("(s)", (SENSOR_NAME,)),
            ).unpack()[0]

        def claim(self):
            self.call(SENSOR_NAME, "ClaimLight")

        def release(self):
            self.call(SENSOR_NAME, "ReleaseLight")

    bridge = LightBridge(SensorProxy(), parameter=LUX_PARAM)

    def properties_changed(_bus, _sender, _path, _interface, _signal, params):
        interface, changed, invalidated = params.unpack()
        if interface == SENSOR_NAME and any(
            key in changed or key in invalidated
            for key in ("HasAmbientLight", "LightLevel", "LightLevelUnit")
        ):
            bridge.refresh()

    def owner_changed(_bus, _sender, _path, _interface, _signal, params):
        _name, _old, new = params.unpack()
        bridge.owner_changed(new)

    bus.signal_subscribe(
        SENSOR_NAME, "org.freedesktop.DBus.Properties", "PropertiesChanged",
        SENSOR_PATH, SENSOR_NAME, Gio.DBusSignalFlags.NONE, properties_changed,
    )
    bus.signal_subscribe(
        "org.freedesktop.DBus", "org.freedesktop.DBus", "NameOwnerChanged",
        "/org/freedesktop/DBus", SENSOR_NAME,
        Gio.DBusSignalFlags.NONE, owner_changed,
    )
    bridge.refresh()
    GLib.timeout_add_seconds(REFRESH_SECONDS, bridge.refresh)
    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.close()


if __name__ == "__main__":
    main()
