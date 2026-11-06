#!/usr/bin/env python3
"""Shared EDL boot-requirements verifier.

Reads ``bootRequirements`` from edl-devices.json profiles and verifies that a
built root tree (and optionally a produced initramfs archive) contains the
required early firmware files and kernel driver modules.

Public surface:

    load_requirements(configuration, models=None) -> dict
    resolve_firmware(root, relative) -> Path
    verify_tree(root, kver, requirements) -> None  (raises ValueError)
    verify_initramfs(image, kver, requirements, work_parent=None) -> None

CLI:

    python build_files/edl_boot_requirements.py --config PATH \
        --root PATH --kver TEXT [--model ID ...] \
        --format {firmware,drivers,verify} [--initrd PATH] [--work-dir PATH]
"""

from __future__ import annotations

import argparse
import hashlib
import lzma
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath

_MODULE_BASENAME_RE = re.compile(r"^[A-Za-z0-9_\-]+$")
_HASH_RE = re.compile(r"^[0-9a-f]{64}$")
_DRIVER_SUFFIXES = (".ko", ".ko.xz", ".ko.zst")
_LEGACY_FIRMWARE_PREFIX = PurePosixPath("usr/lib/firmware")


def _has_control_characters(value):
    return any(
        ord(character) < 0x20 or 0x7F <= ord(character) <= 0x9F
        for character in value
    )


def _plain_text(value, field):
    if not isinstance(value, str) or not value:
        raise ValueError(f"{field} must be a nonempty string")
    if _has_control_characters(value):
        raise ValueError(f"{field} must not contain control characters")
    if value.strip() != value:
        raise ValueError(f"{field} must not have leading or trailing whitespace")
    return value


def _relative_firmware_path(value, field):
    _plain_text(value, field)
    if "\\" in value:
        raise ValueError(f"{field} must not contain backslashes")
    if value.startswith("/"):
        raise ValueError(f"{field} must be a relative path")
    pieces = value.split("/")
    for piece in pieces:
        if piece in ("", ".", ".."):
            raise ValueError(
                f"{field} must not contain empty, '.' or '..' segments"
            )
        if piece.strip() != piece:
            raise ValueError(f"{field} must not contain whitespace-padded segments")
        if any(character in piece for character in "\x00\x7f"):
            raise ValueError(f"{field} must not contain control characters")
    return value


def _driver_name(value, field):
    _plain_text(value, field)
    if "\\" in value or "/" in value:
        raise ValueError(f"{field} must be a bare module name")
    if not _MODULE_BASENAME_RE.match(value):
        raise ValueError(
            f"{field} must use only ASCII letters, digits, '_' and '-'"
        )
    return value


def _firmware_sha(value, field):
    _plain_text(value, field)
    if not _HASH_RE.match(value):
        raise ValueError(f"{field} must be 64 lowercase hexadecimal characters")
    return value


def _string_list(value, field):
    if not isinstance(value, list):
        raise ValueError(f"{field} must be a JSON list of strings")
    for index, item in enumerate(value):
        _plain_text(item, f"{field}[{index}]")
    return value


