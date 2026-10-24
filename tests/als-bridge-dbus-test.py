#!/usr/bin/env python3
"""Run the actual Gio callbacks against a SensorProxy on a private D-Bus."""
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

try:
    import gi
    gi.require_version("Gio", "2.0")
    from gi.repository import Gio, GLib
except ImportError:
    Gio = GLib = None

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "system_files/usr/libexec/armada/armada-als-bridge"
NAME = "net.hadess.SensorProxy"
OBJECT = "/net/hadess/SensorProxy"
XML = f'''<node><interface name="{NAME}">
<method name="ClaimLight"/><method name="ReleaseLight"/>
<property name="HasAmbientLight" type="b" access="read"/>
<property name="LightLevel" type="d" access="read"/>
<property name="LightLevelUnit" type="s" access="read"/>
</interface></node>'''
CHILD = '''import importlib.machinery, importlib.util, sys
from pathlib import Path
spec = importlib.util.spec_from_loader("bridge", importlib.machinery.SourceFileLoader("bridge", sys.argv[1]))
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
mod.LUX_PARAM = Path(sys.argv[2])
mod.REFRESH_SECONDS = 1
mod.LightBridge.modprobe = staticmethod(lambda: mod.LUX_PARAM.touch())
mod.main()
'''


def stop(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    for stream in (process.stdout, process.stderr):
        if stream:
            stream.close()


@unittest.skipUnless(Gio and shutil.which("dbus-daemon"), "requires PyGObject and dbus-daemon")
class DBusTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.parameter = Path(self.tmp.name) / "lux"
        self.present = False
        self.level = 37.0
        self.unit = "vendor"
        self.claims = 0
        self.reads = 0
        self.connections = []
        self.daemon = subprocess.Popen(
            ["dbus-daemon", "--session", "--nofork", "--print-address"],
            stdout=subprocess.PIPE, text=True,
        )
        self.addCleanup(stop, self.daemon)
        self.address = self.daemon.stdout.readline().strip()
        self.loop = GLib.MainLoop()
        self.thread = threading.Thread(target=self.loop.run, daemon=True)
        self.thread.start()
        self.addCleanup(self.stop_loop)
        self.addCleanup(self.close_connections)
        self.connection = self.start_sensor()
        self.child = subprocess.Popen(
            [sys.executable, "-c", CHILD, str(HELPER), str(self.parameter)],
            env={**os.environ, "DBUS_SYSTEM_BUS_ADDRESS": self.address},
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
        )
        self.addCleanup(stop, self.child)
        self.wait_for(lambda: self.reads > 0)

    def stop_loop(self):
        self.loop.quit()
        self.thread.join(timeout=5)

    def close_connections(self):
        for connection in self.connections:
            connection.close_sync(None)

    def bus_call(self, connection, method, parameters):
        return connection.call_sync(
            "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
            method, parameters, None, Gio.DBusCallFlags.NONE, 3000, None,
        )

    def start_sensor(self):
        connection = Gio.DBusConnection.new_for_address_sync(
            self.address,
            Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
            None, None,
        )
        self.connections.append(connection)
        interface = Gio.DBusNodeInfo.new_for_xml(XML).interfaces[0]
        connection.register_object(OBJECT, interface, self.method, self.property, None)
        self.bus_call(connection, "RequestName", GLib.Variant("(su)", (NAME, 0)))
        return connection

    def method(self, _connection, _sender, _path, _interface, method, _parameters, invocation):
        if method == "ClaimLight":
            self.claims += 1
            # The first reading's unit need not be available before claiming.
            self.unit = "lux"
        invocation.return_value(None)

    def property(self, _connection, _sender, _path, _interface, name):
        self.reads += 1
        return self.values()[name]

    def values(self):
        return {
            "HasAmbientLight": GLib.Variant("b", self.present),
            "LightLevel": GLib.Variant("d", self.level),
            "LightLevelUnit": GLib.Variant("s", self.unit),
        }

    def emit(self, changed=(), invalidated=()):
        self.connection.emit_signal(
            None, OBJECT, "org.freedesktop.DBus.Properties", "PropertiesChanged",
            GLib.Variant("(sa{sv}as)", (NAME, {key: self.values()[key] for key in changed}, list(invalidated))),
        )
        self.connection.flush_sync(None)

    def wait_for(self, condition):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if condition():
                return
            if hasattr(self, "child") and self.child.poll() is not None:
                self.fail(self.child.stderr.read())
            time.sleep(0.02)
        self.fail("bridge did not update before deadline")

    def reading_is(self, value):
        return self.parameter.exists() and self.parameter.read_text() == f"{value}\n"

    def enable_sensor(self):
        self.assertFalse(self.parameter.exists())
        self.present = True
        self.emit(changed=("HasAmbientLight",))
        self.wait_for(lambda: self.reading_is(37))

    def test_late_sensor_and_property_updates(self):
        self.enable_sensor()
        self.assertEqual(self.claims, 1)
        self.level = 210
        self.emit(changed=("LightLevel",))
        self.wait_for(lambda: self.reading_is(210))
        self.level = 411
        self.emit(invalidated=("LightLevel",))
        self.wait_for(lambda: self.reading_is(411))

    def test_proxy_owner_replacement_renews_claim(self):
        self.enable_sensor()
        old = self.connection.get_unique_name()
        self.bus_call(self.connection, "ReleaseName", GLib.Variant("(s)", (NAME,)))
        self.level = 81
        self.connection = self.start_sensor()
        self.assertNotEqual(old, self.connection.get_unique_name())
        self.wait_for(lambda: self.claims == 2 and self.reading_is(81))

    def test_periodic_refresh_recovers_missed_signal(self):
        self.enable_sensor()
        self.level = 921
        self.wait_for(lambda: self.reading_is(921))
        self.assertEqual(self.claims, 1)


if __name__ == "__main__":
    unittest.main()
