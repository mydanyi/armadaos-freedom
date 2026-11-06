#!/usr/bin/env python3
"""Unit tests for build_files/edl_boot_requirements.py.

External boundary mocked: only the ``lsinitrd`` subprocess call.
"""

from __future__ import annotations

import hashlib
import json
import lzma
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "build_files"
if str(BUILD_DIR) not in sys.path:
    sys.path.insert(0, str(BUILD_DIR))

import edl_boot_requirements as ebr  # noqa: E402


PROFILE_BASE = {
    "displayName": "Sample",
    "compatible": {"soc": "soc0", "device": "dev0"},
    "dtb": "dtb/sample.dtb",
    "required": [],
    "forbidden": [],
    "extraArgs": [],
    "runtimeConfig": "runtime.conf",
    "runtimeCmakeArgs": [],
    "bootPartitionBytes": 4096,
    "diagnosticMode": "verbose",
}


A_SHA = "a" * 64
B_SHA = "b" * 64


def make_profile(
    early_firmware=None,
    early_drivers=None,
    sha=None,
    display_name="Sample",
):
    profile = dict(PROFILE_BASE)
    profile["displayName"] = display_name
    profile["bootRequirements"] = {
        "earlyFirmware": list(early_firmware or []),
        "earlyDrivers": list(early_drivers or []),
        "firmwareSha256": dict(sha or {}),
    }
    return profile


def make_config(models):
    return {
        "schemaVersion": 1,
        "runtimeSource": "runtime/source.conf",
        "profiles": {model: profile for model, profile in models.items()},
    }


def write_config(path, models):
    document = make_config(models)
    path.write_text(json.dumps(document), encoding="utf-8")
    return document


def write_file(path, payload=b"data"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


def write_module(root, kver, module_name, suffix=".ko", payload=b"\x7fELF"):
    base = root / "usr" / "lib" / "modules" / kver / "kernel" / "drivers"
    return write_file(base / f"{module_name}{suffix}", payload)


def write_builtin(root, kver, entries):
    path = root / "usr" / "lib" / "modules" / kver / "modules.builtin"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(entries) + "\n", encoding="utf-8")
    return path


def write_builtin_modinfo(root, kver, payload):
    path = root / "usr" / "lib" / "modules" / kver / "modules.builtin.modinfo"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


def firmware_path(root, relative):
    return root / "usr" / "lib" / "firmware" / Path(relative)