def _validate_boot_requirements(model, boot):
    field_prefix = f"profiles.{model}.bootRequirements"
    if not isinstance(boot, dict):
        raise ValueError(f"{field_prefix} must be a JSON object")

    if "earlyFirmware" not in boot:
        raise ValueError(f"{field_prefix}.earlyFirmware is required")
    if "earlyDrivers" not in boot:
        raise ValueError(f"{field_prefix}.earlyDrivers is required")
    if "firmwareSha256" not in boot:
        raise ValueError(f"{field_prefix}.firmwareSha256 is required")

    firmware_raw = boot["earlyFirmware"]
    _string_list(firmware_raw, f"{field_prefix}.earlyFirmware")
    for index, item in enumerate(firmware_raw):
        _relative_firmware_path(item, f"{field_prefix}.earlyFirmware[{index}]")

    drivers_raw = boot["earlyDrivers"]
    _string_list(drivers_raw, f"{field_prefix}.earlyDrivers")
    for index, item in enumerate(drivers_raw):
        _driver_name(item, f"{field_prefix}.earlyDrivers[{index}]")

    sha_raw = boot["firmwareSha256"]
    if not isinstance(sha_raw, dict):
        raise ValueError(f"{field_prefix}.firmwareSha256 must be a JSON object")

    listed_firmware = set(firmware_raw)
    validated_sha = {}
    for firmware_path, digest in sha_raw.items():
        _relative_firmware_path(firmware_path, f"{field_prefix}.firmwareSha256 key")
        _firmware_sha(digest, f"{field_prefix}.firmwareSha256[{firmware_path!r}]")
        if firmware_path not in listed_firmware:
            raise ValueError(
                f"{field_prefix}.firmwareSha256 names unlisted firmware {firmware_path!r}"
            )
        validated_sha[firmware_path] = digest

    # Deterministic order; dedupe while preserving first occurrence.
    firmware_ordered = []
    for fw in firmware_raw:
        if fw not in firmware_ordered:
            firmware_ordered.append(fw)
    drivers_ordered = []
    for drv in drivers_raw:
        if drv not in drivers_ordered:
            drivers_ordered.append(drv)

    sha_ordered = {}
    for fw in firmware_ordered:
        if fw in validated_sha:
            sha_ordered[fw] = validated_sha[fw]

    return {
        "earlyFirmware": firmware_ordered,
        "earlyDrivers": drivers_ordered,
        "firmwareSha256": sha_ordered,
    }


def load_requirements(configuration, models=None):
    """Return per-selected-model validated boot requirements in document order."""
    if not isinstance(configuration, dict):
        raise ValueError("configuration must be a JSON object")
    profiles = configuration.get("profiles")
    if not isinstance(profiles, dict) or not profiles:
        raise ValueError("configuration.profiles must be a nonempty JSON object")

    if models is None:
        selected = list(profiles)
    else:
        try:
            requested = list(models)
        except TypeError as error:
            raise ValueError(
                "models must be a sequence of model identifiers"
            ) from error
        if not requested:
            raise ValueError("at least one model must be selected")
        seen = set()
        for item in requested:
            if not isinstance(item, str):
                raise ValueError(
                    f"model identifiers must be strings, got {type(item).__name__}"
                )
            if item not in profiles:
                raise ValueError(f"unknown model {item!r}")
            if item in seen:
                raise ValueError(f"model {item!r} selected more than once")
            seen.add(item)
        selected = [model for model in profiles if model in seen]

    result = {}
    for model in selected:
        profile = profiles[model]
        if not isinstance(profile, dict):
            raise ValueError(f"profile {model!r} must be a JSON object")
        if "bootRequirements" not in profile:
            raise ValueError(f"profiles.{model}.bootRequirements is required")
        result[model] = _validate_boot_requirements(model, profile["bootRequirements"])
    return result


def _check_relative_firmware(relative):
    if not isinstance(relative, str):
        raise ValueError("firmware path must be a string")
    if relative.startswith("/") or "\\" in relative:
        raise ValueError(f"firmware path {relative!r} must be relative")
    pieces = relative.split("/")
    for piece in pieces:
        if piece in ("", ".", ".."):
            raise ValueError(
                f"firmware path {relative!r} must not contain empty, '.' or '..' segments"
            )
    return relative


