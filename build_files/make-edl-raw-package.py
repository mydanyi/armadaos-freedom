#!/usr/bin/env python3
"""Build the direct EDL raw filesystem package bundle.

The producer validates the build options and the source checkout revision,
pulls a linux/arm64 OCI image, verifies the image provenance against that
revision and version, deploys the image into a real ext4 boot plus btrfs root
filesystem pair, derives the per-model device trees and publishes a ZIP64
bundle with per-model manifests and an internal build report. Filesystem
creation and mounting stay in filesystem_pair; pure validation never performs
block or filesystem I/O.
"""

from __future__ import annotations

import argparse
import contextlib
import gzip
import hashlib
import importlib.machinery
import importlib.util
import json
import os
import re
import stat
import subprocess
import sys
import uuid
import zipfile
from pathlib import Path

GIB = 1 << 30

ROOT_IMAGE_GIB_CHOICES = (32, 48)
BOOT_IMAGE_BYTES = 1 * GIB
MINIMUM_ROOT_PARTITION_BYTES = 63 * GIB
BACKUP_PARTITION_BYTES = 2 * GIB
MINIMUM_DATA_BYTES = 8 * GIB
SECTOR_SIZE = 4096
DEFAULT_ENTRY = "android"
RUNTIME_BYTES = 4 * (1 << 20)
ARM64_MAGIC = b"ARM\x64"

DIAGNOSTIC_VERBOSE_TAIL = (
    "loglevel=8",
    "ignore_loglevel",
    "printk.time=1",
    "initcall_debug",
    "rd.debug",
)
DEFAULT_DEVICES_CONFIG = Path(__file__).resolve().with_name("edl-devices.json")

# Filesystem creation and deployment stay model-independent; only compatibility
# tags, device-tree choice, the extra board arguments and the package identity
# differ. Those facts come from the declarative device configuration beside this
# producer, loaded through the sibling edl_device_config module.
COMMON_PAYLOADS = (
    ("common/boot/kernel.img", "boot/kernel.img"),
    ("common/boot/initramfs.img", "boot/initramfs.img"),
    ("common/images/linux-boot.ext4.gz", "images/linux-boot.ext4.gz"),
    ("common/images/linux-root.btrfs.gz", "images/linux-root.btrfs.gz"),
)

_GUID_D = re.compile(
    r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}",
    re.IGNORECASE,
)
_HEX64 = re.compile(r"[0-9a-f]{64}")
_COMMIT40 = re.compile(r"[0-9a-f]{40}")
_SHA256_DIGEST = re.compile(r"sha256:[0-9a-f]{64}")
_OSTREE_POINTER = re.compile(r"ostree=/ostree/boot\.[01]/default/([0-9a-f]{64})/0")

_RESULT_KEYS = (
    "deployment_checksum",
    "filesystem_uuids",
    "bls",
    "image",
    "initrd",
    "fdtdir",
    "options",
)
_RESULT_ASSETS = ("bls", "image", "initrd", "fdtdir")
_BLS_REQUIRED = ("linux", "initrd", "fdtdir", "options")
_BTRFS_MOUNT_OPTIONS = "noatime,compress=zstd:1,discard=async"
_BOOT_MOUNT_OPTIONS = "noatime"
_IO_CHUNK = 1 << 20
_MAX_OPTIONS_BYTES = 4096


def _as_int(value, name):
    if not isinstance(value, int) or isinstance(value, bool):
        raise ValueError(f"{name} must be an integer")
    return value


def _canonical_uuid(value, name):
    if not isinstance(value, str) or not _GUID_D.fullmatch(value):
        raise ValueError(f"{name} must be a canonical GUID-D string")
    parsed = uuid.UUID(value)
    if parsed.int == 0:
        raise ValueError(f"{name} must not be the nil UUID")
    return str(parsed)


def _require_hex64(value, name):
    if not isinstance(value, str) or not _HEX64.fullmatch(value):
        raise ValueError(f"{name} must be a lowercase 64-character hex checksum")
    return value


def _safe_relative(value, name):
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a non-empty relative path")
    if "\x00" in value or "\\" in value or value.startswith("/"):
        raise ValueError(f"{name} must be a safe relative path")
    if any(part in ("", ".", "..") for part in value.split("/")):
        raise ValueError(f"{name} must be a safe relative path")
    return value


def _single_line_options(text):
    if not isinstance(text, str):
        raise ValueError("options must be a string")
    if not text or "\x00" in text or "\n" in text or "\r" in text:
        raise ValueError("options must be one non-empty line without NUL")
    if len(text.encode("utf-8")) > _MAX_OPTIONS_BYTES:
        raise ValueError(f"options exceed {_MAX_OPTIONS_BYTES} UTF-8 bytes")
    return text


def _unique_option(tokens, name):
    found = [token for token in tokens if token.split("=", 1)[0] == name]
    if len(found) != 1:
        raise ValueError(f"expected exactly one {name}= option, found {len(found)}")
    return found[0]


def _parse_bls(text):
    if not isinstance(text, str) or not text.strip():
        raise ValueError("BLS text is empty")
    fields = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(None, 1)
        key = parts[0]
        value = parts[1].strip() if len(parts) > 1 else ""
        if key in _BLS_REQUIRED:
            if key in fields:
                raise ValueError(f"duplicate BLS {key} field")
            fields[key] = value
    missing = [key for key in _BLS_REQUIRED if key not in fields]
    if missing:
        raise ValueError(f"BLS text is missing {missing}")
    return fields