class LoadRequirementsTests(unittest.TestCase):
    def setUp(self):
        self.config = make_config(
            {
                "A": make_profile(
                    early_firmware=["a/fw1.bin", "a/fw2.bin"],
                    early_drivers=["drv_a", "drv_common"],
                    sha={"a/fw1.bin": A_SHA},
                ),
                "B": make_profile(
                    early_firmware=["b/fw1.bin"],
                    early_drivers=["drv_b"],
                ),
            }
        )

    def test_default_selects_all_in_order(self):
        result = ebr.load_requirements(self.config)
        self.assertEqual(list(result), ["A", "B"])
        self.assertEqual(result["A"]["earlyFirmware"], ["a/fw1.bin", "a/fw2.bin"])
        self.assertEqual(result["A"]["earlyDrivers"], ["drv_a", "drv_common"])
        self.assertEqual(result["A"]["firmwareSha256"], {"a/fw1.bin": A_SHA})

    def test_selected_subset(self):
        result = ebr.load_requirements(self.config, ["B"])
        self.assertEqual(list(result), ["B"])

    def test_subset_does_not_need_other_model(self):
        config = make_config(
            {
                "A": make_profile(early_firmware=["a/fw.bin"]),
                "B": {
                    "displayName": "B",
                    "compatible": {"soc": "s", "device": "d"},
                    "dtb": "x.dtb",
                    "required": [],
                    "forbidden": [],
                    "extraArgs": [],
                    "runtimeConfig": "r.conf",
                    "runtimeCmakeArgs": [],
                    "bootPartitionBytes": 1,
                    "diagnosticMode": "verbose",
                },
            }
        )
        result = ebr.load_requirements(config, ["A"])
        self.assertEqual(list(result), ["A"])

    def test_empty_lists_and_sha_allowed(self):
        config = make_config({"A": make_profile()})
        result = ebr.load_requirements(config)
        self.assertEqual(result["A"], {
            "earlyFirmware": [],
            "earlyDrivers": [],
            "firmwareSha256": {},
        })

    def test_missing_boot_requirements_rejected(self):
        config = make_config({"A": make_profile()})
        del config["profiles"]["A"]["bootRequirements"]
        with self.assertRaises(ValueError):
            ebr.load_requirements(config)

    def test_missing_field_rejected(self):
        for field in ("earlyFirmware", "earlyDrivers", "firmwareSha256"):
            config = make_config({"A": make_profile()})
            del config["profiles"]["A"]["bootRequirements"][field]
            with self.assertRaises(ValueError):
                ebr.load_requirements(config)

    def test_unknown_model_rejected(self):
        with self.assertRaises(ValueError):
            ebr.load_requirements(self.config, ["Nope"])

    def test_duplicate_model_rejected(self):
        with self.assertRaises(ValueError):
            ebr.load_requirements(self.config, ["A", "A"])

    def test_empty_selection_rejected(self):
        with self.assertRaises(ValueError):
            ebr.load_requirements(self.config, [])

    def test_unsafe_firmware_path_rejected(self):
        for bad in ("../x", "/abs/x", "a/../b", "a//b", "a/ b", "a/\x01b"):
            config = make_config({"A": make_profile(early_firmware=[bad])})
            with self.assertRaises(ValueError, msg=bad):
                ebr.load_requirements(config)

    def test_invalid_driver_rejected(self):
        for bad in ("has space", "bad/name", "bad\\name", ""):
            config = make_config({"A": make_profile(early_drivers=[bad])})
            with self.assertRaises(ValueError, msg=bad):
                ebr.load_requirements(config)

    def test_invalid_sha_rejected(self):
        for bad in ("A" * 64, "a" * 63, "g" * 64):
            config = make_config(
                {"A": make_profile(early_firmware=["x.bin"], sha={"x.bin": bad})}
            )
            with self.assertRaises(ValueError):
                ebr.load_requirements(config)

    def test_sha_for_unlisted_firmware_rejected(self):
        config = make_config(
            {"A": make_profile(early_firmware=["x.bin"], sha={"y.bin": A_SHA})}
        )
        with self.assertRaises(ValueError):
            ebr.load_requirements(config)