def resolve_firmware(root, relative):
    """Return Path to a nonempty usr/lib/firmware/<relative>[.xz] under root."""
    checked = _check_relative_firmware(relative)
    base = Path(root) / "usr" / "lib" / "firmware" / PurePosixPath(checked)
    raw = base
    compressed = base.with_name(base.name + ".xz")

    if raw.exists():
        if not raw.is_file():
            raise ValueError(f"firmware {checked!r} exists but is not a file")
        if raw.stat().st_size <= 0:
            raise ValueError(f"firmware {checked!r} is empty")
        return raw
    if compressed.exists():
        if not compressed.is_file():
            raise ValueError(f"firmware {checked!r}.xz exists but is not a file")
        if compressed.stat().st_size <= 0:
            raise ValueError(f"firmware {checked!r}.xz is empty")
        return compressed
    raise ValueError(f"firmware {checked!r} is missing (tried raw and .xz)")


def _normalize_module_name(name):
    return name.replace("-", "_")


def _modules_builtin_contains(builtin_path, module_name):
    wanted = _normalize_module_name(module_name)
    try:
        with open(builtin_path, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                stripped = line.strip()
                if not stripped or stripped.startswith("#"):
                    continue
                entry = stripped.split()[0]
                if entry.endswith(".ko"):
                    entry = entry[:-3]
                candidate = entry.rsplit("/", 1)[-1]
                if _normalize_module_name(candidate) == wanted:
                    return True
    except OSError:
        return False
    return False


def _module_present(root, kver, module_name):
    modules_root = Path(root) / "usr" / "lib" / "modules" / kver
    if modules_root.is_dir():
        builtin_path = modules_root / "modules.builtin"
        if builtin_path.is_file() and _modules_builtin_contains(builtin_path, module_name):
            return True

        kernel_dir = modules_root / "kernel"
        if kernel_dir.is_dir():
            wanted = _normalize_module_name(module_name)
            for path in kernel_dir.rglob("*"):
                if not path.is_file():
                    continue
                filename = path.name
                stem = None
                for suffix in _DRIVER_SUFFIXES:
                    if filename.endswith(suffix):
                        stem = filename[: -len(suffix)]
                        break
                if stem is None:
                    continue
                if _normalize_module_name(stem) == wanted:
                    try:
                        if path.stat().st_size <= 0:
                            continue
                    except OSError:
                        continue
                    return True
    return False


def _read_firmware_bytes(path):
    data = Path(path).read_bytes()
    lowered = path.name.lower()
    if lowered.endswith(".xz"):
        return lzma.decompress(data)
    return data


def verify_tree(root, kver, requirements):
    """Verify requirements against an extracted tree; raise ValueError if failing."""
    _plain_text(kver, "kver")
    if not isinstance(requirements, dict) or not requirements:
        raise ValueError("requirements must be a nonempty mapping")

    root_path = Path(root)
    if not root_path.is_dir():
        raise ValueError(f"root {root!r} is not a directory")

    errors = []
    for model, req in requirements.items():
        if not isinstance(req, dict):
            raise ValueError(f"requirements for {model!r} must be a mapping")
        firmware = req.get("earlyFirmware", [])
        drivers = req.get("earlyDrivers", [])
        sha_map = req.get("firmwareSha256", {})

        for firmware_path in firmware:
            try:
                resolved = resolve_firmware(root_path, firmware_path)
            except (ValueError, OSError) as error:
                errors.append(f"{model}: firmware: {error}")
                continue
            try:
                payload = _read_firmware_bytes(resolved)
            except (OSError, lzma.LZMAError) as error:
                errors.append(
                    f"{model}: firmware {firmware_path!r} could not be decompressed: {error}"
                )
                continue
            if not payload:
                errors.append(
                    f"{model}: firmware {firmware_path!r} decompresses to an empty payload"
                )
                continue
            pinned = sha_map.get(firmware_path)
            if pinned is not None:
                digest = hashlib.sha256(payload).hexdigest()
                if digest != pinned:
                    errors.append(
                        f"{model}: firmware {firmware_path!r} sha256 mismatch "
                        f"(expected {pinned}, got {digest})"
                    )

        for module_name in drivers:
            try:
                present = _module_present(root_path, kver, module_name)
            except OSError as error:
                errors.append(f"{model}: driver {module_name!r}: {error}")
                continue
            if not present:
                errors.append(
                    f"{model}: driver {module_name!r} missing under usr/lib/modules/{kver}"
                )

    if errors:
        raise ValueError("; ".join(errors))
    return None


def verify_initramfs(image, kver, requirements, work_parent=None):
    """Unpack *image* with lsinitrd into a temporary dir and verify the tree."""
    image_path = Path(image)
    if not image_path.is_absolute():
        image_path = image_path.resolve()
    if not image_path.is_file():
        raise ValueError(f"initramfs image {str(image_path)!r} is not a file")
    if image_path.stat().st_size <= 0:
        raise ValueError(f"initramfs image {str(image_path)!r} is empty")

    temp_kwargs = {}
    if work_parent is not None:
        parent = Path(work_parent)
        if not parent.is_dir():
            raise ValueError(f"work-dir {str(parent)!r} is not a directory")
        temp_kwargs["dir"] = str(parent)

    with tempfile.TemporaryDirectory(**temp_kwargs) as workdir:
        command = ["lsinitrd", "--unpack", str(image_path)]
        try:
            subprocess.run(
                command,
                cwd=workdir,
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except FileNotFoundError as error:
            raise ValueError("lsinitrd executable not found") from error
        except subprocess.CalledProcessError as error:
            stderr = error.stderr or b""
            if isinstance(stderr, bytes):
                stderr = stderr.decode("utf-8", "replace")
            stderr = stderr.strip()
            detail = f": {stderr}" if stderr else ""
            raise ValueError(
                f"lsinitrd failed (status {error.returncode}){detail}"
            ) from error

        verify_tree(Path(workdir), kver, requirements)
    return None


def _collect_firmware_paths(root, requirements):
    seen = set()
    ordered = []
    for model in requirements:
        for relative in requirements[model]["earlyFirmware"]:
            path = resolve_firmware(root, relative)
            # Use the lexically-absolute requested path so firmware symlink
            # aliases preserve the requested name for dracut --install.
            lexical = os.path.abspath(str(path))
            if lexical not in seen:
                seen.add(lexical)
                ordered.append(lexical)
    return ordered


def _collect_driver_names(requirements):
    seen = set()
    ordered = []
    for model in requirements:
        for driver in requirements[model]["earlyDrivers"]:
            if driver not in seen:
                seen.add(driver)
                ordered.append(driver)
    return ordered


def _import_device_config_module():
    script_dir = Path(__file__).resolve().parent
    if str(script_dir) not in sys.path:
        sys.path.insert(0, str(script_dir))
    import edl_device_config  # noqa: WPS433 (deliberate local import)

    return edl_device_config


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Verify EDL boot requirements for a built tree and initramfs."
    )
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--kver", required=True, type=str)
    parser.add_argument("--model", action="append", metavar="ID")
    parser.add_argument(
        "--format", required=True, choices=("firmware", "drivers", "verify")
    )
    parser.add_argument("--initrd", type=Path)
    parser.add_argument("--work-dir", type=Path)
    arguments = parser.parse_args(argv)

    try:
        device_config = _import_device_config_module()
        configuration = device_config.load_configuration(arguments.config)
        requirements = load_requirements(configuration, arguments.model)

        verify_tree(arguments.root, arguments.kver, requirements)

        if arguments.format == "verify":
            if arguments.initrd is not None:
                verify_initramfs(
                    arguments.initrd,
                    arguments.kver,
                    requirements,
                    work_parent=arguments.work_dir,
                )
            return 0

        if arguments.format == "firmware":
            for path in _collect_firmware_paths(arguments.root, requirements):
                print(path)
            return 0

        if arguments.format == "drivers":
            for name in _collect_driver_names(requirements):
                print(name)
            return 0

        raise ValueError(f"unsupported format {arguments.format!r}")
    except (OSError, ValueError, lzma.LZMAError, subprocess.SubprocessError) as error:
        print(f"edl_boot_requirements: error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
