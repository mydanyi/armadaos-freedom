#!/usr/bin/env python3
"""RED tests for the configuration-driven EDL device profiles.

These tests describe the contract of build_files/edl_device_config.py and of
the declarative build_files/edl-devices.json document before any production
behaviour exists. They use the standard library only, never touch GI, OSTree or
a real device, and keep every temporary document synthetic (TMPDIR is honoured
through tempfile).

The two real records are pinned to the data the hardcoded production table in
build_files/make-edl-raw-package.py carries today; a third, purely synthetic
profile proves that future models are configuration, not code.
"""

from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = REPO_ROOT / "build_files" / "edl-devices.json"
HELPER_PATH = REPO_ROOT / "build_files" / "edl_device_config.py"

SCHEMA_VERSION = 1
RUNTIME_SOURCE = "devices/lenovo-tb323fu/boot/kshim-success-source"
BOOT_PARTITION_BYTES = 100663296

TB322FC_PROFILE = {
    "displayName": "Lenovo Legion Y700 Gen 4",
    "compatible": {"soc": "SM8750", "device": "elden"},
    "dtb": "qcom/sm8750-lenovo-elden.dtb",
    "required": ["lenovo,elden", "qcom,sm8750"],
    "forbidden": [
        "lenovo,baldur",
        "lenovo,tb323fu",
        "qcom,sm8850",
        "qcom,canoe",
        "qcom,canoep",
        "qcom,kaanapali",
    ],
    "extraArgs": [],
    "runtimeConfig": "configs/tb322fc_android_defconfig",
    "runtimeCmakeArgs": ["-DKSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL=OFF"],
    "bootPartitionBytes": BOOT_PARTITION_BYTES,
    "diagnosticMode": "rd-break",
    "bootRequirements": {
        "earlyFirmware": [
            "qcom/gen80000_sqe.fw",
            "qcom/gen80000_gmu.bin",
            "qcom/gen80000_aqe.fw",
            "qcom/sm8750/lenovo/elden/gen80000_zap.mbn",
            "novatek_ts_csot_3k_fw.bin",
        ],
        "earlyDrivers": [
            "qcom_pil_info",
            "qcom_common",
            "qcom_sysmon",
            "qcom_q6v5",
            "qcom_q6v5_pas",
            "novatek-nvt36xxx-spi",
        ],
        "firmwareSha256": {},
    },
}

TB323FU_PROFILE = {
    "displayName": "Lenovo Legion Y700 Gen 5",
    "compatible": {"soc": "SM8850", "device": "baldur"},
    "dtb": "qcom/kaanapali-lenovo-baldur-display.dtb",
    "required": ["lenovo,baldur", "qcom,kaanapali"],
    "forbidden": ["lenovo,elden", "lenovo,tb322fc", "qcom,sm8750", "qcom,sm8550"],
    "extraArgs": [
        "baldur.end=hold",
        "cpuidle_psci_domain.allow_cluster_off=0",
        "msm.dpu_use_virtual_planes=0",
    ],
    "runtimeConfig": "configs/tb323fu_unified_defconfig",
    "runtimeCmakeArgs": ["-DKSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL=ON"],
    "bootPartitionBytes": BOOT_PARTITION_BYTES,
    "diagnosticMode": "verbose",
    "bootRequirements": {
        "earlyFirmware": [
            "qcom/gen80200_sqe.fw",
            "qcom/gen80200_gmu.bin",
            "qcom/gen80200_aqe.fw",
            "qcom/kaanapali/Lenovo/baldur/gen80200_zap.mbn",
            "novatek/novatek_ts_fw.bin",
        ],
        "earlyDrivers": [
            "qcom_pil_info",
            "qcom_common",
            "qcom_sysmon",
            "qcom_q6v5",
            "qcom_q6v5_pas",
            "nt36536_ts",
        ],
        "firmwareSha256": {
            "qcom/kaanapali/Lenovo/baldur/gen80200_zap.mbn": "2d56c203d49492363c5458812d8da41a3e6a8949f62b72f090653b4897f1013a",
            "novatek/novatek_ts_fw.bin": "a374ce0856f9e61bd62de955e93d2585963a0636ff7390de2248386e896c114f",
        },
    },
}