def validate_build_options(root_gib, boot_uuid, root_uuid, source_commit):
    """Return normalized build options or raise ValueError.

    root_gib is exactly 32 or 48. boot_uuid/root_uuid are distinct, nonzero,
    canonical GUID-D strings. source_commit is the actual nonzero lowercase
    40-hex source revision and is returned verbatim. The returned mapping is
    {rootBytes, bootBytes, bootUuid, rootUuid, sourceCommit,
    minimumRootPartitionBytes, backupBytes}.
    """
    size = _as_int(root_gib, "root_gib")
    if size not in ROOT_IMAGE_GIB_CHOICES:
        raise ValueError("root image size must be exactly 32 or 48 GiB")
    boot = _canonical_uuid(boot_uuid, "boot_uuid")
    root = _canonical_uuid(root_uuid, "root_uuid")
    if boot == root:
        raise ValueError("boot and root UUIDs must differ")
    if not isinstance(source_commit, str) or not _COMMIT40.fullmatch(source_commit):
        raise ValueError("source_commit must be a lowercase 40-character hex revision")
    if not source_commit.strip("0"):
        raise ValueError("source_commit must not be the all-zero revision")
    return {
        "rootBytes": size * GIB,
        "bootBytes": BOOT_IMAGE_BYTES,
        "bootUuid": boot,
        "rootUuid": root,
        "sourceCommit": source_commit,
        "minimumRootPartitionBytes": MINIMUM_ROOT_PARTITION_BYTES,
        "backupBytes": BACKUP_PARTITION_BYTES,
    }


def validate_image_provenance(
    config, actual_manifest_digest, expected_manifest_digest, source_commit, version
):
    """Return None when the OCI image proves the expected source build.

    config must declare linux/arm64. Both manifest digests must be
    sha256:<64 lowercase hex> and equal. config['config']['Labels'] must be a
    mapping whose revision label is exactly source_commit and whose version
    label is exactly version, which must itself be non-empty and not latest.
    Raises ValueError otherwise; the given config is never mutated.
    """
    if not isinstance(config, dict):
        raise ValueError("OCI image config must be a mapping")
    if config.get("architecture") != "arm64" or config.get("os") != "linux":
        raise ValueError("OCI image must be linux/arm64")
    if not isinstance(actual_manifest_digest, str) or not _SHA256_DIGEST.fullmatch(
        actual_manifest_digest
    ):
        raise ValueError("actual manifest digest must be sha256:<64 lowercase hex>")
    if not isinstance(expected_manifest_digest, str) or not _SHA256_DIGEST.fullmatch(
        expected_manifest_digest
    ):
        raise ValueError("expected manifest digest must be sha256:<64 lowercase hex>")
    if actual_manifest_digest != expected_manifest_digest:
        raise ValueError("OCI manifest digest does not match the expected digest")
    image_config = config.get("config")
    labels = image_config.get("Labels") if isinstance(image_config, dict) else None
    if not isinstance(labels, dict):
        raise ValueError("OCI image config labels are missing or malformed")
    if labels.get("org.opencontainers.image.revision") != source_commit:
        raise ValueError("OCI image revision label does not match the source commit")
    if not isinstance(version, str) or not version or version == "latest":
        raise ValueError("version must be non-empty and must not be latest")
    if labels.get("org.opencontainers.image.version") != version:
        raise ValueError("OCI image version label does not match the version")
    return None


def parse_deployment_result(result, bls_text):
    """Derive boot compatibility facts from a deployment result plus BLS text.

    Returns {bootUuid, rootUuid, normalBootArgs, diagnosticBootArgs,
    bootChecksum, defaultEntry}. bootChecksum comes from the single
    ostree=/ostree/boot.[01]/default/<64hex>/0 pointer and is independent of
    result['deployment_checksum']; the two are never interchangeable. The
    fixed filesystem UUIDs come from result['filesystem_uuids'] ('boot' and
    'root'); result['options'] is the full already-joined string of kernel
    arguments. Options must carry exactly one root=UUID (rootUuid) and one
    boot=UUID (bootUuid) matching the fixed filesystem UUIDs. Ambiguous,
    duplicated or mismatched UUID/pointer data raises ValueError.
    """
    if not isinstance(result, dict):
        raise ValueError("deployment result must be a mapping")
    missing = [key for key in _RESULT_KEYS if key not in result]
    if missing:
        raise ValueError(f"deployment result is missing {missing}")

    _require_hex64(result["deployment_checksum"], "deployment_checksum")

    uuids = result["filesystem_uuids"]
    if not isinstance(uuids, dict) or "boot" not in uuids or "root" not in uuids:
        raise ValueError("filesystem_uuids must carry boot and root entries")
    boot_uuid = _canonical_uuid(uuids["boot"], "filesystem_uuids.boot")
    root_uuid = _canonical_uuid(uuids["root"], "filesystem_uuids.root")
    if boot_uuid == root_uuid:
        raise ValueError("boot and root UUIDs must differ")

    assets = {name: _safe_relative(result[name], name) for name in _RESULT_ASSETS}
    options = _single_line_options(result["options"])

    bls = _parse_bls(bls_text)
    for name in ("image", "initrd", "fdtdir"):
        if bls["linux" if name == "image" else name] != "/boot/" + assets[name]:
            raise ValueError(f"BLS {name} path must be /boot/<result {name}>")
    if bls["options"].split() != options.split():
        raise ValueError("result options and BLS options disagree")

    tokens = options.split()
    if _unique_option(tokens, "root") != f"root=UUID={root_uuid}":
        raise ValueError("root= must point at the fixed root filesystem UUID")
    if _unique_option(tokens, "boot") != f"boot=UUID={boot_uuid}":
        raise ValueError("boot= must point at the fixed boot filesystem UUID")

    pointers = [token for token in tokens if token.startswith("ostree=")]
    if len(pointers) != 1:
        raise ValueError(f"expected exactly one ostree= pointer, found {len(pointers)}")
    match = _OSTREE_POINTER.fullmatch(pointers[0])
    if match is None:
        raise ValueError("ostree= pointer must be /ostree/boot.[01]/default/<64hex>/0")

    normal = " ".join(tokens)
    return {
        "bootUuid": boot_uuid,
        "rootUuid": root_uuid,
        "normalBootArgs": normal,
        "diagnosticBootArgs": normal + " rd.break=pre-mount",
        "bootChecksum": match.group(1),
        "defaultEntry": DEFAULT_ENTRY,
    }


