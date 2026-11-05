import importlib.util
import importlib.machinery
import os
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "system_files/usr/lib/armada"
PLUGIN = ROOT / "decky/freedom-control/decky/armada-control/py_modules"

sys.path.insert(0, str(LIB))
sys.path.insert(0, str(PLUGIN))

import armada_perf  # noqa: E402
import armada_control.system as system  # noqa: E402


def load_powerd():
    from importlib.machinery import SourceFileLoader

    path = ROOT / "system_files/usr/libexec/armada/armada-powerd"
    loader = SourceFileLoader("armada_powerd_testmod", str(path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


powerd = load_powerd()


def make_powerd():
    obj = powerd.ArmadaPower.__new__(powerd.ArmadaPower)
    obj.scx_child = None
    obj.scx_spec = None
    obj.scx_failed = None
    obj.scx_warned = set()
    return obj


class KernelMarkerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        for mod in (armada_perf, system):
            patcher = mock.patch.object(mod, "MODULES_ROOT", self.root)
            patcher.start()
            self.addCleanup(patcher.stop)
        self.release = "7.2.6-freedom-infinity"
        patcher = mock.patch.object(os, "uname", return_value=SimpleNamespace(release=self.release))
        patcher.start()
        self.addCleanup(patcher.stop)
        self.marker = self.root / self.release / "freedom-scheduler"
        self.marker.parent.mkdir(parents=True, exist_ok=True)

    def write(self, content, encoding="utf-8"):
        if isinstance(content, bytes):
            self.marker.write_bytes(content)
        else:
            self.marker.write_text(content, encoding=encoding)

    def assert_both(self, expected):
        self.assertEqual(armada_perf.kernel_default_scheduler(), expected)
        self.assertEqual(system.kernel_default_scheduler(), expected)

    def test_missing_marker(self):
        self.assert_both("eevdf")

    def test_exact_infinity_v1_newline(self):
        self.write("infinity-v1\n")
        self.assert_both("infinity")

    def test_malformed(self):
        self.write("infinity-v2\n")
        self.assert_both("eevdf")

    def test_other_release_ignored(self):
        other = self.root / "6.99.0-freedom-infinity"
        other.mkdir(parents=True)
        (other / "freedom-scheduler").write_text("infinity-v1\n")
        self.assert_both("eevdf")

    def test_unreadable(self):
        self.marker.mkdir()
        self.assert_both("eevdf")

    def test_invalid_utf8(self):
        self.write(b"\xff\xfeinfinity-v1")
        self.assert_both("eevdf")

    def test_read_text_error_isolated(self):
        self.write("infinity-v1\n")
        with mock.patch.object(Path, "read_text", side_effect=OSError("boom")):
            self.assert_both("eevdf")
        self.assert_both("infinity")


class SanitizeTests(unittest.TestCase):
    def test_preserves_accepted(self):
        self.assertEqual(armada_perf.sanitize_perf({"scheduler": "lavd"}), {"scheduler": "lavd"})
        self.assertEqual(armada_perf.sanitize_perf({"scheduler": "cosmos"}), {"scheduler": "cosmos"})

    def test_maps_to_kernel_default(self):
        with mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            self.assertEqual(armada_perf.sanitize_perf({"scheduler": "eevdf"}), {"scheduler": "infinity"})
            self.assertEqual(armada_perf.sanitize_perf({"scheduler": "infinity"}), {"scheduler": "infinity"})

    def test_invalid_dropped(self):
        self.assertEqual(armada_perf.sanitize_perf({"scheduler": "bogus"}), {})


class EffectiveStateTests(unittest.TestCase):
    def test_default(self):
        with mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            values = armada_perf.effective_state({})
        self.assertEqual(values["scheduler"], "infinity")
        self.assertIsNone(values["schedulerDomain"])

    def test_global_then_override(self):
        with mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            state = {"global": {"scheduler": "lavd", "schedulerDomain": [1, 2]}, "override": {"scheduler": "cosmos", "schedulerDomain": [3]}}
            values = armada_perf.effective_state(state)
        self.assertEqual(values["scheduler"], "cosmos")
        self.assertEqual(values["schedulerDomain"], [3])

    def test_explicit_eevdf_clears_lavd(self):
        with mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            state = {"global": {"scheduler": "lavd"}, "override": {"scheduler": "eevdf"}}
            values = armada_perf.effective_state(state)
        self.assertEqual(values["scheduler"], "infinity")

    def test_rollback_no_mutation(self):
        import copy
        state = {"global": {"scheduler": "infinity"}}
        snapshot = copy.deepcopy(state)
        with mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="eevdf"):
            values = armada_perf.effective_state(state)
        self.assertEqual(values["scheduler"], "eevdf")
        self.assertEqual(state, snapshot)


class PowerdEnforceTests(unittest.TestCase):
    def test_existing_lavd_terminated_selecting_infinity(self):
        obj = make_powerd()
        child = mock.Mock()
        child.poll.return_value = None
        child.returncode = None
        obj.scx_child = child
        obj.scx_spec = ("lavd", "")
        with mock.patch.object(powerd.subprocess, "Popen") as popen, \
             mock.patch.object(powerd.os.path, "exists", return_value=True), \
             mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            obj.enforce_scheduler({"scheduler": "infinity"})
        child.terminate.assert_called_once()
        child.wait.assert_called_once()
        popen.assert_not_called()
        self.assertIsNone(obj.scx_child)
        self.assertIsNone(obj.scx_spec)

    def test_no_fake_scx_infinity_spawned(self):
        obj = make_powerd()
        with mock.patch.object(powerd.subprocess, "Popen") as popen, \
             mock.patch.object(powerd.os.path, "exists", return_value=True), \
             mock.patch.object(armada_perf, "kernel_default_scheduler", return_value="infinity"):
            obj.enforce_scheduler({"scheduler": "infinity"})
        popen.assert_not_called()
        self.assertIsNone(obj.scx_child)

    def test_lavd_idempotent(self):
        obj = make_powerd()
        child = mock.Mock()
        child.poll.return_value = None
        child.returncode = None
        obj.scx_child = child
        obj.scx_spec = ("lavd", "")
        with mock.patch.object(powerd.subprocess, "Popen") as popen:
            obj.enforce_scheduler({"scheduler": "lavd"})
        popen.assert_not_called()
        child.terminate.assert_not_called()

    def test_lavd_start_from_stopped(self):
        obj = make_powerd()
        child = mock.Mock()
        child.poll.return_value = None
        child.returncode = None
        with mock.patch.object(powerd.subprocess, "Popen", return_value=child) as popen, \
             mock.patch.object(powerd.os.path, "exists", return_value=True):
            obj.enforce_scheduler({"scheduler": "lavd"})
            obj.enforce_scheduler({"scheduler": "lavd"})
        self.assertEqual(popen.call_count, 1)
        argv = popen.call_args[0][0]
        self.assertEqual(argv, ["/usr/bin/scx_lavd", "--pinned-slice-us", "500", "--dd-max-wait-us", "0"])

    def test_cosmos_domain_argv_unchanged(self):
        obj = make_powerd()
        child = mock.Mock()
        child.poll.return_value = None
        child.returncode = None
        with mock.patch.object(powerd.subprocess, "Popen", return_value=child) as popen, \
             mock.patch.object(powerd.os.path, "exists", return_value=True):
            obj.enforce_scheduler({"scheduler": "cosmos", "schedulerDomain": [3, 4, 5, 7]})
        popen.assert_called_once()
        argv = popen.call_args[0][0]
        self.assertEqual(argv, ["/usr/bin/scx_cosmos", "--primary-domain", "3-5,7"])
        self.assertEqual(obj.scx_spec, ("cosmos", "3-5,7"))


class PerfInfoTests(unittest.TestCase):
    def test_infinity_with_lavd_binary_and_schedutil(self):
        with mock.patch.object(system, "read_text", return_value="userspace schedutil"), \
             mock.patch.object(system, "device_env", return_value={}), \
             mock.patch.object(system, "kernel_default_scheduler", return_value="infinity"), \
             mock.patch.object(Path, "exists", autospec=True, side_effect=lambda path: str(path) == "/usr/bin/scx_lavd"):
            info = system.perf_info()
        self.assertEqual(info["schedulers"], ["infinity", "lavd"])
        self.assertIn("schedutil", info["governors"])

    def test_no_sched_ext_present(self):
        import armada_control.system as system

        def fake_read_text(path):
            return "schedutil performance"

        with mock.patch.object(system, "read_text", side_effect=fake_read_text), \
             mock.patch.object(system, "device_env", return_value={}), \
             mock.patch.object(system, "kernel_default_scheduler", return_value="infinity"), \
             mock.patch.object(Path, "exists", autospec=True, return_value=False):
            info = system.perf_info()
        self.assertEqual(info["schedulers"], ["infinity"])
        self.assertIn("schedutil", info["governors"])


if __name__ == "__main__":
    unittest.main()
