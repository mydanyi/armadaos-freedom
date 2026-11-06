#!/usr/bin/env python3
"""Regressions for TB322FC Bluetooth early-firmware/driver verification."""

from __future__ import annotations

import hashlib
import json
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BUILD_FILES = REPO_ROOT / "build_files"
if str(BUILD_FILES) not in sys.path:
    sys.path.insert(0, str(BUILD_FILES))

import edl_boot_requirements as ebr  # noqa: E402

DEVICES_JSON = BUILD_FILES / "edl-devices.json"
DTS_PATH = REPO_ROOT / "packages/kernel/dts/sm8750-lenovo-elden.dts"
REAL_FW_ROOT = REPO_ROOT / "system_files/usr/lib/firmware"

TB322FC_FIRMWARE = {
    "qca/brhbtfw20.tlv": "837aea39d33e95a4ba272ef2a2306e5ecf9848853332d2f7b8002901d6af771d",
    "qca/lenovo/tb322fc/brhbtnv20.bin": "f91354b0872f1f7680142df6852b2c8e67e3151ee2ad406b3d183a2fb539702e",
    "qca/brhperifw20.tlv": "9a8de09a127a5943f8c23d4ee6005531b900f085740a292c9ab5a20e76db6cb5",
    "qca/brhperinv20.bin": "3c73215352ba6422a9f2639124d27f64ec83a4d2aa91b388439ca88ea31098d0",
    "qca/tmel_peach_20.elf": "a175c8d9d0e6b38b5ae23ff0175da795b14b8268919463d2b6e1cb2bfeebc9f8",
}

BT_ALIAS = "of:N*T*Cqcom,wcn7861-bt"
BT_NVM_PATH = "qca/lenovo/tb322fc/brhbtnv20.bin"
BT_DTS_NVM = "lenovo/tb322fc/brhbtnv20.bin"
BT_DTS_FW = "brhbtfw20.tlv"
GLOBAL_NVM = "qca/brhbtnv20.bin"
KVER = "6.12.0-test"


def _sha256(data):
    return hashlib.sha256(data).hexdigest()