def validate_space(root_bytes, available_bytes, minimum_data_bytes=MINIMUM_DATA_BYTES):
    """Return used bytes (root_bytes - available_bytes) or raise ValueError.

    available_bytes is the actually free space reported for the target, so the
    return value is the space the deployment consumes. At least
    minimum_data_bytes must remain free, and available_bytes must fall within
    0..root_bytes; otherwise the candidate is rejected so no package metadata
    is published.
    """
    total = _as_int(root_bytes, "root_bytes")
    available = _as_int(available_bytes, "available_bytes")
    minimum = _as_int(minimum_data_bytes, "minimum_data_bytes")
    if total <= 0:
        raise ValueError("root_bytes must be positive")
    if minimum < 0:
        raise ValueError("minimum_data_bytes must not be negative")
    if available < 0 or available > total:
        raise ValueError("available_bytes must fall within 0..root_bytes")
    if available < minimum:
        raise ValueError("available space is below the required free space")
    return total - available


def gzip_image(source, destination):
    """Gzip a raw image with deterministic metadata and report both meanings.

    Returns {bytes, sha256, expandedBytes, expandedSha256}: the first pair is
    the compressed file on disk, the second the uncompressed stream. mtime is
    fixed at 0 so repeated runs over the same input are byte-identical.
    """
    source = Path(source)
    destination = Path(destination)
    try:
        source_mode = source.stat().st_mode
    except OSError as error:
        raise ValueError(f"source image is unreadable: {source}") from error
    if not stat.S_ISREG(source_mode):
        raise ValueError(f"source image is not a regular file: {source}")

    expanded = hashlib.sha256()
    expanded_bytes = 0
    created = False
    try:
        with open(source, "rb") as raw_source:
            raw_destination = open(destination, "xb")
            created = True
            with raw_destination:
                with gzip.GzipFile(
                    filename="", mode="wb", fileobj=raw_destination, mtime=0
                ) as stream:
                    while True:
                        chunk = raw_source.read(_IO_CHUNK)
                        if not chunk:
                            break
                        expanded.update(chunk)
                        expanded_bytes += len(chunk)
                        stream.write(chunk)

        compressed = hashlib.sha256()
        compressed_bytes = 0
        with open(destination, "rb") as compressed_file:
            while True:
                chunk = compressed_file.read(_IO_CHUNK)
                if not chunk:
                    break
                compressed.update(chunk)
                compressed_bytes += len(chunk)
    except BaseException:
        if created:
            with contextlib.suppress(OSError):
                destination.unlink()
        raise

    return {
        "bytes": compressed_bytes,
        "sha256": compressed.hexdigest(),
        "expandedBytes": expanded_bytes,
        "expandedSha256": expanded.hexdigest(),
    }


def _run(argv):
    return subprocess.run(
        [str(arg) for arg in argv],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    ).stdout


def validate_source_checkout(source, source_commit):
    """Return None when the checkout HEAD is exactly source_commit.

    source_commit must already be a validated revision; the git output is
    stripped and compared verbatim so the source tree bound into the package
    is the actual checkout, not a declared pin.
    """
    head = _run(["git", "-C", str(source), "rev-parse", "HEAD"]).strip()
    if head != source_commit:
        raise ValueError(
            f"source checkout HEAD {head!r} does not match {source_commit!r}"
        )
    return None


def _create_image(path, size):
    descriptor = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        os.ftruncate(descriptor, size)
    finally:
        os.close(descriptor)


def _attach_loop(image):
    device = _run(["losetup", "--find", "--show", str(image)]).strip()
    if not device.startswith("/dev/loop"):
        raise RuntimeError(f"losetup returned an unexpected device: {device!r}")
    return device


def _mount(source, directory, options, mounts):
    argv = ["mount"]
    if options:
        argv.extend(["-o", options])
    argv.extend([str(source), str(directory)])
    _run(argv)
    mounts.append(str(directory))


def _unmount(point, mounts):
    _run(["umount", str(point)])
    for index in range(len(mounts) - 1, -1, -1):
        if mounts[index] == str(point):
            del mounts[index]
            break


def _teardown(mounts, loops):
    while mounts:
        point = mounts[-1]
        try:
            _run(["umount", point])
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f"cleanup retained mount {point}: {error}") from error
        mounts.pop()
    while loops:
        device = loops[-1]
        try:
            _run(["losetup", "-d", device])
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f"cleanup retained loop {device}: {error}") from error
        loops.pop()


