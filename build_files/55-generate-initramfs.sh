#!/bin/bash
set -euxo pipefail

# Runs after 40-vendor-system-files: the initramfs bundles the armada splash,
# which is not yet installed when the kernel step runs.
KVER="$(ls /usr/lib/modules)"
IMG="/usr/lib/modules/${KVER}/initramfs.img"

# fedora-bootc ships /root -> var/roothome, absent in the build container;
# dracut-install aborts resolving /root without the target.
mkdir -p /var/roothome

BOOT_HELPER=/ctx/build_files/edl_boot_requirements.py
CONFIG=/ctx/build_files/edl-devices.json

firmware_text="$(python3 "${BOOT_HELPER}" --config "${CONFIG}" --root / --kver "${KVER}" --format firmware)"
firmware=()
if [ -n "${firmware_text}" ]; then
    mapfile -t firmware <<< "${firmware_text}"
fi

drivers_text="$(python3 "${BOOT_HELPER}" --config "${CONFIG}" --root / --kver "${KVER}" --format drivers)"
drivers=()
if [ -n "${drivers_text}" ]; then
    mapfile -t drivers <<< "${drivers_text}"
fi

optional_args=()
if [ "${#firmware[@]}" -gt 0 ]; then
    optional_args+=(--install "${firmware[*]}")
fi
if [ "${#drivers[@]}" -gt 0 ]; then
    optional_args+=(--add-drivers "${drivers[*]}")
fi

# --xz: the Zstd default exceeds the 96MiB Android boot partition envelope;
# XZ keeps the early CPIO bytes and main CPIO content unchanged.
dracut \
    --force \
    --no-hostonly \
    --reproducible \
    --xz \
    --kver "${KVER}" \
    --add ostree \
    --add armada-splash \
    --add armada-ostree-fallback \
    "${optional_args[@]}" \
    "${IMG}" "${KVER}"

# dracut drops modules silently: fail the build rather than ship without.
# Exact match: a substring test lets armada-splash-launcher pass for the binary.
required=(
    usr/lib/systemd/system/armada-splash-initrd.service \
    usr/lib/systemd/system/dracut-pre-mount.service.d/armada-splash.conf \
    usr/libexec/armada/armada-splash \
    usr/libexec/armada/armada-splash-launcher \
    usr/libexec/armada/device-env \
    usr/share/armada/splash/splash.asp \
    usr/libexec/armada/armada-ostree-fallback \
    usr/lib/systemd/system/ostree-prepare-root.service.d/armada-fallback.conf \
    usr/lib/ostree/ostree-prepare-root
)

if ! lsinitrd "${IMG}" | awk '
    BEGIN {
        count = ARGC - 1
        for (i = 1; i <= count; i++) {
            paths[i] = ARGV[i]
            required[ARGV[i]] = 1
        }
        ARGC = 1
    }
    $NF in required { found[$NF] = 1 }
    END {
        for (i = 1; i <= count; i++) {
            if (!(paths[i] in found)) {
                print "ERROR: " paths[i] " missing from initramfs" > "/dev/stderr"
                missing = 1
            }
        }
        exit missing
    }
' "${required[@]}"; then
    dracut --list-modules --kver "${KVER}" | grep -i armada || true
    exit 1
fi

python3 "${BOOT_HELPER}" --config "${CONFIG}" --kver "${KVER}" --initrd "${IMG}" --root / --format verify

echo "initramfs generated for ${KVER} with armada-splash"
