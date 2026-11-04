# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2026 danyi
"""Black-box tests for tb323fu-bluetooth-prepare.

Stubs only mount/umount/udevadm OS side effects via sys.executable Python
helpers. Executes the real helper, real file copies, fixtures under a
TemporaryDirectory. Tests run on Linux; the real helper is invoked with bash
so the executable bit is not required for the fixture tree."""
import os
import pathlib
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest

REPO = pathlib.Path(__file__).resolve().parent.parent
HELPER = REPO / "system_files/usr/libexec/armada/tb323fu-bluetooth-prepare"
UNIT = REPO / "system_files/usr/lib/systemd/system/tb323fu-bluetooth-firmware.service"
BUILD = REPO / "build_files/40-vendor-system-files.sh"

MOUNT_STUB = textwrap.dedent(
    """\
    import os, shutil, sys
    log = os.environ["STUB_LOG"]
    args = sys.argv[1:]
    pos = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "-t":
            i += 2; continue
        if a == "-o":
            i += 2; continue
        if a.startswith("-"):
            i += 1; continue
        pos.append(a); i += 1
    if len(pos) < 2:
        with open(log, "a") as f:
            f.write("mount BADARGS " + " ".join(args) + "\\n")
        sys.exit(1)
    src, dst = pos[0], pos[1]
    extra = " ".join(args)
    with open(log, "a") as f:
        f.write("mount " + extra + "\\n")
    if not os.path.exists(src):
        sys.exit(1)
    os.makedirs(dst, exist_ok=True)
    for name in os.listdir(src):
        s = os.path.join(src, name)
        d = os.path.join(dst, name)
        if os.path.isdir(s):
            shutil.copytree(s, d)
        else:
            shutil.copy2(s, d)
    """
)

UMOUNT_STUB = textwrap.dedent(
    """\
    import os, shutil, sys
    log = os.environ["STUB_LOG"]
    target = sys.argv[1] if len(sys.argv) > 1 else ""
    with open(log, "a") as f:
        f.write("umount " + target + "\\n")
    if os.environ.get("STUB_UMOUNT_FAIL"):
        sys.exit(1)
    if target and os.path.isdir(target):
        shutil.rmtree(target)
    """
)

UDEV_STUB = "import sys\nsys.exit(0)\n"