REAL_PROFILES = {"TB322FC": TB322FC_PROFILE, "TB323FU": TB323FU_PROFILE}

TEST_THIRD_PROFILE = {
    "displayName": "Synthetic Test Device",
    "compatible": {"soc": "TEST-SOC", "device": "synthetic"},
    "dtb": "synthetic/test-third.dtb",
    "required": ["vendor,test-third"],
    "forbidden": ["vendor,something-else"],
    "extraArgs": ["test.flag=1"],
    "runtimeConfig": "configs/test_third_defconfig",
    "runtimeCmakeArgs": ["-DTEST_THIRD=ON", "-DTEST_THIRD_LEVEL=2"],
    "bootPartitionBytes": 4096,
    "diagnosticMode": "verbose",
}

_DELETE = object()


def _import_helper():
    spec = importlib.util.spec_from_file_location("edl_device_config", str(HELPER_PATH))
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot import {HELPER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


HELPER = _import_helper()


def _valid_document(profiles=None):
    if profiles is None:
        profiles = copy.deepcopy(REAL_PROFILES)
    return {
        "schemaVersion": SCHEMA_VERSION,
        "runtimeSource": RUNTIME_SOURCE,
        "profiles": profiles,
    }


def _set(keys, value=_DELETE):
    def mutate(document):
        target = document
        for key in keys[:-1]:
            target = target[key]
        if value is _DELETE:
            del target[keys[-1]]
        else:
            target[keys[-1]] = value

    return mutate


def _rename_model(name):
    def mutate(document):
        document["profiles"][name] = document["profiles"].pop("TB322FC")

    return mutate


def _invalid_cases():
    profile = ("profiles", "TB322FC")
    compatible = profile + ("compatible",)
    cases = [
        ("schemaVersion missing", _set(("schemaVersion",))),
        ("schemaVersion is a string", _set(("schemaVersion",), "1")),
        ("schemaVersion is a boolean", _set(("schemaVersion",), True)),
        ("schemaVersion is two", _set(("schemaVersion",), 2)),
        ("runtimeSource missing", _set(("runtimeSource",))),
        ("runtimeSource empty", _set(("runtimeSource",), "")),
        ("runtimeSource absolute", _set(("runtimeSource",), "/devices/x")),
        ("runtimeSource backslash", _set(("runtimeSource",), "devices\\x")),
        ("runtimeSource dot segment", _set(("runtimeSource",), "./devices/x")),
        ("runtimeSource dotdot segment", _set(("runtimeSource",), "devices/../x")),
        ("runtimeSource empty segment", _set(("runtimeSource",), "devices//x")),
        ("runtimeSource control character", _set(("runtimeSource",), "devices/\x01x")),
        ("runtimeSource not a string", _set(("runtimeSource",), 5)),
        ("profiles missing", _set(("profiles",))),
        ("profiles not a mapping", _set(("profiles",), [])),
        ("profiles empty", _set(("profiles",), {})),
        ("profile not a mapping", _set(profile, [])),
        ("model empty", _rename_model("")),
        ("model is dot", _rename_model(".")),
        ("model is dotdot", _rename_model("..")),
        ("model carries a slash", _rename_model("TB/322")),
        ("model carries a backslash", _rename_model("TB\\322")),
        ("model carries a control character", _rename_model("TB\x02322")),
        ("displayName missing", _set(profile + ("displayName",))),
        ("displayName empty", _set(profile + ("displayName",), "")),
        ("displayName not a string", _set(profile + ("displayName",), 5)),
        ("compatible missing", _set(compatible)),
        ("compatible not a mapping", _set(compatible, [])),
        ("compatible soc missing", _set(compatible + ("soc",))),
        ("compatible soc empty", _set(compatible + ("soc",), "")),
        ("compatible soc not a string", _set(compatible + ("soc",), 5)),
        ("compatible device missing", _set(compatible + ("device",))),
        ("compatible device empty", _set(compatible + ("device",), "")),
        ("compatible device not a string", _set(compatible + ("device",), 5)),
        ("dtb missing", _set(profile + ("dtb",))),
        ("dtb empty", _set(profile + ("dtb",), "")),
        ("dtb absolute", _set(profile + ("dtb",), "/qcom/board.dtb")),
        ("dtb backslash", _set(profile + ("dtb",), "qcom\\board.dtb")),
        ("dtb dot segment", _set(profile + ("dtb",), "qcom/./board.dtb")),
        ("dtb dotdot segment", _set(profile + ("dtb",), "qcom/../board.dtb")),
        ("dtb control character", _set(profile + ("dtb",), "qcom/board\x00.dtb")),
        ("dtb not a string", _set(profile + ("dtb",), 7)),
        ("required missing", _set(profile + ("required",))),
        ("required is a string", _set(profile + ("required",), "lenovo,elden")),
        ("required element not a string", _set(profile + ("required",), ["ok", 5])),
        ("required is a mapping", _set(profile + ("required",), {"a": "b"})),
        ("forbidden missing", _set(profile + ("forbidden",))),
        ("forbidden is a string", _set(profile + ("forbidden",), "lenovo,elden")),
        ("forbidden element not a string", _set(profile + ("forbidden",), [1])),
        ("forbidden is a mapping", _set(profile + ("forbidden",), {"a": "b"})),
        ("extraArgs missing", _set(profile + ("extraArgs",))),
        ("extraArgs is a string", _set(profile + ("extraArgs",), "console=tty0")),
        ("extraArgs element not a string", _set(profile + ("extraArgs",), [5])),
        ("extraArgs element with newline", _set(profile + ("extraArgs",), ["a\nb"])),
        ("extraArgs element with carriage return", _set(profile + ("extraArgs",), ["a\rb"])),
        ("extraArgs element with NUL", _set(profile + ("extraArgs",), ["a\x00b"])),
        ("runtimeConfig missing", _set(profile + ("runtimeConfig",))),
        ("runtimeConfig empty", _set(profile + ("runtimeConfig",), "")),
        ("runtimeConfig absolute", _set(profile + ("runtimeConfig",), "/configs/x")),
        ("runtimeConfig backslash", _set(profile + ("runtimeConfig",), "configs\\x")),
        ("runtimeConfig dotdot segment", _set(profile + ("runtimeConfig",), "configs/../x")),
        ("runtimeConfig not a string", _set(profile + ("runtimeConfig",), 3)),
        ("runtimeCmakeArgs missing", _set(profile + ("runtimeCmakeArgs",))),
        ("runtimeCmakeArgs is a string", _set(profile + ("runtimeCmakeArgs",), "-DX=1")),
        ("runtimeCmakeArgs element not a string", _set(profile + ("runtimeCmakeArgs",), [5])),
        (
            "runtimeCmakeArgs element with newline",
            _set(profile + ("runtimeCmakeArgs",), ["-DX=1\n-DY=2"]),
        ),
        (
            "runtimeCmakeArgs element with NUL",
            _set(profile + ("runtimeCmakeArgs",), ["-DX=1\x00"]),
        ),
        ("runtimeCmakeArgs is a mapping", _set(profile + ("runtimeCmakeArgs",), {})),
        ("bootPartitionBytes missing", _set(profile + ("bootPartitionBytes",))),
        ("bootPartitionBytes is true", _set(profile + ("bootPartitionBytes",), True)),
        ("bootPartitionBytes is false", _set(profile + ("bootPartitionBytes",), False)),
        ("bootPartitionBytes zero", _set(profile + ("bootPartitionBytes",), 0)),
        ("bootPartitionBytes negative", _set(profile + ("bootPartitionBytes",), -4096)),
        ("bootPartitionBytes is a string", _set(profile + ("bootPartitionBytes",), "100663296")),
        ("bootPartitionBytes is a float", _set(profile + ("bootPartitionBytes",), 100663296.0)),
        ("bootPartitionBytes is null", _set(profile + ("bootPartitionBytes",), None)),
        ("diagnosticMode missing", _set(profile + ("diagnosticMode",))),
        ("diagnosticMode unknown", _set(profile + ("diagnosticMode",), "debug")),
        ("diagnosticMode uppercase", _set(profile + ("diagnosticMode",), "RD-BREAK")),
        ("diagnosticMode empty", _set(profile + ("diagnosticMode",), "")),
        ("diagnosticMode not a string", _set(profile + ("diagnosticMode",), 5)),
    ]
    return cases


def _run_cli(argv):
    stdout = io.StringIO()
    stderr = io.StringIO()
    try:
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = HELPER.main(list(argv))
    except SystemExit as exit_error:
        code = exit_error.code if isinstance(exit_error.code, int) else 1
    return code, stdout.getvalue(), stderr.getvalue()


class TempConfigMixin:
    """Temporary configuration documents; tempfile honours TMPDIR."""

    def make_directory(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        return Path(directory.name)

    def write_config(self, document, directory=None, name="edl-devices.json"):
        if directory is None:
            directory = self.make_directory()
        path = Path(directory) / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def write_text(self, text, name="edl-devices.json"):
        path = self.make_directory() / name
        path.write_text(text, encoding="utf-8")
        return path


class RealConfigurationTests(TempConfigMixin, unittest.TestCase):
    def test_document_matches_the_production_profiles(self):
        document = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        self.assertEqual(document["schemaVersion"], SCHEMA_VERSION)
        self.assertEqual(document["runtimeSource"], RUNTIME_SOURCE)
        self.assertEqual(document["profiles"], REAL_PROFILES)

    def test_document_keeps_json_lists_and_model_order(self):
        document = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        self.assertEqual(list(document["profiles"]), ["TB322FC", "TB323FU"])
        for name, profile in document["profiles"].items():
            with self.subTest(profile=name):
                for field in ("required", "forbidden", "extraArgs", "runtimeCmakeArgs"):
                    self.assertIsInstance(profile[field], list)
                self.assertEqual(profile["bootPartitionBytes"], BOOT_PARTITION_BYTES)
                self.assertIn(profile["diagnosticMode"], ("rd-break", "verbose"))

    def test_load_configuration_returns_the_document(self):
        configuration = HELPER.load_configuration(CONFIG_PATH)
        self.assertEqual(configuration["schemaVersion"], SCHEMA_VERSION)
        self.assertEqual(configuration["runtimeSource"], RUNTIME_SOURCE)
        self.assertEqual(list(configuration["profiles"]), ["TB322FC", "TB323FU"])
        self.assertEqual(configuration["profiles"]["TB322FC"], TB322FC_PROFILE)
        self.assertEqual(configuration["profiles"]["TB323FU"], TB323FU_PROFILE)

    def test_load_configuration_preserves_list_order(self):
        configuration = HELPER.load_configuration(CONFIG_PATH)
        self.assertEqual(
            list(configuration["profiles"]["TB322FC"]["forbidden"]),
            TB322FC_PROFILE["forbidden"],
        )
        self.assertEqual(
            list(configuration["profiles"]["TB323FU"]["extraArgs"]),
            TB323FU_PROFILE["extraArgs"],
        )
        self.assertEqual(
            list(configuration["profiles"]["TB323FU"]["runtimeCmakeArgs"]),
            TB323FU_PROFILE["runtimeCmakeArgs"],
        )


class SelectionTests(unittest.TestCase):
    def setUp(self):
        self.configuration = HELPER.load_configuration(CONFIG_PATH)

    def test_default_selects_every_model_in_configuration_order(self):
        self.assertEqual(HELPER.select_models(self.configuration), ("TB322FC", "TB323FU"))

    def test_single_model_selection(self):
        self.assertEqual(HELPER.select_models(self.configuration, ["TB323FU"]), ("TB323FU",))

    def test_selection_follows_configuration_order(self):
        self.assertEqual(
            HELPER.select_models(self.configuration, ["TB323FU", "TB322FC"]),
            ("TB322FC", "TB323FU"),
        )

    def test_unknown_model_is_rejected(self):
        with self.assertRaises(ValueError):
            HELPER.select_models(self.configuration, ["TB9999"])

    def test_duplicate_model_is_rejected(self):
        with self.assertRaises(ValueError):
            HELPER.select_models(self.configuration, ["TB322FC", "TB322FC"])

    def test_empty_selection_is_rejected(self):
        with self.assertRaises(ValueError):
            HELPER.select_models(self.configuration, [])


class SyntheticProfileTests(TempConfigMixin, unittest.TestCase):
    def _configuration(self, profiles):
        path = self.write_config(_valid_document(profiles))
        return path, HELPER.load_configuration(path)

    def test_third_profile_loads_with_a_custom_boot_budget(self):
        profile = copy.deepcopy(TEST_THIRD_PROFILE)
        profile["bootPartitionBytes"] = 777777
        _path, configuration = self._configuration(
            {
                "TB322FC": copy.deepcopy(TB322FC_PROFILE),
                "TB323FU": copy.deepcopy(TB323FU_PROFILE),
                "TEST_THIRD": profile,
            }
        )
        self.assertEqual(configuration["profiles"]["TEST_THIRD"]["bootPartitionBytes"], 777777)
        self.assertEqual(configuration["profiles"]["TEST_THIRD"], profile)

    def test_third_profile_is_selected_by_default_after_the_real_models(self):
        _path, configuration = self._configuration(
            {
                "TB322FC": copy.deepcopy(TB322FC_PROFILE),
                "TB323FU": copy.deepcopy(TB323FU_PROFILE),
                "TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE),
            }
        )
        self.assertEqual(
            HELPER.select_models(configuration),
            ("TB322FC", "TB323FU", "TEST_THIRD"),
        )

    def test_synthetic_document_alone_has_no_fixed_model_count(self):
        _path, configuration = self._configuration(
            {"TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE)}
        )
        self.assertEqual(HELPER.select_models(configuration), ("TEST_THIRD",))
        self.assertEqual(
            HELPER.select_models(configuration, ["TEST_THIRD"]), ("TEST_THIRD",)
        )

    def test_synthetic_subset_keeps_configuration_order(self):
        _path, configuration = self._configuration(
            {
                "TB322FC": copy.deepcopy(TB322FC_PROFILE),
                "TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE),
            }
        )
        self.assertEqual(
            HELPER.select_models(configuration, ["TEST_THIRD", "TB322FC"]),
            ("TB322FC", "TEST_THIRD"),
        )
        self.assertEqual(
            HELPER.select_models(configuration, ["TEST_THIRD"]), ("TEST_THIRD",)
        )


class CliTests(TempConfigMixin, unittest.TestCase):
    def _config(self):
        return str(CONFIG_PATH)

    def _synthetic_third_config(self):
        return str(
            self.write_config(
                _valid_document(
                    {
                        "TB322FC": copy.deepcopy(TB322FC_PROFILE),
                        "TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE),
                    }
                )
            )
        )

    def test_models_lists_every_selected_id(self):
        code, stdout, _stderr = _run_cli(["--config", self._config(), "--format", "models"])
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TB322FC", "TB323FU"])

    def test_models_lists_a_synthetic_third_profile(self):
        config = self._synthetic_third_config()
        code, stdout, _stderr = _run_cli(["--config", config, "--format", "models"])
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TB322FC", "TEST_THIRD"])
        code, stdout, _stderr = _run_cli(
            ["--config", config, "--format", "models", "--model", "TEST_THIRD"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TEST_THIRD"])

    def test_models_honours_a_single_model(self):
        code, stdout, _stderr = _run_cli(
            ["--config", self._config(), "--format", "models", "--model", "TB323FU"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TB323FU"])

    def test_models_keeps_configuration_order_for_reversed_selection(self):
        code, stdout, _stderr = _run_cli(
            [
                "--config",
                self._config(),
                "--format",
                "models",
                "--model",
                "TB323FU",
                "--model",
                "TB322FC",
            ]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TB322FC", "TB323FU"])

    def test_runtime_source_prints_the_single_source_path(self):
        code, stdout, _stderr = _run_cli(
            ["--config", self._config(), "--format", "runtime-source"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), [RUNTIME_SOURCE])

    def test_runtime_plan_prints_model_and_config_pairs(self):
        code, stdout, _stderr = _run_cli(
            ["--config", self._config(), "--format", "runtime-plan"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.splitlines(),
            [
                "TB322FC\tconfigs/tb322fc_android_defconfig",
                "TB323FU\tconfigs/tb323fu_unified_defconfig",
            ],
        )

    def test_runtime_plan_includes_a_synthetic_third_profile(self):
        config = self._synthetic_third_config()
        code, stdout, _stderr = _run_cli(
            ["--config", config, "--format", "runtime-plan"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.splitlines(),
            [
                "TB322FC\tconfigs/tb322fc_android_defconfig",
                "TEST_THIRD\tconfigs/test_third_defconfig",
            ],
        )
        code, stdout, _stderr = _run_cli(
            ["--config", config, "--format", "runtime-plan", "--model", "TEST_THIRD"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["TEST_THIRD\tconfigs/test_third_defconfig"])

    def test_runtime_args_emits_one_argument_per_line(self):
        code, stdout, _stderr = _run_cli(
            [
                "--config",
                self._config(),
                "--format",
                "runtime-args",
                "--model",
                "TB323FU",
            ]
        )
        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.splitlines(), ["-DKSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL=ON"]
        )

    def test_runtime_args_supports_a_third_synthetic_profile(self):
        path = self.write_config(
            _valid_document(
                {
                    "TB322FC": copy.deepcopy(TB322FC_PROFILE),
                    "TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE),
                }
            )
        )
        code, stdout, _stderr = _run_cli(
            ["--config", str(path), "--format", "runtime-args", "--model", "TEST_THIRD"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["-DTEST_THIRD=ON", "-DTEST_THIRD_LEVEL=2"])

    def test_runtime_args_with_an_empty_array_emits_nothing(self):
        profile = copy.deepcopy(TEST_THIRD_PROFILE)
        profile["runtimeCmakeArgs"] = []
        path = self.write_config(_valid_document({"TEST_THIRD": profile}))
        code, stdout, _stderr = _run_cli(
            ["--config", str(path), "--format", "runtime-args", "--model", "TEST_THIRD"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.strip(), "")

    def test_runtime_args_requires_exactly_one_selected_profile(self):
        code, _stdout, stderr = _run_cli(
            ["--config", self._config(), "--format", "runtime-args"]
        )
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_runtime_args_rejects_two_selected_profiles(self):
        code, _stdout, stderr = _run_cli(
            [
                "--config",
                self._config(),
                "--format",
                "runtime-args",
                "--model",
                "TB322FC",
                "--model",
                "TB323FU",
            ]
        )
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_runtime_args_rejects_an_unknown_model(self):
        code, _stdout, stderr = _run_cli(
            ["--config", self._config(), "--format", "runtime-args", "--model", "TB9999"]
        )
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_readme_lists_display_name_and_model(self):
        code, stdout, _stderr = _run_cli(
            ["--config", self._config(), "--format", "readme"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.splitlines(),
            ["Lenovo Legion Y700 Gen 4 (TB322FC)", "Lenovo Legion Y700 Gen 5 (TB323FU)"],
        )

    def test_readme_includes_a_synthetic_third_profile(self):
        config = self._synthetic_third_config()
        code, stdout, _stderr = _run_cli(["--config", config, "--format", "readme"])
        self.assertEqual(code, 0)
        self.assertEqual(
            stdout.splitlines(),
            ["Lenovo Legion Y700 Gen 4 (TB322FC)", "Synthetic Test Device (TEST_THIRD)"],
        )
        code, stdout, _stderr = _run_cli(
            ["--config", config, "--format", "readme", "--model", "TEST_THIRD"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines(), ["Synthetic Test Device (TEST_THIRD)"])

    def test_json_prints_the_whole_document(self):
        code, stdout, _stderr = _run_cli(
            ["--config", self._config(), "--format", "json"]
        )
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout),
            {
                "schemaVersion": SCHEMA_VERSION,
                "runtimeSource": RUNTIME_SOURCE,
                "profiles": REAL_PROFILES,
            },
        )

    def test_json_keeps_only_the_selected_profiles(self):
        code, stdout, _stderr = _run_cli(
            [
                "--config",
                self._config(),
                "--format",
                "json",
                "--model",
                "TB323FU",
            ]
        )
        self.assertEqual(code, 0)
        document = json.loads(stdout)
        self.assertEqual(document["schemaVersion"], SCHEMA_VERSION)
        self.assertEqual(document["runtimeSource"], RUNTIME_SOURCE)
        self.assertEqual(list(document["profiles"]), ["TB323FU"])
        self.assertEqual(document["profiles"]["TB323FU"], TB323FU_PROFILE)

    def test_json_keeps_a_synthetic_profile_intact(self):
        path = self.write_config(
            _valid_document({"TEST_THIRD": copy.deepcopy(TEST_THIRD_PROFILE)})
        )
        code, stdout, _stderr = _run_cli(["--config", str(path), "--format", "json"])
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(stdout)["profiles"], {"TEST_THIRD": TEST_THIRD_PROFILE}
        )

    def test_missing_config_file_fails(self):
        missing = str(self.make_directory() / "absent.json")
        code, _stdout, stderr = _run_cli(["--config", missing, "--format", "models"])
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_malformed_config_file_fails(self):
        path = self.write_text("{ not json")
        code, _stdout, stderr = _run_cli(["--config", str(path), "--format", "models"])
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_invalid_config_document_fails(self):
        path = self.write_config(_valid_document({"TB322FC": {"displayName": ""}}))
        code, _stdout, stderr = _run_cli(["--config", str(path), "--format", "models"])
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_unknown_format_fails(self):
        code, _stdout, stderr = _run_cli(
            ["--config", self._config(), "--format", "yaml"]
        )
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())

    def test_missing_config_argument_fails(self):
        code, _stdout, stderr = _run_cli(["--format", "models"])
        self.assertNotEqual(code, 0)
        self.assertTrue(stderr.strip())


class ConfigurationValidationTests(TempConfigMixin, unittest.TestCase):
    def test_root_must_be_a_mapping(self):
        for payload in ([], "text", 5, None):
            with self.subTest(payload=payload):
                path = self.write_config(payload)
                with self.assertRaises(ValueError):
                    HELPER.load_configuration(path)

    def test_malformed_documents_are_rejected(self):
        for label, mutate in _invalid_cases():
            with self.subTest(case=label):
                document = _valid_document()
                mutate(document)
                path = self.write_config(document)
                with self.assertRaises(ValueError):
                    HELPER.load_configuration(path)

    def test_empty_argument_and_compatibility_lists_are_allowed(self):
        profile = copy.deepcopy(TB322FC_PROFILE)
        profile["extraArgs"] = []
        profile["runtimeCmakeArgs"] = []
        profile["required"] = ["vendor,single"]
        profile["forbidden"] = ["vendor,other"]
        path = self.write_config(_valid_document({"TB322FC": profile}))
        configuration = HELPER.load_configuration(path)
        self.assertEqual(configuration["profiles"]["TB322FC"], profile)

    def test_any_positive_boot_budget_is_allowed(self):
        for budget in (1, 4096, BOOT_PARTITION_BYTES, 1 << 40):
            with self.subTest(budget=budget):
                profile = copy.deepcopy(TB322FC_PROFILE)
                profile["bootPartitionBytes"] = budget
                path = self.write_config(_valid_document({"TB322FC": profile}))
                configuration = HELPER.load_configuration(path)
                self.assertEqual(
                    configuration["profiles"]["TB322FC"]["bootPartitionBytes"], budget
                )

    def test_synthetic_model_identifier_is_allowed(self):
        path = self.write_config(
            _valid_document({"TEST_THIRD_2": copy.deepcopy(TEST_THIRD_PROFILE)})
        )
        configuration = HELPER.load_configuration(path)
        self.assertEqual(list(configuration["profiles"]), ["TEST_THIRD_2"])

    def test_model_identifier_with_dots_is_allowed(self):
        path = self.write_config(
            _valid_document({"TEST.THREE-2": copy.deepcopy(TEST_THIRD_PROFILE)})
        )
        configuration = HELPER.load_configuration(path)
        self.assertEqual(list(configuration["profiles"]), ["TEST.THREE-2"])
        self.assertEqual(HELPER.select_models(configuration), ("TEST.THREE-2",))
        self.assertEqual(
            HELPER.select_models(configuration, ["TEST.THREE-2"]), ("TEST.THREE-2",)
        )


if __name__ == "__main__":
    unittest.main()
