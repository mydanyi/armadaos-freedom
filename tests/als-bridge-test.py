#!/usr/bin/env python3
"""Exercise the bridge without a system bus, kernel module or hardware."""
import importlib.machinery
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "system_files/usr/libexec/armada/armada-als-bridge"
spec = importlib.util.spec_from_loader(
    "als_bridge", importlib.machinery.SourceFileLoader("als_bridge", str(HELPER))
)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class Sensor:
    def __init__(self):
        self.props = {"HasAmbientLight": True, "LightLevelUnit": "lux", "LightLevel": 42.5}
        self.claims = 0
        self.releases = 0
        self.error = None
        self.claim_error = None
        self.after_claim = None

    def properties(self):
        if self.error:
            raise RuntimeError(self.error)
        return dict(self.props)

    def claim(self):
        self.claims += 1
        if self.claim_error:
            raise RuntimeError(self.claim_error)
        if self.after_claim:
            self.props.update(self.after_claim)

    def release(self):
        self.releases += 1


class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.parameter = Path(self.tmp.name) / "lux"
        self.loads = 0
        self.sensor = Sensor()
        self.bridge = mod.LightBridge(self.sensor, self.parameter, self.load_module)

    def load_module(self):
        self.loads += 1
        self.parameter.touch()

    def test_delayed_sensor_does_not_create_phantom_als(self):
        self.sensor.props["HasAmbientLight"] = False
        self.bridge.refresh()
        self.assertFalse(self.parameter.exists())
        self.assertEqual(self.sensor.claims, 0)
        self.sensor.props["HasAmbientLight"] = True
        self.bridge.refresh()
        self.assertEqual(self.parameter.read_text(), "42\n")
        self.assertEqual(self.loads, 1)

    def test_initially_absent_proxy_is_retried(self):
        self.sensor.error = "name has no owner"
        with self.assertLogs(mod.LOG, level="WARNING"):
            self.assertTrue(self.bridge.refresh())
        self.assertFalse(self.parameter.exists())
        self.sensor.error = None
        self.bridge.refresh()
        self.assertEqual(self.sensor.claims, 1)
        self.assertEqual(self.parameter.read_text(), "42\n")

    def test_failed_claim_is_retried_before_publishing(self):
        self.sensor.claim_error = "sensor is opening"
        with self.assertLogs(mod.LOG, level="WARNING"):
            self.bridge.refresh()
        self.assertFalse(self.parameter.exists())
        self.sensor.claim_error = None
        self.bridge.refresh()
        self.assertEqual(self.sensor.claims, 2)
        self.assertTrue(self.parameter.exists())

    def test_unit_can_become_available_only_after_claim(self):
        self.sensor.props["LightLevelUnit"] = "vendor"
        self.sensor.after_claim = {"LightLevelUnit": "lux", "LightLevel": 123}
        self.bridge.refresh()
        self.assertEqual(self.parameter.read_text(), "123\n")

    def test_vendor_units_are_not_mistaken_for_lux(self):
        self.sensor.props["LightLevelUnit"] = "vendor"
        self.bridge.refresh()
        self.assertFalse(self.parameter.exists())
        self.assertEqual(self.loads, 0)

    def test_invalid_samples_do_not_replace_last_good_reading(self):
        self.bridge.refresh()
        for value in (float("nan"), float("inf"), -1, None, "invalid"):
            self.sensor.props["LightLevel"] = value
            self.bridge.refresh()
            self.assertEqual(self.parameter.read_text(), "42\n")
        self.sensor.props["LightLevel"] = 0
        self.bridge.refresh()
        self.assertEqual(self.parameter.read_text(), "0\n")

    def test_extreme_samples_cannot_wrap_iio_integer(self):
        self.sensor.props["LightLevel"] = 1e30
        self.bridge.refresh()
        self.assertEqual(self.parameter.read_text(), f"{2**31 - 1}\n")

    def test_refresh_recovers_missed_signal_without_reclaiming(self):
        self.bridge.refresh()
        self.sensor.props["LightLevel"] = 1234
        self.bridge.refresh()
        self.assertEqual(self.parameter.read_text(), "1234\n")
        self.assertEqual((self.loads, self.sensor.claims), (1, 1))

    def test_owner_replacement_renews_claim(self):
        self.bridge.refresh()
        self.bridge.owner_changed("")
        self.assertFalse(self.bridge.claimed)
        self.sensor.props["LightLevel"] = 18
        self.bridge.owner_changed(":1.123")
        self.assertEqual(self.sensor.claims, 2)
        self.assertEqual(self.parameter.read_text(), "18\n")
        self.bridge.close()
        self.assertEqual(self.sensor.releases, 1)

    def test_module_failure_is_retried(self):
        def failed_load():
            raise OSError("module unavailable")
        self.bridge.load_module = failed_load
        with self.assertLogs(mod.LOG, level="WARNING"):
            self.bridge.refresh()
        self.assertFalse(self.parameter.exists())
        self.bridge.load_module = self.load_module
        self.bridge.refresh()
        self.assertTrue(self.parameter.exists())


if __name__ == "__main__":
    unittest.main()