def _load_configuration():
    with open(DEVICES_JSON, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _write_bytes(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def _make_root(tmpdir, models=("TB322FC",), include_bt=True, skip_firmware=()):
    """Build a fake extracted root that satisfies the selected models.

    Bluetooth blobs are populated with fixture payloads and the SHA map is
    retargeted accordingly (mutating a local copy of the loaded profile),
    so structural checks do not depend on the real pinned hashes. Real
    pinned hashes are exercised by ``RealBlobHashTests`` and
    ``test_tb322fc_profile_has_pinned_bluetooth``.
    """
    configuration = _load_configuration()
    requirements = ebr.load_requirements(configuration, list(models))
    root = Path(tmpdir)
    fw_root = root / "usr/lib/firmware"

    for model in models:
        req = requirements[model]
        for rel in req["earlyFirmware"]:
            if rel in skip_firmware:
                continue
            if rel.startswith("qca/"):
                if not include_bt:
                    continue
                payload = ("BT-" + rel).encode("utf-8")
            else:
                payload = ("FW-" + rel).encode("utf-8")
            _write_bytes(fw_root / rel, payload)

    modules_root = root / "usr/lib/modules" / KVER
    kernel_root = modules_root / "kernel"
    for model in models:
        req = requirements[model]
        for drv in req["earlyDrivers"]:
            if drv in ("hci_uart", "btqca"):
                continue
            _write_bytes(
                kernel_root / "drivers/misc" / f"{drv}.ko",
                b"\x00module\x00",
            )

    if include_bt and "TB322FC" in models:
        _write_bytes(
            kernel_root / "drivers/bluetooth/hci_uart.ko", b"\x00hci_uart\x00"
        )
        _write_bytes(
            kernel_root / "drivers/bluetooth/btqca.ko", b"\x00btqca\x00"
        )
        aliases = (
            f"alias {BT_ALIAS} kernel/drivers/bluetooth/hci_uart.ko\n"
            "alias of:N*T*Cqcom,wcn7850-bt kernel/drivers/bluetooth/hci_uart.ko\n"
            "alias of:N*T*Cqcom,wcn6750-bt kernel/drivers/bluetooth/hci_uart.ko\n"
        )
        (modules_root / "modules.alias").write_text(aliases, encoding="utf-8")

    # Retarget all firmware hashes in the loaded profile to the fixture
    # payloads so verify_tree focuses on the behaviour under test.
    for model in models:
        req = requirements[model]
        new_sha = {}
        for rel in req.get("firmwareSha256", {}):
            path = fw_root / rel
            if path.is_file():
                new_sha[rel] = _sha256(path.read_bytes())
        req["firmwareSha256"] = new_sha

    return requirements, root


def _corrupt_file(root, rel):
    path = root / "usr/lib/firmware" / rel
    path.write_bytes(b"corrupted" + rel.encode("utf-8"))


class ConfigValidationTests(unittest.TestCase):
    def setUp(self):
        self.config = _load_configuration()

    def _load(self):
        return ebr.load_requirements(self.config, ["TB322FC"])

    def test_tb322fc_profile_has_pinned_bluetooth(self):
        req = self._load()["TB322FC"]
        for rel, digest in TB322FC_FIRMWARE.items():
            self.assertIn(rel, req["earlyFirmware"])
            self.assertEqual(req["firmwareSha256"][rel], digest)
        self.assertIn("hci_uart", req["earlyDrivers"])
        self.assertIn("btqca", req["earlyDrivers"])
        self.assertEqual(req["moduleAliases"]["hci_uart"], [BT_ALIAS])

    def test_other_profiles_have_no_module_aliases(self):
        req = ebr.load_requirements(self.config)
        for model, entry in req.items():
            if model == "TB322FC":
                continue
            self.assertNotIn("moduleAliases", entry)

    def test_alias_key_must_be_in_early_drivers(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {
            "bluetooth": [BT_ALIAS]
        }
        with self.assertRaises(ValueError):
            self._load()

    def test_alias_list_must_be_list(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {
            "hci_uart": BT_ALIAS
        }
        with self.assertRaises(ValueError):
            self._load()

    def test_alias_list_must_be_nonempty(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {
            "hci_uart": []
        }
        with self.assertRaises(ValueError):
            self._load()

    def test_empty_module_aliases_mapping_allowed(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {}
        req = self._load()
        self.assertEqual(req["TB322FC"]["moduleAliases"], {})

    def test_alias_entries_must_be_nonempty_strings(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {
            "hci_uart": [""]
        }
        with self.assertRaises(ValueError):
            self._load()

    def test_alias_rejects_whitespace(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = {
            "hci_uart": ["of:N*T*Cqcom, wcn7861-bt"]
        }
        with self.assertRaises(ValueError):
            self._load()

    def test_module_aliases_must_be_object(self):
        self.config["profiles"]["TB322FC"]["bootRequirements"]["moduleAliases"] = [
            BT_ALIAS
        ]
        with self.assertRaises(ValueError):
            self._load()


class AliasVerificationTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmpdir = Path(self._tmp.name)

    def test_modular_alias_succeeds(self):
        req, root = _make_root(self.tmpdir)
        ebr.verify_tree(root, KVER, req)

    def test_missing_alias_fails_even_with_module(self):
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "modules.alias").write_text(
            "alias of:N*T*Cqcom,wcn7850-bt kernel/drivers/bluetooth/hci_uart.ko\n",
            encoding="utf-8",
        )
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(root, KVER, req)
        message = str(ctx.exception)
        self.assertIn("TB322FC", message)
        self.assertIn("hci_uart", message)
        self.assertIn("wcn7861-bt", message)

    def test_alias_owned_by_wrong_module_fails(self):
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "modules.alias").write_text(
            f"alias {BT_ALIAS} kernel/drivers/bluetooth/btusb.ko\n",
            encoding="utf-8",
        )
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(root, KVER, req)
        message = str(ctx.exception)
        self.assertIn("hci_uart", message)
        self.assertIn("wcn7861-bt", message)

    def test_alias_requires_exact_or_cwildcard(self):
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "modules.alias").write_text(
            "alias of:N*T*Cqcom,wcn7861-btX kernel/drivers/bluetooth/hci_uart.ko\n",
            encoding="utf-8",
        )
        with self.assertRaises(ValueError):
            ebr.verify_tree(root, KVER, req)

    def test_alias_cwildcard_suffix_accepted(self):
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "modules.alias").write_text(
            f"alias {BT_ALIAS}C* kernel/drivers/bluetooth/hci_uart.ko\n",
            encoding="utf-8",
        )
        ebr.verify_tree(root, KVER, req)

    def test_builtin_alias_succeeds(self):
        """Built-in module path with .ko in modules.builtin must still match
        modules.builtin.modinfo keys that drop the .ko suffix."""
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "kernel/drivers/bluetooth/hci_uart.ko").unlink()
        (modules_root / "modules.alias").unlink()
        (modules_root / "modules.builtin").write_text(
            "kernel/drivers/bluetooth/hci_uart.ko\n", encoding="utf-8"
        )
        modinfo = (
            "kernel/drivers/bluetooth/hci_uart.label=hci_uart\x00"
            f"kernel/drivers/bluetooth/hci_uart.alias={BT_ALIAS}\x00"
        )
        (modules_root / "modules.builtin.modinfo").write_bytes(
            modinfo.encode("utf-8")
        )
        ebr.verify_tree(root, KVER, req)

    def test_builtin_without_alias_fails(self):
        req, root = _make_root(self.tmpdir)
        modules_root = root / "usr/lib/modules" / KVER
        (modules_root / "kernel/drivers/bluetooth/hci_uart.ko").unlink()
        (modules_root / "modules.alias").unlink()
        (modules_root / "modules.builtin").write_text(
            "kernel/drivers/bluetooth/hci_uart.ko\n", encoding="utf-8"
        )
        (modules_root / "modules.builtin.modinfo").write_bytes(
            b"kernel/drivers/bluetooth/hci_uart.label=hci_uart\x00"
        )
        with self.assertRaises(ValueError) as ctx:
            ebr.verify_tree(root, KVER, req)
        self.assertIn("hci_uart", str(ctx.exception))
        self.assertIn("alias", str(ctx.exception))


class FirmwareVerificationTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmpdir = Path(self._tmp.name)

    def test_each_missing_blob_fails_verify_tree(self):
        for rel in TB322FC_FIRMWARE:
            with self.subTest(rel=rel):
                with tempfile.TemporaryDirectory() as inner:
                    req, root = _make_root(inner)
                    (root / "usr/lib/firmware" / rel).unlink()
                    with self.assertRaises(ValueError) as ctx:
                        ebr.verify_tree(root, KVER, req)
                    self.assertIn(rel, str(ctx.exception))

    def test_mismatched_hash_reports_that_path(self):
        for rel in TB322FC_FIRMWARE:
            with self.subTest(rel=rel):
                with tempfile.TemporaryDirectory() as inner:
                    req, root = _make_root(inner)
                    _corrupt_file(root, rel)
                    with self.assertRaises(ValueError) as ctx:
                        ebr.verify_tree(root, KVER, req)
                    message = str(ctx.exception)
                    self.assertIn("sha256 mismatch", message)
                    self.assertIn(rel, message)
                    for other in TB322FC_FIRMWARE:
                        if other == rel:
                            continue
                        self.assertNotIn(other, message)


class RealBlobHashTests(unittest.TestCase):
    """Read the five actual blobs from system_files and match pinned hashes."""

    def test_real_blobs_match_pinned_hashes_and_profile(self):
        configuration = _load_configuration()
        req = ebr.load_requirements(configuration, ["TB322FC"])["TB322FC"]
        profile_sha = req["firmwareSha256"]
        for rel, expected in TB322FC_FIRMWARE.items():
            with self.subTest(rel=rel):
                self.assertIn(rel, req["earlyFirmware"])
                self.assertEqual(profile_sha.get(rel), expected)
                blob = REAL_FW_ROOT / rel
                self.assertTrue(
                    blob.is_file(), f"missing real blob: {blob}"
                )
                actual = _sha256(blob.read_bytes())
                self.assertEqual(actual, expected)


class NvmPathTests(unittest.TestCase):
    def test_profile_uses_board_specific_nvm_only(self):
        req = ebr.load_requirements(_load_configuration(), ["TB322FC"])["TB322FC"]
        firmware = req["earlyFirmware"]
        self.assertIn(BT_NVM_PATH, firmware)
        self.assertNotIn(GLOBAL_NVM, firmware)
        self.assertFalse(
            any(p.endswith("qca/brhbtnv20.bin") for p in firmware)
        )

    def test_system_files_has_no_global_qca_nvm(self):
        self.assertFalse(
            (REAL_FW_ROOT / GLOBAL_NVM).exists(),
            f"global NVM {GLOBAL_NVM} must not exist under system_files",
        )

    def test_dts_nvm_resolves_to_repo_blob(self):
        self.assertTrue((REAL_FW_ROOT / BT_NVM_PATH).is_file())


class DtsTests(unittest.TestCase):
    def _effective_dts(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "arch/arm64/boot/dts/qcom/sm8750-lenovo-elden.dts"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(DTS_PATH.read_text(encoding="utf-8"), encoding="utf-8")
            patch_path = DTS_PATH.with_suffix(".dts.patch")
            patch_text = patch_path.read_text(encoding="utf-8")
            subprocess.run(
                ["patch", "--batch", "--fuzz=0", "-p1"],
                cwd=tmp,
                input=patch_text,
                text=True,
                capture_output=True,
                check=True,
            )
            return target.read_text(encoding="utf-8")

    def _bluetooth_block(self):
        text = self._effective_dts()
        marker = 'compatible = "qcom,wcn7861-bt"'
        idx = text.find(marker)
        self.assertGreaterEqual(idx, 0, "wcn7861-bt compatible not found")
        brace = text.rfind("{", 0, idx)
        self.assertGreaterEqual(brace, 0)
        depth = 0
        start = brace
        for pos in range(brace, len(text)):
            ch = text[pos]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return text[start : pos + 1]
        self.fail("unterminated bluetooth node")

    def test_bluetooth_node_uses_board_specific_nvm(self):
        block = self._bluetooth_block()
        self.assertIn('compatible = "qcom,wcn7861-bt"', block)
        self.assertIn(BT_DTS_NVM, block)
        self.assertIn(BT_DTS_FW, block)
        self.assertNotIn('"brhbtnv20.bin"', block.replace(BT_DTS_NVM, ""))

    def test_local_bd_address_removed(self):
        text = self._effective_dts()
        self.assertNotIn("local-bd-address", text)


class IsolationTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmpdir = Path(self._tmp.name)

    def test_other_model_does_not_require_bt(self):
        req, root = _make_root(self.tmpdir, models=("TB323FU",), include_bt=False)
        self.assertNotIn("moduleAliases", req["TB323FU"])
        ebr.verify_tree(root, KVER, req)

    def test_selecting_tb322fc_only_does_not_force_other_models(self):
        configuration = _load_configuration()
        req = ebr.load_requirements(configuration, ["TB322FC"])
        self.assertEqual(set(req.keys()), {"TB322FC"})


if __name__ == "__main__":
    unittest.main()
