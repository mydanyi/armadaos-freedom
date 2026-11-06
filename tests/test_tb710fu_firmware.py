#!/usr/bin/env python3
"""Static checks for the TB710FU firmware bundle (devices/lenovo-tb710fu/firmware).

- SHA256SUMS lists exactly the shipped blobs and every hash matches;
- every TB710FU-only firmware path the board's device tree names
  (qcom/sm8650/xiaoxin/gt/..., novatek/...) is shipped, either in the bundle or
  in system_files/usr/lib/firmware;
- the four amplifier nodes name the board's own ACF file;
- the joined .mbn images start with an ELF header.

Run: python3 tests/test_tb710fu_firmware.py
"""

import hashlib
import pathlib
import re
import unittest

REPO = pathlib.Path(__file__).resolve().parents[1]
BUNDLE = REPO / "devices" / "lenovo-tb710fu" / "firmware"
SYSTEM_FW = REPO / "system_files" / "usr" / "lib" / "firmware"
DTS = REPO / "packages" / "kernel" / "dts" / "sm8650-lenovo-tb710fu.dts"
DTS_PATCH = REPO / "packages" / "kernel" / "dts" / "sm8650-lenovo-tb710fu.dts.patch"
DOCS = {"README.md"}
TOOLS = "tools"


def sums():
    entries = {}
    for line in (BUNDLE / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        digest, name = line.split("  ", 1)
        entries[name] = digest
    return entries


PATH_RE = r'"((?:qcom/sm8650/xiaoxin/gt|novatek)/[^"]+)"'
# The video codec (iris) is not enabled, so its firmware is not shipped.
NOT_SHIPPED = {"qcom/sm8650/xiaoxin/gt/vpu33_4v.mbn"}
# Names in the vendored board file that the patch replaces (the DSPs move to the joined .mbn).
REPLACED = {
    "qcom/sm8650/xiaoxin/gt/adsp.mbn", "qcom/sm8650/xiaoxin/gt/adsp_dtb.mbn",
    "qcom/sm8650/xiaoxin/gt/cdsp.mbn", "qcom/sm8650/xiaoxin/gt/cdsp_dtb.mbn",
}


def board_firmware_names():
    """TB710FU-only firmware-name values of the vendored board file plus its patch."""
    text = DTS_PATCH.read_text(encoding="utf-8")
    added = "\n".join(line[1:] for line in text.splitlines() if line.startswith("+"))
    base = set(re.findall(PATH_RE, DTS.read_text(encoding="utf-8"))) - REPLACED
    return (base | set(re.findall(PATH_RE, added))) - NOT_SHIPPED


class BundleTest(unittest.TestCase):
    def test_sums_cover_exactly_the_blobs(self):
        shipped = {
            path.relative_to(BUNDLE).as_posix()
            for path in BUNDLE.rglob("*")
            if path.is_file()
            and path.name != "SHA256SUMS"
            and path.relative_to(BUNDLE).as_posix() not in DOCS
            and path.relative_to(BUNDLE).parts[0] != TOOLS
        }
        self.assertEqual(shipped, set(sums()))

    def test_hashes_match(self):
        for name, digest in sums().items():
            with self.subTest(name=name):
                self.assertEqual(hashlib.sha256((BUNDLE / name).read_bytes()).hexdigest(), digest)

    def test_every_board_only_path_is_shipped(self):
        names = board_firmware_names()
        self.assertTrue(names)
        for name in names:
            with self.subTest(name=name):
                self.assertTrue((BUNDLE / name).is_file() or (SYSTEM_FW / name).is_file(), name)

    def test_dsp_images_are_joined_elf_files(self):
        for name in ("adsp", "adsp_dtb", "cdsp", "cdsp_dtb"):
            with self.subTest(name=name):
                data = (BUNDLE / "qcom" / "sm8650" / "xiaoxin" / "gt" / f"{name}.mbn").read_bytes()
                self.assertEqual(data[:4], b"\x7fELF")

    def test_amplifiers_name_the_board_acf(self):
        text = DTS_PATCH.read_text(encoding="utf-8")
        self.assertEqual(
            text.count('+\t\tfirmware-name = "qcom/sm8650/xiaoxin/gt/aw882xx_acf.bin";'),
            text.count('compatible = "awinic,aw882xx_smartpa"'),
        )
        self.assertEqual(text.count('compatible = "awinic,aw882xx_smartpa"'), 4)

    def test_no_bare_acf_at_the_firmware_root(self):
        self.assertNotIn("aw882xx_acf.bin", sums())


if __name__ == "__main__":
    unittest.main()