@contextlib.contextmanager
def filesystem_pair(work, boot_bytes, root_bytes, boot_uuid, root_uuid):
    """Create, mount and tear down one real file-backed ext4+btrfs pair.

    Yields {target, top, boot_image, root_image, boot_loop, root_loop}.
    boot_image is mkfs.ext4 with label "boot" and boot_uuid; root_image is
    mkfs.btrfs with label "root" and root_uuid. The root filesystem carries
    root, var and home subvolumes; target/boot, target/var and
    target/var/home are mounted separately. Cleanup unmounts and detaches in
    reverse order and only touches loops/mounts created here; the raw image
    files persist afterwards. work must be a new execution directory.
    """
    work = Path(work)
    if not work.is_dir():
        raise ValueError(f"work directory does not exist: {work}")
    if any(work.iterdir()):
        raise ValueError(f"work directory is not fresh: {work}")
    boot_size = _as_int(boot_bytes, "boot_bytes")
    root_size = _as_int(root_bytes, "root_bytes")
    if boot_size <= 0 or root_size <= 0:
        raise ValueError("image sizes must be positive")
    boot_id = _canonical_uuid(boot_uuid, "boot_uuid")
    root_id = _canonical_uuid(root_uuid, "root_uuid")
    if boot_id == root_id:
        raise ValueError("boot and root UUIDs must differ")

    top = work / "top"
    target = work / "target"
    boot_image = work / "boot.img"
    root_image = work / "root.img"
    top.mkdir()
    target.mkdir()
    _create_image(boot_image, boot_size)
    _create_image(root_image, root_size)
    _run(
        [
            "mkfs.ext4",
            "-F",
            "-b",
            SECTOR_SIZE,
            "-L",
            "boot",
            "-U",
            boot_id,
            boot_image,
        ]
    )
    _run(
        [
            "mkfs.btrfs",
            "-f",
            "-s",
            SECTOR_SIZE,
            "-L",
            "root",
            "-U",
            root_id,
            root_image,
        ]
    )

    mounts = []
    loops = []
    try:
        boot_loop = _attach_loop(boot_image)
        loops.append(boot_loop)
        root_loop = _attach_loop(root_image)
        loops.append(root_loop)

        _mount(root_loop, top, _BTRFS_MOUNT_OPTIONS, mounts)
        for name in ("root", "var", "home"):
            _run(["btrfs", "subvolume", "create", top / name])
        _unmount(top, mounts)

        _mount(root_loop, target, _BTRFS_MOUNT_OPTIONS + ",subvol=root", mounts)
        (target / "var").mkdir()
        _mount(
            root_loop, target / "var", _BTRFS_MOUNT_OPTIONS + ",subvol=var", mounts
        )
        (target / "var/home").mkdir()
        _mount(
            root_loop,
            target / "var/home",
            _BTRFS_MOUNT_OPTIONS + ",subvol=home",
            mounts,
        )
        (target / "boot").mkdir()
        _mount(boot_loop, target / "boot", _BOOT_MOUNT_OPTIONS, mounts)

        yield {
            "target": str(target),
            "top": str(top),
            "boot_image": str(boot_image),
            "root_image": str(root_image),
            "boot_loop": boot_loop,
            "root_loop": root_loop,
        }
    finally:
        _teardown(mounts, loops)