class ResolveFirmwareTests(unittest.TestCase):
    def test_raw_and_xz(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_file(firmware_path(root, "raw.bin"), b"abc")
            write_file(firmware_path(root, "comp.bin.xz"), lzma.compress(b"hello"))
            self.assertEqual(
                ebr.resolve_firmware(root, "raw.bin"),
                firmware_path(root, "raw.bin"),
            )
            self.assertEqual(
                ebr.resolve_firmware(root, "comp.bin"),
                firmware_path(root, "comp.bin.xz"),
            )

    def test_missing_and_empty_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(ValueError):
                ebr.resolve_firmware(root, "gone.bin")
            write_file(firmware_path(root, "empty.bin"), b"")
            with self.assertRaises(ValueError):
                ebr.resolve_firmware(root, "empty.bin")

    def test_unsupported_suffix_not_resolved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_file(firmware_path(root, "x.bin.gz"), b"data")
            with self.assertRaises(ValueError):
                ebr.resolve_firmware(root, "x.bin")


class VerifyTreeTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.kver = "6.1.0-test"

    def tearDown(self):
        self._tmp.cleanup()

    def test_both_independent_models_valid(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"A")
        write_file(firmware_path(self.root, "b/fw.bin"), b"B")
        write_module(self.root, self.kver, "drv_a")
        write_module(self.root, self.kver, "drv_b")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            },
            "B": {
                "earlyFirmware": ["b/fw.bin"],
                "earlyDrivers": ["drv_b"],
                "firmwareSha256": {},
            },
        }
        self.assertIsNone(ebr.verify_tree(self.root, self.kver, requirements))

    def test_missing_requirements_rejected_model_specific(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"A")
        write_module(self.root, self.kver, "drv_a")
        write_module(self.root, self.kver, "drv_b")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            },
            "B": {
                "earlyFirmware": ["b/fw.bin"],
                "earlyDrivers": ["drv_b"],
                "firmwareSha256": {},
            },
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        message = str(ctx.exception)
        self.assertIn("B:", message)
        self.assertIn("b/fw.bin", message)

    def test_deleting_each_model_firmware_fails_model_specific(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"A")
        write_file(firmware_path(self.root, "b/fw.bin"), b"B")
        write_module(self.root, self.kver, "drv_a")
        write_module(self.root, self.kver, "drv_b")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            },
            "B": {
                "earlyFirmware": ["b/fw.bin"],
                "earlyDrivers": ["drv_b"],
                "firmwareSha256": {},
            },
        }
        (firmware_path(self.root, "a/fw.bin")).unlink()
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("A:", str(ctx.exception))
        self.assertNotIn("B:", str(ctx.exception))

    def test_wrong_case_board_path_fails(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"A")
        requirements = {
            "A": {
                "earlyFirmware": ["A/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_xz_accepted_and_pin_checks_decompressed(self):
        payload = b"decompressed-content"
        write_file(firmware_path(self.root, "a/fw.bin.xz"), lzma.compress(payload))
        digest = hashlib.sha256(payload).hexdigest()
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {"a/fw.bin": digest},
            }
        }
        self.assertIsNone(ebr.verify_tree(self.root, self.kver, requirements))

    def test_xz_pin_mismatch_fails(self):
        write_file(
            firmware_path(self.root, "a/fw.bin.xz"), lzma.compress(b"payload")
        )
        wrong = hashlib.sha256(b"other").hexdigest()
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {"a/fw.bin": wrong},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("sha256 mismatch", str(ctx.exception))

    def test_plain_pin_mismatch_fails(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"payload")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {"a/fw.bin": A_SHA},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_corrupt_xz_rejected(self):
        write_file(firmware_path(self.root, "a/fw.bin.xz"), b"not-xz-data")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {"a/fw.bin": A_SHA},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("could not be decompressed", str(ctx.exception))

    def test_corrupt_unpinned_xz_rejected(self):
        write_file(firmware_path(self.root, "a/fw.bin.xz"), b"not-xz-data")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("could not be decompressed", str(ctx.exception))

    def test_compressed_empty_unpinned_xz_rejected(self):
        write_file(firmware_path(self.root, "a/fw.bin.xz"), lzma.compress(b""))
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("empty payload", str(ctx.exception))

    def test_empty_firmware_rejected(self):
        write_file(firmware_path(self.root, "a/fw.bin"), b"")
        requirements = {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": [],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_exact_module_name_avoids_substring(self):
        write_module(self.root, self.kver, "drv_extended")
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("drv", str(ctx.exception))

    def test_module_suffixes_accepted(self):
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a", "drv_b", "drv_c"],
                "firmwareSha256": {},
            }
        }
        write_module(self.root, self.kver, "drv_a", suffix=".ko")
        write_module(self.root, self.kver, "drv_b", suffix=".ko.xz")
        write_module(self.root, self.kver, "drv_c", suffix=".ko.zst")
        self.assertIsNone(ebr.verify_tree(self.root, self.kver, requirements))

    def test_hyphen_underscore_equivalent(self):
        write_module(self.root, self.kver, "drv_mixed")
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv-mixed"],
                "firmwareSha256": {},
            }
        }
        self.assertIsNone(ebr.verify_tree(self.root, self.kver, requirements))

    def test_other_kver_does_not_satisfy(self):
        write_module(self.root, "6.2.0-other", "drv_a")
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_builtin_accepted(self):
        write_builtin(
            self.root, self.kver, ["kernel/drivers/net/drv_a.ko", "kernel/x"]
        )
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }
        self.assertIsNone(ebr.verify_tree(self.root, self.kver, requirements))

    def test_builtin_substring_not_accepted(self):
        write_builtin(self.root, self.kver, ["kernel/drivers/net/drv_extended.ko"])
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_builtin_modinfo_not_a_path_list(self):
        # modules.builtin.modinfo is NUL-delimited metadata, not a path list.
        # A module name appearing only there must NOT satisfy the driver check.
        payload = b"name=drv_a\0vermagic=6.1.0\0"
        write_builtin_modinfo(self.root, self.kver, payload)
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        self.assertIn("drv_a", str(ctx.exception))

    def test_builtin_modinfo_masquerading_path_not_accepted(self):
        # Even a path-looking .modinfo payload must not be treated as paths.
        payload = b"kernel/drivers/net/drv_a.ko\0kernel/x.ko\0"
        write_builtin_modinfo(self.root, self.kver, payload)
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_empty_module_file_rejected(self):
        write_module(self.root, self.kver, "drv_a", payload=b"")
        requirements = {
            "A": {
                "earlyFirmware": [],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }
        with self.assertRaises(ValueError):
            ebr.verify_tree(self.root, self.kver, requirements)

    def test_multiple_errors_retained(self):
        requirements = {
            "A": {
                "earlyFirmware": ["a/missing.bin"],
                "earlyDrivers": ["missing_drv"],
                "firmwareSha256": {},
            },
            "B": {
                "earlyFirmware": ["b/missing.bin"],
                "earlyDrivers": ["missing2"],
                "firmwareSha256": {},
            },
        }
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(self.root, self.kver, requirements)
        message = str(ctx.exception)
        self.assertIn("A:", message)
        self.assertIn("B:", message)
        self.assertGreaterEqual(message.count("; ") + 1, 3)


class VerifyInitramfsTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.base = Path(self._tmp.name)
        self.image = self.base / "initramfs.img"
        self.image.write_bytes(b"fake-image")

    def tearDown(self):
        self._tmp.cleanup()

    def _requirements(self):
        return {
            "A": {
                "earlyFirmware": ["a/fw.bin"],
                "earlyDrivers": ["drv_a"],
                "firmwareSha256": {},
            }
        }

    def _populate(self, workdir):
        workdir = Path(workdir)
        write_file(firmware_path(workdir, "a/fw.bin"), b"A")
        write_module(workdir, "6.1.0", "drv_a")

    def test_uses_lsinitrd_unpack_and_verifies(self):
        calls = []

        def fake_run(command, cwd=None, check=None, stdout=None, stderr=None):
            calls.append((list(command), str(cwd), check))
            self.assertTrue(Path(cwd).is_dir())
            self._populate(cwd)
            return subprocess.CompletedProcess(command, 0, b"", b"")

        with mock.patch.object(ebr.subprocess, "run", side_effect=fake_run):
            ebr.verify_initramfs(
                self.image, "6.1.0", self._requirements(), work_parent=self.base
            )

        self.assertEqual(len(calls), 1)
        command, cwd, check = calls[0]
        self.assertEqual(command[0], "lsinitrd")
        self.assertEqual(command[1], "--unpack")
        self.assertEqual(Path(command[2]), self.image.resolve())
        self.assertTrue(check)
        self.assertFalse(Path(cwd).exists())

    def test_process_failure_rejected(self):
        def fake_run(command, cwd=None, check=None, stdout=None, stderr=None):
            raise subprocess.CalledProcessError(
                returncode=2, cmd=command, output=b"", stderr=b"boom"
            )

        with mock.patch.object(ebr.subprocess, "run", side_effect=fake_run):
            with self.assertRaises(ValueError) as ctx:
                ebr.verify_initramfs(
                    self.image, "6.1.0", self._requirements(), work_parent=self.base
                )
        self.assertIn("lsinitrd failed", str(ctx.exception))
        self.assertIn("boom", str(ctx.exception))

    def test_extracted_tree_missing_data_rejected(self):
        def fake_run(command, cwd=None, check=None, stdout=None, stderr=None):
            return subprocess.CompletedProcess(command, 0, b"", b"")

        with mock.patch.object(ebr.subprocess, "run", side_effect=fake_run):
            with self.assertRaises(ValueError) as ctx:
                ebr.verify_initramfs(
                    self.image, "6.1.0", self._requirements(), work_parent=self.base
                )
        self.assertIn("A:", str(ctx.exception))

    def test_missing_image_rejected(self):
        with self.assertRaises(ValueError):
            ebr.verify_initramfs(
                self.base / "nope.img", "6.1.0", self._requirements()
            )


class CliIntegrationTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.base = Path(self._tmp.name)
        self.root = self.base / "root"
        self.config_path = self.base / "edl-devices.json"
        self.kver = "6.1.0"
        self.models = {
            "TB900": make_profile(
                early_firmware=["tb900/fw.bin"],
                early_drivers=["drv_tb900"],
            ),
            "TB901": make_profile(
                early_firmware=["tb901/fw.bin"],
                early_drivers=["drv_tb901"],
            ),
        }
        write_config(self.config_path, self.models)

    def tearDown(self):
        self._tmp.cleanup()

    def _populate_root(self):
        write_file(firmware_path(self.root, "tb900/fw.bin"), b"A")
        write_file(firmware_path(self.root, "tb901/fw.bin"), b"B")
        write_module(self.root, self.kver, "drv_tb900")
        write_module(self.root, self.kver, "drv_tb901")

    def _run(self, *extra):
        command = [
            sys.executable,
            str(BUILD_DIR / "edl_boot_requirements.py"),
            "--config",
            str(self.config_path),
            "--root",
            str(self.root),
            "--kver",
            self.kver,
        ] + list(extra)
        return subprocess.run(
            command, capture_output=True, text=True, check=False
        )

    def test_firmware_format_success(self):
        self._populate_root()
        result = self._run("--format", "firmware")
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.strip().splitlines()
        self.assertEqual(len(lines), 2)
        expected_a = str(firmware_path(self.root, "tb900/fw.bin").resolve())
        expected_b = str(firmware_path(self.root, "tb901/fw.bin").resolve())
        self.assertEqual(lines, [expected_a, expected_b])

    def test_firmware_format_preserves_relative_symlink_alias(self):
        self._populate_root()
        # Replace tb901/fw.bin with a relative symlink whose basename differs
        # from the target; the emitted CLI firmware name must be the alias.
        target = firmware_path(self.root, "tb901/fw_real.bin")
        link = firmware_path(self.root, "tb901/fw.bin")
        link.unlink()
        write_file(target, b"B")
        link.symlink_to(Path("fw_real.bin"))

        result = self._run("--format", "firmware")
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.strip().splitlines()
        alias_path = firmware_path(self.root, "tb901/fw.bin")
        self.assertIn(str(alias_path), lines)
        self.assertNotIn(str(target.resolve()), lines)

    def test_drivers_format_success_and_dedup(self):
        self._populate_root()
        result = self._run("--format", "drivers")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.strip().splitlines(), ["drv_tb900", "drv_tb901"]
        )

    def test_missing_data_failure(self):
        (self.base / "root").mkdir()
        result = self._run("--format", "firmware", "--model", "TB900")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("TB900:", result.stderr)

    def test_unknown_model_failure(self):
        self._populate_root()
        result = self._run("--format", "verify", "--model", "Nope")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unknown model", result.stderr)

    def test_verify_success_against_root(self):
        self._populate_root()
        result = self._run("--format", "verify")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_verify_with_initrd_uses_mocked_lsinitrd(self):
        self._populate_root()

        def fake_run(command, cwd=None, check=None, stdout=None, stderr=None):
            cwd = Path(cwd)
            write_file(firmware_path(cwd, "tb900/fw.bin"), b"A")
            write_file(firmware_path(cwd, "tb901/fw.bin"), b"B")
            write_module(cwd, self.kver, "drv_tb900")
            write_module(cwd, self.kver, "drv_tb901")
            return subprocess.CompletedProcess(command, 0, b"", b"")

        image = self.base / "initramfs.img"
        image.write_bytes(b"fake")
        with mock.patch.object(ebr.subprocess, "run", side_effect=fake_run):
            ebr.verify_initramfs(
                image,
                self.kver,
                ebr.load_requirements(make_config(self.models)),
                work_parent=self.base,
            )


if __name__ == "__main__":
    unittest.main()