def _make_exe(path: pathlib.Path, body: str) -> None:
    path.write_text(body)
    path.chmod(0o755)


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix="btprep-"))
        self.addCleanup(shutil.rmtree, self.tmp, True)
        self.bin = self.tmp / "bin"
        self.bin.mkdir()
        self.log = self.tmp / "stub.log"
        self.log.write_text("")
        _make_exe(self.bin / "mount", f"#!{sys.executable}\n{MOUNT_STUB}")
        _make_exe(self.bin / "umount", f"#!{sys.executable}\n{UMOUNT_STUB}")
        _make_exe(self.bin / "udevadm", f"#!{sys.executable}\n{UDEV_STUB}")

        self.dt = self.tmp / "compatible"
        self.dt.write_bytes(b"lenovo,baldur\0qcom,foo\0")
        self.part = self.tmp / "partlabel"
        self.part.mkdir()
        self.fwparam = self.tmp / "fwparam"
        self.fwparam.write_text("")
        self.runfw = self.tmp / "runfw"
        self.runfw.mkdir()
        self.work = self.tmp / "work"
        self.work.mkdir()
        self._make_partition(self.part / "bluetooth_a", {
            b"brhbtfw20.mbn": b"FW20-A",
            b"brhbtnv20.bin": b"NV20-A",
        })

    def _make_partition(self, path: pathlib.Path, files):
        path.mkdir()
        img = path / "image"
        img.mkdir()
        for name, data in files.items():
            (img / name.decode()).write_bytes(data)

    def env(self, overrides=None):
        e = dict(os.environ)
        e["PATH"] = f"{self.bin}:{e.get('PATH','')}"
        e["STUB_LOG"] = str(self.log)
        e["TB323FU_DT_COMPATIBLE"] = str(self.dt)
        e["TB323FU_BT_PARTLABEL_DIR"] = str(self.part)
        e["TB323FU_FIRMWARE_CLASS_PATH"] = str(self.fwparam)
        e["TB323FU_RUN_FIRMWARE"] = str(self.runfw)
        e["TB323FU_BT_WORK_DIR"] = str(self.work / "prep")
        if overrides:
            e.update(overrides)
        return e

    def run_helper(self, overrides=None):
        if not HELPER.is_file():
            self.fail(f"helper missing: {HELPER}")
        return subprocess.run(
            ["bash", str(HELPER)],
            env=self.env(overrides),
            capture_output=True, text=True,
        )

    def qca(self):
        return self.runfw / "qca"

    def log_lines(self):
        return [l for l in self.log.read_text().splitlines() if l]

    def _install_cp_fail(self):
        _make_exe(self.bin / "cp", "#!/bin/bash\nexit 1\n")

    # ---------------- tests ----------------

    def test_non_target_no_mutations(self):
        self.dt.write_bytes(b"other,board\0")
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.qca().exists())
        self.assertEqual(self.log_lines(), [])

    def test_near_token_not_matched(self):
        self.dt.write_bytes(b"lenovo,baldur-extra\0lenovo,other\0")
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.qca().exists())
        self.assertEqual(self.log_lines(), [])

    def test_target_copies_exact_bytes_and_unmounts(self):
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.qca() / "brhbtfw20.mbn").read_bytes(), b"FW20-A")
        self.assertEqual((self.qca() / "brhbtnv20.bin").read_bytes(), b"NV20-A")
        self.assertEqual(stat.S_IMODE((self.qca() / "brhbtfw20.mbn").stat().st_mode), 0o644)
        self.assertEqual(stat.S_IMODE(self.qca().stat().st_mode), 0o755)
        lines = self.log_lines()
        mounts = [l for l in lines if l.startswith("mount ")]
        umounts = [l for l in lines if l.startswith("umount ")]
        self.assertEqual(len(mounts), 1, lines)
        self.assertEqual(len(umounts), 1, lines)
        self.assertIn("-t vfat -o ro,nosuid,nodev,noexec", mounts[0])

    def test_preserves_unrelated_files(self):
        self.qca().mkdir()
        (self.qca() / "keep.txt").write_text("keep")
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.qca() / "keep.txt").read_text(), "keep")

    def test_custom_path_with_spaces(self):
        custom = self.tmp / "fw with space"
        custom.mkdir()
        self.fwparam.write_text(str(custom))
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((custom / "qca/brhbtfw20.mbn").read_bytes(), b"FW20-A")
        self.assertEqual(self.fwparam.read_text(), str(custom))

    def test_empty_param_fallback_writes(self):
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.runfw / "qca/brhbtfw20.mbn").read_bytes(), b"FW20-A")
        self.assertEqual(self.fwparam.read_text(), str(self.runfw))

    def test_relative_param_rejected(self):
        self.fwparam.write_text("relative/path")
        r = self.run_helper()
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse((self.tmp / "relative").exists())
        self.assertEqual(self.log_lines(), [])

    def test_relative_fallback_rejected(self):
        r = self.run_helper({"TB323FU_RUN_FIRMWARE": "relfw"})
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse((self.tmp / "relfw").exists())
        self.assertEqual(self.log_lines(), [])

    def test_firmware_param_write_failure_nonzero(self):
        # Param path is a directory: cat fails, fallback applies, final write fails.
        param_dir = self.tmp / "paramdir"
        param_dir.mkdir()
        r = self.run_helper({"TB323FU_FIRMWARE_CLASS_PATH": str(param_dir)})
        self.assertNotEqual(r.returncode, 0)

    def test_a_unavailable_falls_back_to_b(self):
        shutil.rmtree(self.part / "bluetooth_a")
        self._make_partition(self.part / "bluetooth_b", {
            b"brhbtfw20.mbn": b"FW20-B",
            b"brhbtnv20.bin": b"NV20-B",
        })
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.qca() / "brhbtfw20.mbn").read_bytes(), b"FW20-B")
        self.assertEqual((self.qca() / "brhbtnv20.bin").read_bytes(), b"NV20-B")

    def test_a_incomplete_falls_back_no_mixing(self):
        img = self.part / "bluetooth_a" / "image"
        (img / "brhbtnv20.bin").unlink()
        self._make_partition(self.part / "bluetooth_b", {
            b"brhbtfw20.mbn": b"FW20-B",
            b"brhbtnv20.bin": b"NV20-B",
        })
        r = self.run_helper()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual((self.qca() / "brhbtfw20.mbn").read_bytes(), b"FW20-B")
        self.assertEqual((self.qca() / "brhbtnv20.bin").read_bytes(), b"NV20-B")
        # ensure A unmounted cleanly before B mount
        umounts = [l for l in self.log_lines() if l.startswith("umount ")]
        self.assertGreaterEqual(len(umounts), 2)

    def test_neither_complete_returns_failure(self):
        img = self.part / "bluetooth_a" / "image"
        (img / "brhbtfw20.mbn").unlink()
        r = self.run_helper()
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(self.qca().exists())
        self.assertEqual(self.fwparam.read_text(), "")

    def test_idempotent_rerun(self):
        r1 = self.run_helper()
        self.assertEqual(r1.returncode, 0, r1.stderr)
        r2 = self.run_helper()
        self.assertEqual(r2.returncode, 0, r2.stderr)
        self.assertEqual((self.qca() / "brhbtfw20.mbn").read_bytes(), b"FW20-A")

    def test_cp_failure_aborts_with_clean_unmount(self):
        # cp fails: helper must abort, unmount once, and remove the workdir.
        self.qca().mkdir()
        dest_a = self.qca() / "brhbtfw20.mbn"
        dest_b = self.qca() / "brhbtnv20.bin"
        dest_k = self.qca() / "keep.txt"
        dest_a.write_bytes(b"ORIG-A")
        dest_b.write_bytes(b"ORIG-B")
        dest_k.write_bytes(b"ORIG-K")
        self._install_cp_fail()
        r = self.run_helper()
        self.assertNotEqual(r.returncode, 0)
        self.assertEqual(dest_a.read_bytes(), b"ORIG-A")
        self.assertEqual(dest_b.read_bytes(), b"ORIG-B")
        self.assertEqual(dest_k.read_bytes(), b"ORIG-K")
        umounts = [l for l in self.log_lines() if l.startswith("umount ")]
        self.assertEqual(len(umounts), 1)
        leftovers = [p for p in self.work.iterdir() if p.name.startswith("prep.")]
        self.assertEqual(leftovers, [])

    def test_umount_failure_preserves_mount_and_skips_b(self):
        self._make_partition(self.part / "bluetooth_b", {
            b"brhbtfw20.mbn": b"FW20-B",
            b"brhbtnv20.bin": b"NV20-B",
        })
        r = self.run_helper({"STUB_UMOUNT_FAIL": "1"})
        self.assertNotEqual(r.returncode, 0)
        self.assertFalse(self.qca().exists())
        # only A attempted (B not mounted)
        mounts = [l for l in self.log_lines() if l.startswith("mount ")]
        self.assertEqual(len(mounts), 1, self.log_lines())
        # workdir preserved (mount not cleared because umount failed and trap fails too)
        leaves = [p for p in self.work.iterdir() if p.name.startswith("prep.")]
        self.assertEqual(len(leaves), 1, leaves)
        # mountpoint still contains the copied fixture tree (mount stub copy)
        mnt = leaves[0] / "mnt"
        self.assertTrue((mnt / "image" / "brhbtfw20.mbn").exists())


class IntegrationTests(unittest.TestCase):
    def test_helper_present(self):
        self.assertTrue(HELPER.is_file(), f"missing {HELPER}")

    def test_unit_order(self):
        self.assertTrue(UNIT.is_file(), f"missing {UNIT}")
        text = UNIT.read_text()
        self.assertIn("ExecStart=/usr/libexec/armada/tb323fu-bluetooth-prepare", text)
        self.assertIn("After=local-fs.target systemd-udev-trigger.service", text)
        self.assertIn("Requires=tb323fu-btaddr.service", text)
        self.assertIn("Before=tb323fu-btaddr.service bluetooth.service", text)
        self.assertNotIn("After=tb323fu-btaddr", text)
        self.assertIn("WantedBy=bluetooth.target", text)
        self.assertIn("RequiredBy=bluetooth.service", text)

    def test_build_enables_service(self):
        self.assertTrue(BUILD.is_file(), f"missing {BUILD}")
        text = BUILD.read_text()
        self.assertIn("systemctl enable tb323fu-bluetooth-firmware.service", text)


if __name__ == "__main__":
    unittest.main()