def _sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        while True:
            chunk = stream.read(_IO_CHUNK)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def _load_module(name, path):
    loader = importlib.machinery.SourceFileLoader(name, str(path))
    spec = importlib.util.spec_from_loader(name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


_DEVICE_CONFIG_MODULE = None
_DEFAULT_CONFIGURATION = None


def _device_config_module():
    """Return the sibling edl_device_config module loaded without sys.path edits."""
    global _DEVICE_CONFIG_MODULE
    if _DEVICE_CONFIG_MODULE is None:
        _DEVICE_CONFIG_MODULE = _load_module(
            "edl_device_config_sibling",
            Path(__file__).resolve().with_name("edl_device_config.py"),
        )
    return _DEVICE_CONFIG_MODULE


def _default_configuration():
    """Load the default edl-devices.json beside this producer exactly once."""
    global _DEFAULT_CONFIGURATION
    if _DEFAULT_CONFIGURATION is None:
        _DEFAULT_CONFIGURATION = _device_config_module().load_configuration(
            DEFAULT_DEVICES_CONFIG
        )
    return _DEFAULT_CONFIGURATION


PROFILES = _default_configuration()["profiles"]
PROFILE_ORDER = tuple(_device_config_module().select_models(_default_configuration()))


def _write_json(path, payload):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    path.write_text(text, encoding="utf-8")


def _fresh_directory(path):
    path = Path(path)
    if path.exists():
        if not path.is_dir():
            raise ValueError(f"path is not a directory: {path}")
        if any(path.iterdir()):
            raise ValueError(f"directory is not fresh: {path}")
    else:
        path.mkdir(parents=True)
    return path


def _parse_runtimes(values, profiles=None):
    """Return {model: runtime facts} for the selected profile mapping.

    profiles maps the already-selected model identifiers to their profiles; it
    defaults to the derived checked-in profiles. A value is either an explicit
    MODEL=PATH whose MODEL must be selected or a bare PATH, which is only
    unambiguous when exactly one model is selected. Repeated models are
    rejected. Every runtime must be a regular file of exactly RUNTIME_BYTES.
    """
    if profiles is None:
        profiles = PROFILES
    selected = {}
    for value in values:
        candidate, separator, remainder = value.partition("=")
        if separator:
            if candidate not in profiles:
                raise ValueError(
                    f"runtime names unknown model {candidate!r}; expected one of "
                    f"{sorted(profiles)}"
                )
            model, text = candidate, remainder
        else:
            if len(profiles) != 1:
                raise ValueError(
                    "a bare --runtime PATH needs exactly one selected model; "
                    "use --runtime MODEL=PATH"
                )
            model, text = next(iter(profiles)), value
        if model in selected:
            raise ValueError(f"runtime for {model} was supplied more than once")
        path = Path(text)
        if not path.is_file():
            raise ValueError(f"runtime is not a regular file: {path}")
        size = path.stat().st_size
        if size != RUNTIME_BYTES:
            raise ValueError(f"runtime {path} must be exactly {RUNTIME_BYTES} bytes")
        selected[model] = {
            "path": str(path.resolve()),
            "bytes": size,
            "sha256": _sha256_file(path),
        }
    if not selected:
        raise ValueError("at least one --runtime PATH or --runtime MODEL=PATH is required")
    return selected


def _requested_models(runtimes, requested=None, configuration=None):
    """Return the selected models in configuration order after exact checks.

    requested is forwarded to select_models; None selects every configured
    profile. The supplied runtime keys must equal the selected model set, so
    both a missing and an extra runtime are rejected before any deployment work.
    """
    if configuration is None:
        configuration = _default_configuration()
    models = tuple(_device_config_module().select_models(configuration, requested))
    supplied = set(runtimes)
    expected = set(models)
    if supplied != expected:
        detail = []
        missing = [model for model in models if model not in supplied]
        extra = [model for model in runtimes if model not in expected]
        if missing:
            detail.append(f"missing runtimes for {missing}")
        if extra:
            detail.append(f"unexpected runtimes for {extra}")
        raise ValueError(
            "runtime selection does not match the requested models: "
            + "; ".join(detail)
        )
    return models


def _model_boot_args(normal, profile):
    tokens = normal.split()
    for extra in profile["extraArgs"]:
        key = extra.split("=", 1)[0]
        present = [token for token in tokens if token.split("=", 1)[0] == key]
        if present:
            if any(token != extra for token in present):
                raise ValueError(f"deployment already passes a conflicting {key}= option")
            continue
        tokens.append(extra)
    return " ".join(tokens)


def _diagnostic_boot_args(normal, profile):
    """Return the diagnostic kernel command line for one profile mode.

    rd-break simply appends the pre-mount break. verbose keeps every normal
    token (including duplicates and original order) except the quiet switch and
    all loglevel=/rd.break switches, then appends the missing verbose tail in
    its fixed order.
    """
    mode = profile["diagnosticMode"]
    if mode == "rd-break":
        return normal + " rd.break=pre-mount"
    if mode != "verbose":
        raise ValueError(f"unsupported diagnosticMode {mode!r}")
    tokens = []
    for token in normal.split():
        if token == "quiet":
            continue
        if token.startswith("loglevel="):
            continue
        if token == "rd.break" or token.startswith("rd.break="):
            continue
        tokens.append(token)
    for token in DIAGNOSTIC_VERBOSE_TAIL:
        if token not in tokens:
            tokens.append(token)
    return " ".join(tokens)


def _boot_compatibility(profile):
    """Return the manifest boot compatibility facts declared by one profile."""
    return {
        "format": "android-boot-v4",
        "partitionBytes": profile["bootPartitionBytes"],
    }


def _resolve_dtb(fdtdir, profile, model):
    dtb = profile["dtb"]
    if not isinstance(dtb, str) or not dtb:
        raise ValueError(f"{model}: profile does not declare a device tree")
    path = Path(fdtdir) / dtb
    if not path.is_file():
        raise ValueError(f"{model}: missing deployment device tree {path}")
    return path


def _prepare_dtb_module(source, work):
    module = _load_module(
        "armada_kshim_build_boot",
        Path(source) / "system_files/usr/libexec/armada/kshim-build-boot.py",
    )
    base = Path(work) / "dtb"
    module.WORK = str(base / "work")
    module.LOGS = str(base / "logs")
    module.TMP = str(base / "tmp")
    for path in (module.WORK, module.LOGS, module.TMP):
        Path(path).mkdir(parents=True)
    module.ENV = dict(os.environ)
    module.ENV["PYTHONDONTWRITEBYTECODE"] = "1"
    module.ENV["TMPDIR"] = module.TMP
    module.ENV["TEMP"] = module.TMP
    module.ENV["TMP"] = module.TMP
    module.CHECKS = []
    module.COMMANDS = []
    for tool in ("fdtget", "fdtput", "dtc"):
        module.require_tool(tool)
    return module


def _build_dtb(module, dtb, bootargs, stage, tag, destination):
    derived, data, _changed, _dts = module.build_dtb(
        {"dtb": str(dtb)},
        bootargs,
        boot_stage=stage,
        root_medium="internal",
        initrd_index=5,
        tag=tag,
    )
    if not data:
        raise ValueError(f"derived device tree is empty: {derived}")
    destination = Path(destination)
    with open(destination, "xb") as stream:
        stream.write(data)


def _copy_payload(source, destination, require_arm64=False):
    source = Path(source)
    destination = Path(destination)
    if not source.is_file():
        raise ValueError(f"payload is not a regular file: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    length = 0
    with open(source, "rb") as reader, open(destination, "xb") as writer:
        while True:
            chunk = reader.read(_IO_CHUNK)
            if not chunk:
                break
            digest.update(chunk)
            length += len(chunk)
            writer.write(chunk)
    if length <= 0:
        raise ValueError(f"payload is empty: {source}")
    if require_arm64:
        with open(destination, "rb") as stream:
            header = stream.read(64)
        if len(header) < 64 or header[56:60] != ARM64_MAGIC:
            raise ValueError(f"kernel is not a raw ARM64 Image: {source}")
    return {"bytes": length, "sha256": digest.hexdigest()}


def _selected_deployment(target, commit):
    root = Path(target) / "ostree/deploy/default/deploy"
    if not root.is_dir():
        raise ValueError(f"deployment directory is missing: {root}")
    pattern = re.compile(re.escape(commit) + r"\.[0-9]+")
    matches = [path for path in root.iterdir() if path.is_dir() and pattern.fullmatch(path.name)]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one deployment for {commit}, found {len(matches)}")
    return matches[0]


def _verify_frozen_overlay(source, deployment):
    pairs = (
        ("usr/libexec/armada/armada-edl-grow-root", 0o755),
        ("usr/lib/systemd/system/armada-edl-root-grow.service", 0o644),
    )
    for relative, mode in pairs:
        expected = Path(source) / "system_files" / relative
        deployed = Path(deployment) / relative
        if not expected.is_file() or not deployed.is_file():
            raise ValueError(f"frozen file is missing: {deployed}")
        if _sha256_file(expected) != _sha256_file(deployed):
            raise ValueError(f"deployed content does not match the source overlay: {relative}")
        if stat.S_IMODE(deployed.stat().st_mode) != mode:
            raise ValueError(f"deployed mode does not match the source overlay: {relative}")


def _verify_plasma_packages(deployment):
    helper_path = Path(__file__).resolve().with_name('verify_plasma_runtime.py')
    module = _load_module('verify_plasma_runtime_sibling', helper_path)
    module.verify_packages(Path(deployment))


def _verify_model_boot_requirements(deployment, initrd, configuration, models, work):
    helper_path = Path(__file__).resolve().with_name("edl_boot_requirements.py")
    module = _load_module('edl_boot_requirements_sibling', helper_path)
    modules_dir = Path(deployment) / "usr/lib/modules"
    kvers = [path for path in modules_dir.iterdir() if path.is_dir()] if modules_dir.is_dir() else []
    if len(kvers) != 1:
        raise ValueError(
            f"expected exactly one kernel version under {modules_dir}, found {len(kvers)}"
        )
    kver = kvers[0].name
    req = module.load_requirements(configuration, models)
    module.verify_tree(deployment, kver, req)
    module.verify_initramfs(initrd, kver, req, work_parent=work)


def _write_deployment_markers(target, deployment, build):
    etc = Path(deployment) / "etc"
    marker = {
        "schema": 1,
        "bootUuid": build["bootUuid"],
        "rootUuid": build["rootUuid"],
        "minimumRootPartitionBytes": build["minimumRootPartitionBytes"],
        "backupBytes": build["backupBytes"],
    }
    _write_json(etc / "armada/edl-direct.json", marker)
    unit = Path(deployment) / "usr/lib/systemd/system/armada-edl-root-grow.service"
    if not unit.is_file():
        raise ValueError(f"deployed growth unit is missing: {unit}")
    wants = etc / "systemd/system/multi-user.target.wants"
    wants.mkdir(parents=True, exist_ok=True)
    wanted = wants / "armada-edl-root-grow.service"
    if wanted.exists() or wanted.is_symlink():
        wanted.unlink()
    wanted.symlink_to("/usr/lib/systemd/system/armada-edl-root-grow.service")
    growpart = Path(deployment) / "usr/lib/systemd/system/bootc-generic-growpart.service"
    if not growpart.is_file():
        raise ValueError(f"deployed growpart unit is missing: {growpart}")
    mask = etc / "systemd/system/bootc-generic-growpart.service"
    if mask.exists() or mask.is_symlink():
        mask.unlink()
    mask.symlink_to("/dev/null")
    if (Path(target) / "var/lib/armada/edl-root-expanded").exists():
        raise ValueError("deployment unexpectedly carries the growth success state")


def _probe_image(path, fs_type, fs_uuid):
    fields = {}
    for line in _run(["blkid", "-p", "-o", "export", str(path)]).splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            fields[key] = value.strip('"')
    if fields.get("TYPE") != fs_type:
        raise ValueError(f"{path} is not {fs_type}")
    if fields.get("UUID", "").lower() != fs_uuid.lower():
        raise ValueError(f"{path} does not carry the fixed filesystem UUID")


def _artifact(staging, role, file, location, extra=None):
    path = Path(staging) / location
    entry = {
        "role": role,
        "file": file,
        "bytes": path.stat().st_size,
        "sha256": _sha256_file(path),
    }
    if extra:
        entry.update(extra)
    return entry


def _create_archive(root, destination):
    root = Path(root)
    destination = Path(destination)
    if destination.exists():
        raise ValueError(f"archive already exists: {destination}")
    names = []
    try:
        with zipfile.ZipFile(
            destination, "x", compression=zipfile.ZIP_STORED, allowZip64=True
        ) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_dir():
                    continue
                if path.is_symlink() or not stat.S_ISREG(path.lstat().st_mode):
                    raise ValueError(f"archive payload is not a regular file: {path}")
                name = path.relative_to(root).as_posix()
                _safe_relative(name, "archive entry")
                names.append(name)
                archive.write(path, arcname=name, compress_type=zipfile.ZIP_STORED)
        if len(set(names)) != len(names):
            raise ValueError("archive entries are not unique")
        with zipfile.ZipFile(destination, "r") as archive:
            infos = archive.infolist()
            if len(infos) != len(names):
                raise ValueError("archive entry count changed during publication")
            for info in infos:
                if info.filename not in names:
                    raise ValueError(f"unexpected archive entry: {info.filename}")
                if info.compress_type != zipfile.ZIP_STORED:
                    raise ValueError(f"archive entry is not stored: {info.filename}")
                if (root / info.filename).stat().st_size != info.file_size:
                    raise ValueError(f"archive entry length mismatch: {info.filename}")
        return {"bytes": destination.stat().st_size, "sha256": _sha256_file(destination)}
    except BaseException:
        with contextlib.suppress(OSError):
            destination.unlink()
        raise


def _produce(args):
    if not args.version or args.version == "latest":
        raise ValueError("version must be non-empty and must not be latest")
    if not isinstance(args.manifest_digest, str) or not _SHA256_DIGEST.fullmatch(
        args.manifest_digest
    ):
        raise ValueError("manifest digest must be sha256:<64 lowercase hex>")
    build = validate_build_options(
        args.root_image_gib, args.boot_uuid, args.root_uuid, args.source_commit
    )
    source = Path(args.source).resolve()
    validate_source_checkout(source, build["sourceCommit"])
    installer_path = source / "system_files/usr/libexec/armada/armada-installer"
    kshim_path = source / "system_files/usr/libexec/armada/kshim-build-boot.py"
    for path in (installer_path, kshim_path):
        if not path.is_file():
            raise ValueError(f"source file is missing: {path}")
    configuration = _device_config_module().load_configuration(args.devices_config)
    models = tuple(_device_config_module().select_models(configuration, args.model))
    profiles = {name: configuration["profiles"][name] for name in models}
    runtimes = _parse_runtimes(args.runtime, profiles)
    models = _requested_models(runtimes, args.model, configuration)
    work = _fresh_directory(args.work)
    out = _fresh_directory(args.out)
    if work == out:
        raise ValueError("--work and --out must be distinct directories")
    staging = work / "bundle"
    for relative in ("common/boot", "common/images", "models"):
        (staging / relative).mkdir(parents=True, exist_ok=True)

    repo_dir = work / "ostree-repo"
    _run(["ostree", "init", f"--repo={repo_dir}", "--mode=bare"])
    _run(
        [
            "bootc",
            "internals",
            "ostree-ext",
            "container",
            "image",
            "pull",
            str(repo_dir),
            f"ostree-unverified-image:oci:{args.oci}:latest",
        ]
    )
    refs = [
        line
        for line in _run(["ostree", "refs", f"--repo={repo_dir}"]).splitlines()
        if line.startswith("ostree/container/image/")
    ]
    if len(refs) != 1:
        raise ValueError(f"expected exactly one OCI image ref, found {len(refs)}")
    commit = _run(["ostree", "rev-parse", f"--repo={repo_dir}", refs[0]]).strip()
    _require_hex64(commit, "OCI commit")
    _run(["ostree", "fsck", f"--repo={repo_dir}"])

    installer = _load_module("armada_installer", installer_path)
    Gio, OSTree = installer.ostree_api()
    repo = OSTree.Repo.new(Gio.File.new_for_path(str(repo_dir)))
    repo.open(None)
    image = installer.SourceImage.from_seed(repo, commit, args.origin, args.manifest_digest)
    metadata = image.metadata(repo, commit)
    config = json.loads(metadata["ostree.container.image-config"])
    validate_image_provenance(
        config,
        metadata.get("ostree.manifest-digest"),
        args.manifest_digest,
        build["sourceCommit"],
        args.version,
    )
    kargs = installer.seed_kargs(image, work / "kargs")

    fs_work = work / "fs"
    fs_work.mkdir()
    recipes = {}
    with filesystem_pair(
        fs_work,
        build["bootBytes"],
        build["rootBytes"],
        build["bootUuid"],
        build["rootUuid"],
    ) as pair:
        target = Path(pair["target"])
        _run(["ostree", "admin", "init-fs", "--modern", str(target)])
        job = installer.Installation(None)
        job.work = work / "install"
        job.work.mkdir()
        result = job.deploy_tree(
            target,
            image,
            {"boot": build["bootUuid"], "root": build["rootUuid"]},
            kargs,
            boot_method="kshimc",
        )
        _require_hex64(result["deployment_checksum"], "deployment checksum")
        bls_path = target / "boot" / result["bls"]
        parsed = parse_deployment_result(
            result, bls_path.read_text(encoding="utf-8")
        )
        deployment = _selected_deployment(target, result["deployment_checksum"])
        _verify_frozen_overlay(source, deployment)
        _verify_plasma_packages(deployment)
        _verify_model_boot_requirements(
            deployment,
            target / "boot" / result["initrd"],
            configuration,
            models,
            work,
        )
        _write_deployment_markers(target, deployment, build)
        _copy_payload(
            target / "boot" / result["image"],
            staging / "common/boot/kernel.img",
            require_arm64=True,
        )
        _copy_payload(
            target / "boot" / result["initrd"], staging / "common/boot/initramfs.img"
        )
        kshim = _prepare_dtb_module(source, work)
        fdtdir = target / "boot" / result["fdtdir"]
        for name in models:
            profile = profiles[name]
            model_dir = staging / "models" / name
            (model_dir / "boot").mkdir(parents=True)
            (model_dir / "runtime").mkdir()
            dtb = _resolve_dtb(fdtdir, profile, name)
            normal = _model_boot_args(parsed["normalBootArgs"], profile)
            diagnostic = _diagnostic_boot_args(normal, profile)
            kshim.REQUIRED_COMPATIBLE = profile["required"]
            kshim.FORBIDDEN_COMPATIBLE = profile["forbidden"]
            _build_dtb(
                kshim,
                dtb,
                normal,
                "os",
                f"{name}-normal",
                model_dir / "boot/dtb-normal.img",
            )
            _build_dtb(
                kshim,
                dtb,
                diagnostic,
                "maintenance",
                f"{name}-diagnostic",
                model_dir / "boot/dtb-diagnostic.img",
            )
            _copy_payload(
                runtimes[name]["path"], model_dir / "runtime/runtime.bin"
            )
            recipes[name] = {"normal": normal, "diagnostic": diagnostic}
        _run(["ostree", "fsck", f"--repo={target}/ostree/repo"])
        os.sync()
        stats = os.statvfs(target)
        available = stats.f_bavail * stats.f_frsize
        used = validate_space(build["rootBytes"], available, MINIMUM_DATA_BYTES)

    boot_image = Path(pair["boot_image"])
    root_image = Path(pair["root_image"])
    if boot_image.stat().st_size != build["bootBytes"]:
        raise ValueError("boot image length does not match the declared bounds")
    if root_image.stat().st_size != build["rootBytes"]:
        raise ValueError("root image length does not match the declared bounds")
    _probe_image(boot_image, "ext4", build["bootUuid"])
    _probe_image(root_image, "btrfs", build["rootUuid"])
    _run(["e2fsck", "-fn", str(boot_image)])
    _run(["btrfs", "check", "--readonly", str(root_image)])
    boot_gz = gzip_image(boot_image, staging / "common/images/linux-boot.ext4.gz")
    root_gz = gzip_image(root_image, staging / "common/images/linux-root.btrfs.gz")

    for name in models:
        model_dir = staging / "models" / name
        model_files = f"models/{name}"
        artifacts = [
            _artifact(
                staging,
                "linux-boot-raw",
                "images/linux-boot.ext4.gz",
                "common/images/linux-boot.ext4.gz",
                {
                    "compression": "gzip",
                    "expandedBytes": build["bootBytes"],
                    "expandedSha256": boot_gz["expandedSha256"],
                    "sectorSize": SECTOR_SIZE,
                    "fsType": "ext4",
                    "fsUuid": build["bootUuid"],
                    "minimumPartitionBytes": build["bootBytes"],
                },
            ),
            _artifact(
                staging,
                "linux-root-raw",
                "images/linux-root.btrfs.gz",
                "common/images/linux-root.btrfs.gz",
                {
                    "compression": "gzip",
                    "expandedBytes": build["rootBytes"],
                    "expandedSha256": root_gz["expandedSha256"],
                    "sectorSize": SECTOR_SIZE,
                    "fsType": "btrfs",
                    "fsUuid": build["rootUuid"],
                    "minimumPartitionBytes": MINIMUM_ROOT_PARTITION_BYTES,
                },
            ),
            _artifact(staging, "kernel", "boot/kernel.img", "common/boot/kernel.img"),
            _artifact(
                staging,
                "runtime",
                "runtime/runtime.bin",
                f"{model_files}/runtime/runtime.bin",
            ),
            _artifact(
                staging, "initramfs", "boot/initramfs.img", "common/boot/initramfs.img"
            ),
            _artifact(
                staging,
                "dtb-normal",
                "boot/dtb-normal.img",
                f"{model_files}/boot/dtb-normal.img",
            ),
            _artifact(
                staging,
                "dtb-diagnostic",
                "boot/dtb-diagnostic.img",
                f"{model_files}/boot/dtb-diagnostic.img",
            ),
        ]
        manifest = {
            "schemaVersion": 2,
            "packageId": "armadaos-freedom-" + name.lower(),
            "version": args.version,
            "deviceModel": name,
            "compatible": dict(profiles[name]["compatible"]),
            "sourceCommit": build["sourceCommit"],
            "testOnly": False,
            "installTargets": ["internal"],
            "bootCompatibility": _boot_compatibility(profiles[name]),
            "toolsCompatibility": {"installerProtocol": 2, "bootBuilderProtocol": 1},
            "storage": {
                "linuxBootBytes": build["bootBytes"],
                "systemRequiredBytes": used,
                "deploymentScratchBytes": 0,
                "minimumLinuxDataBytes": MINIMUM_DATA_BYTES,
            },
            "artifacts": artifacts,
            "deployment": {
                "kind": "edl-raw-v1",
                "commit": result["deployment_checksum"],
                "manifestDigest": args.manifest_digest,
                "origin": args.origin,
                "bootChecksum": parsed["bootChecksum"],
            },
            "bootRecipe": {
                "bootUuid": build["bootUuid"],
                "rootUuid": build["rootUuid"],
                "normalBootArgs": recipes[name]["normal"],
                "diagnosticBootArgs": recipes[name]["diagnostic"],
                "defaultEntry": parsed["defaultEntry"],
            },
        }
        _write_json(model_dir / "package.json", manifest)

    _write_json(
        staging / "bundle.json",
        {
            "schemaVersion": 2,
            "bundleId": "armadaos-freedom",
            "version": args.version,
            "packages": [
                {
                    "deviceModel": name,
                    "directory": f"models/{name}",
                    "sharedFiles": [
                        {"source": source_name, "destination": destination}
                        for source_name, destination in COMMON_PAYLOADS
                    ],
                }
                for name in models
            ],
        },
    )

    archive_name = f"ArmadaOS-Freedom-{args.version}.zip"
    temp_archive = out / ("." + archive_name + ".part")
    info = _create_archive(staging, temp_archive)
    _write_json(
        work / "internal-build-report.json",
        {
            "schema": 1,
            "sourceCommit": build["sourceCommit"],
            "ociCommit": commit,
            "manifestDigest": args.manifest_digest,
            "deploymentCommit": result["deployment_checksum"],
            "bootChecksum": parsed["bootChecksum"],
            "version": args.version,
            "origin": args.origin,
            "bootUuid": build["bootUuid"],
            "rootUuid": build["rootUuid"],
            "bootBytes": build["bootBytes"],
            "rootBytes": build["rootBytes"],
            "systemRequiredBytes": used,
            "deviceAvailableBytes": available,
            "bootCompressed": boot_gz,
            "rootCompressed": root_gz,
            "runtimes": runtimes,
            "archive": {"name": archive_name, **info},
            "hardwareTested": False,
        },
    )
    os.replace(str(temp_archive), str(out / archive_name))
    sys.stdout.write(f"wrote {out / archive_name}\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Build the direct EDL raw filesystem bundle (ZIP64)."
    )
    parser.add_argument("--source", required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--oci", required=True)
    parser.add_argument("--manifest-digest", required=True)
    parser.add_argument("--origin", required=True)
    parser.add_argument("--runtime", action="append", required=True)
    parser.add_argument("--boot-uuid", required=True)
    parser.add_argument("--root-uuid", required=True)
    parser.add_argument(
        "--root-image-gib",
        type=int,
        choices=ROOT_IMAGE_GIB_CHOICES,
        default=ROOT_IMAGE_GIB_CHOICES[0],
    )
    parser.add_argument("--version", required=True)
    parser.add_argument("--devices-config", default=str(DEFAULT_DEVICES_CONFIG))
    parser.add_argument("--work", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--model", action="append")
    args = parser.parse_args(argv)
    try:
        _produce(args)
    except subprocess.CalledProcessError as error:
        command = " ".join(str(part) for part in error.cmd)
        sys.stderr.write(f"ERROR: command failed ({error.returncode}): {command}\n")
        if error.output:
            sys.stderr.write(error.output)
        return 1
    except Exception as error:
        sys.stderr.write(f"ERROR: {type(error).__name__}: {error}\n")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
