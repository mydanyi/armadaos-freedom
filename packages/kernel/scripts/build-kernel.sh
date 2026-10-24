#!/bin/bash
# Build aarch64 kernel from kernel.org tarball + patches/ + config overrides into
# the tarball layout armada's 20-install-kernel.sh consumes. FAST=1 reuses the
# work dir. Native on aarch64; cross-compiles via aarch64-linux-gnu-gcc on x86_64.

set -euo pipefail

# ---------- Config ----------
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL_VERSION="${KERNEL_VERSION:-$(. "${REPO_ROOT}/BASE.env"; echo "$VERSION")}"
KERNEL_MAJOR="${KERNEL_VERSION%%.*}"
WORK_DIR="${WORK_DIR:-/var/tmp/armada-kernel-build}"
OUT_DIR="${OUT_DIR:-${REPO_ROOT}/out}"
SERIES_FILE="${REPO_ROOT}/patches/series"
PATCHES_DIR="${REPO_ROOT}/patches"
DTS_DIR="${REPO_ROOT}/dts"
KCONFIG_OVERRIDES="${REPO_ROOT}/config/armada-kernel.config.overrides"
# Opt-in board overlays (overlays/<name>/, see overlays/README.md), applied on
# top of the shared tree. Empty by default: the default build is unchanged.
KERNEL_OVERLAYS="${KERNEL_OVERLAYS:-}"
OVERLAY_DIRS=()
for overlay in ${KERNEL_OVERLAYS}; do
    if [ ! -d "${REPO_ROOT}/overlays/${overlay}" ]; then
        echo "ERROR: unknown kernel overlay: ${overlay}" >&2
        exit 1
    fi
    OVERLAY_DIRS+=("${REPO_ROOT}/overlays/${overlay}")
done

# ---------- Host arch / cross-compile setup ----------
HOST_ARCH=$(uname -m)
JOBS=$(nproc)
MAKE_ARGS=(-j"${JOBS}")
if [[ "${HOST_ARCH}" == "aarch64" ]]; then
    echo "==> Native aarch64 build (${JOBS} jobs)"
else
    if ! command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        echo "ERROR: aarch64-linux-gnu-gcc not found. Install the cross toolchain:" >&2
        echo "  sudo pacman -S aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils  # CachyOS/Arch" >&2
        echo "  sudo apt install gcc-aarch64-linux-gnu                            # Debian/Ubuntu" >&2
        exit 1
    fi
    MAKE_ARGS+=(ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-)
    echo "==> Cross-compiling from ${HOST_ARCH} to aarch64 (${JOBS} jobs)"
fi

mkdir -p "${WORK_DIR}" "${OUT_DIR}"
export KBUILD_BUILD_USER="${KBUILD_BUILD_USER:-armada}"
export KBUILD_BUILD_HOST="${KBUILD_BUILD_HOST:-builder}"

if command -v ccache >/dev/null 2>&1; then
    ccache -M 2G >/dev/null 2>&1 || true
    ccache -z >/dev/null 2>&1 || true
    if [[ "${HOST_ARCH}" == "aarch64" ]]; then
        MAKE_ARGS+=(CC="ccache gcc")
    else
        MAKE_ARGS+=(CC="ccache aarch64-linux-gnu-gcc")
    fi
fi

# ---------- 1. Fetch upstream source ----------
if [[ "${KERNEL_VERSION}" == *-rc* ]]; then
    SRC_TARBALL="linux-${KERNEL_VERSION}.tar.gz"
    SRC_URL="${KERNEL_SRC_URL:-https://git.kernel.org/torvalds/t/${SRC_TARBALL}}"
    SRC_URL_FALLBACK="https://github.com/torvalds/linux/archive/refs/tags/v${KERNEL_VERSION}.tar.gz"
else
    SRC_TARBALL="linux-${KERNEL_VERSION}.tar.xz"
    # Some build environments cannot fetch from cdn.kernel.org; gregkh/linux
    # mirrors the stable tags and provides a fallback source.
    SRC_URL="${KERNEL_SRC_URL:-https://cdn.kernel.org/pub/linux/kernel/v${KERNEL_MAJOR}.x/${SRC_TARBALL}}"
    SRC_URL_FALLBACK="https://github.com/gregkh/linux/archive/refs/tags/v${KERNEL_VERSION}.tar.gz"
fi

cd "${WORK_DIR}"
if [ ! -f "${SRC_TARBALL}" ]; then
    echo "==> Downloading ${SRC_URL}"
    curl -fsSL -o "${SRC_TARBALL}" "${SRC_URL}" \
        || { echo "==> primary source failed, trying ${SRC_URL_FALLBACK}"; curl -fsSL -o "${SRC_TARBALL}" "${SRC_URL_FALLBACK}"; }
else
    echo "==> Using cached ${SRC_TARBALL}"
fi

SRC_DIR="${WORK_DIR}/linux-${KERNEL_VERSION}"
# The overlay set a tree was patched for; FAST=1 must not reuse it for another.
OVERLAY_STAMP="${SRC_DIR}.overlays"
OVERLAY_NAMES="$(echo ${KERNEL_OVERLAYS})"
if [ -n "${FAST:-}" ] && [ -d "${SRC_DIR}" ]; then
    echo "==> FAST=1: reusing existing ${SRC_DIR} (skipping extract + patches)"
    built_overlays="$(cat "${OVERLAY_STAMP}" 2>/dev/null || true)"
    if [ "${built_overlays}" != "${OVERLAY_NAMES}" ]; then
        echo "ERROR: ${SRC_DIR} was built with KERNEL_OVERLAYS='${built_overlays}'," \
             "not '${OVERLAY_NAMES}'; rebuild without FAST=1" >&2
        exit 1
    fi
else
    rm -rf "${SRC_DIR}"
    echo "==> Extracting ${SRC_TARBALL}"
    tar -xf "${SRC_TARBALL}"
    # github archives name the top dir after the tag, not linux-<version>
    [ -d "${SRC_DIR}" ] || mv "${WORK_DIR}/$(tar -tf "${SRC_TARBALL}" | head -1 | cut -d/ -f1)" "${SRC_DIR}"
    echo "${OVERLAY_NAMES}" > "${OVERLAY_STAMP}"
fi
cd "${SRC_DIR}"
PREFIX_MAP_FLAGS="-ffile-prefix-map=${SRC_DIR}=linux-${KERNEL_VERSION} -fdebug-prefix-map=${SRC_DIR}=linux-${KERNEL_VERSION} -fmacro-prefix-map=${SRC_DIR}=linux-${KERNEL_VERSION} -ffile-prefix-map=${WORK_DIR}=armada-kernel-build -fdebug-prefix-map=${WORK_DIR}=armada-kernel-build -fmacro-prefix-map=${WORK_DIR}=armada-kernel-build"
export KCFLAGS="${KCFLAGS:-} ${PREFIX_MAP_FLAGS}"
export HOSTCFLAGS="${HOSTCFLAGS:-} ${PREFIX_MAP_FLAGS}"
export KAFLAGS="${KAFLAGS:-} ${PREFIX_MAP_FLAGS}"

# ---------- 2. Apply patches ----------
# Apply one patch strictly; updates APPLIED/FAILED. $1 = file, $2 = label.
apply_patch_file() {
    local PATCH="$1" line="$2"

    # GNU patch can return success after consuming only the first hunk of a
    # malformed patch and labelling the rest "trailing garbage". Preflight
    # verbosely and require every hunk header in the payload to be applied.
    patch_log=$(mktemp)
    expected_hunks=$(grep -c '^@@ ' "${PATCH}" || true)
    if ! patch -p1 --batch --forward -F0 --dry-run --verbose \
            < "${PATCH}" > "${patch_log}" 2>&1; then
        cat "${patch_log}"
        rm -f "${patch_log}"
        FAILED=$((FAILED+1))
        echo "  ✗ ${line}  (preflight failed in ${SRC_DIR})"
        return
    fi
    applied_hunks=$(grep -c '^Hunk #[0-9].*succeeded' "${patch_log}" || true)
    if [ "${applied_hunks}" -ne "${expected_hunks}" ]; then
        cat "${patch_log}"
        rm -f "${patch_log}"
        FAILED=$((FAILED+1))
        echo "  ✗ ${line}  (${applied_hunks}/${expected_hunks} hunks parsed)"
        return
    fi
    rm -f "${patch_log}"

    if patch -p1 --batch --forward -F0 --no-backup-if-mismatch --quiet \
            < "${PATCH}"; then
        APPLIED=$((APPLIED+1))
        echo "  ✓ ${line}"
    else
        FAILED=$((FAILED+1))
        echo "  ✗ ${line}  (rejects in ${SRC_DIR})"
    fi
}

# Precount for the .armada-source metadata; FAST=1 skips the apply loop below.
APPLIED=$(sed 's/#.*//' "${SERIES_FILE}" | awk 'NF { count++ } END { print count+0 }')
if [ -z "${FAST:-}" ]; then
    echo "==> Applying patches from $(basename ${SERIES_FILE})"
    APPLIED=0
    FAILED=0
    while IFS= read -r line; do
        line="${line%%#*}"
        line="${line## }"
        line="${line%% *}"
        [ -z "${line}" ] && continue
        PATCH="${PATCHES_DIR}/${line}"
        if [ ! -f "${PATCH}" ]; then
            echo "  WARN: ${line} not in patches/ (skipping)"
            continue
        fi
        apply_patch_file "${PATCH}" "${line}"
    done < "${SERIES_FILE}"
    # Overlay patches: every overlays/<name>/patches/*.patch, in name order
    for ovdir in "${OVERLAY_DIRS[@]}"; do
        echo "==> Applying overlay $(basename "${ovdir}")"
        for PATCH in "${ovdir}"/patches/*.patch; do
            [ -e "${PATCH}" ] || continue
            apply_patch_file "${PATCH}" "overlays/$(basename "${ovdir}")/patches/$(basename "${PATCH}")"
        done
    done
    echo "==> Patches: ${APPLIED} applied, ${FAILED} failed"
    if [ "${FAILED}" -gt 0 ]; then
        echo "ERROR: some patches failed. Look for *.rej files in ${SRC_DIR} and either:"
        echo "  - refresh the patch against ${KERNEL_VERSION}"
        echo "  - drop it from patches/series (comment out)"
        echo "  - try an older KERNEL_VERSION that matches the patch base"
        exit 1
    fi
fi

# ---------- 3. Vendor DTS files (kernel only builds DTBs listed in its Makefile) ----------
echo "==> Copying DTS files into arch/arm64/boot/dts/qcom/"
DTS_TARGET="${SRC_DIR}/arch/arm64/boot/dts/qcom"
mkdir -p "${DTS_TARGET}"
cp -v "${DTS_DIR}"/*.dts "${DTS_DIR}"/*.dtsi "${DTS_TARGET}/" 2>&1 | sed 's/^/  /'

# Apply armada's DTS edits (dts/*.dts(i) are vendored verbatim; the deltas are
# dts/*.patch). Board DTS aren't in the tree at patch time, so they patch here,
# after the copy above.
echo "==> Applying DTS edit patches"
for p in "${DTS_DIR}"/*.patch; do
    [ -e "${p}" ] || continue
    if patch -p1 -d "${SRC_DIR}" -F0 --no-backup-if-mismatch -s < "${p}"; then
        echo "  ✓ $(basename "${p}")"
    else
        echo "  ✗ $(basename "${p}") failed (see *.rej in ${DTS_TARGET})"; exit 1
    fi
done

# Append missing DTB entries to the qcom Makefile so they get built. The
# marker is only a comment: FAST=1 re-runs after a board is added must still
# register the new .dtb, while already-registered boards stay untouched.
QCOM_MAKEFILE="${DTS_TARGET}/Makefile"
echo "==> Adding DTB entries to qcom Makefile"
if ! grep -qxF "# armada-kernel DTBs" "${QCOM_MAKEFILE}" 2>/dev/null; then
    {
        echo ""
        echo "# armada-kernel DTBs"
    } >> "${QCOM_MAKEFILE}"
fi
added_dtbs=0
for dts in "${DTS_DIR}"/*.dts; do
    base=$(basename "${dts}" .dts)
    entry="dtb-\$(CONFIG_ARCH_QCOM) += ${base}.dtb"
    if grep -qxF "${entry}" "${QCOM_MAKEFILE}"; then
        continue
    fi
    echo "${entry}" >> "${QCOM_MAKEFILE}"
    added_dtbs=$((added_dtbs+1))
done
echo "  added ${added_dtbs} DTB entries"

# Overlay DTS: board sources and .dtso overlays are copied in; dts/*.mk lines
# (plain dtb-y entries and composite <name>-dtbs := base.dtb x.dtbo lines,
# merged by fdtoverlay) are appended to the qcom Makefile once.
for ovdir in "${OVERLAY_DIRS[@]}"; do
    [ -d "${ovdir}/dts" ] || continue
    echo "==> Adding overlay DTS from $(basename "${ovdir}")"
    for f in "${ovdir}"/dts/*.dts "${ovdir}"/dts/*.dtsi "${ovdir}"/dts/*.dtso; do
        [ -e "${f}" ] || continue
        cp -v "${f}" "${DTS_TARGET}/" 2>&1 | sed 's/^/  /'
    done
    for mk in "${ovdir}"/dts/*.mk; do
        [ -e "${mk}" ] || continue
        while IFS= read -r mkline; do
            [ -n "${mkline}" ] || continue
            grep -qxF "${mkline}" "${QCOM_MAKEFILE}" || echo "${mkline}" >> "${QCOM_MAKEFILE}"
        done < "${mk}"
    done
done

# ---------- 4. Generate config ----------
# defconfig + fragment via merge_config.sh. Explicit =y requests are checked
# after Kconfig resolution so a dependency cannot silently demote them to =m.
if [ -z "${FAST:-}" ]; then
    echo "==> Generating .config from defconfig"
    make "${MAKE_ARGS[@]}" defconfig >/dev/null
else
    echo "==> FAST=1: reusing existing .config (fragment re-merged below)"
fi

if [ -f "${KCONFIG_OVERRIDES}" ]; then
    echo "==> Merging config fragment $(basename "${KCONFIG_OVERRIDES}")"
    # merge_config.sh's internal `make` reads ARCH/CROSS_COMPILE from the env
    export ARCH="${ARCH:-arm64}"
    [ "${HOST_ARCH}" = "aarch64" ] || export CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"
    frag=$(mktemp); merge_log=$(mktemp)
    # kconfig's conf can't parse trailing inline comments; strip them off assignments
    # overlay fragments (overlays/<name>/config/*.config) follow and win
    overlay_frags=()
    for ovdir in "${OVERLAY_DIRS[@]}"; do
        for f in "${ovdir}"/config/*.config; do
            [ -e "${f}" ] && overlay_frags+=("${f}")
        done
    done
    [ "${#overlay_frags[@]}" -eq 0 ] || echo "==> Overlay config fragments: ${overlay_frags[*]##*/}"
    cat "${KCONFIG_OVERRIDES}" "${overlay_frags[@]}" \
        | sed -E '/^[[:space:]]*CONFIG_[A-Z0-9_]+=/ s/[[:space:]]*#.*$//' > "${frag}"
    bash scripts/kconfig/merge_config.sh .config "${frag}" 2>&1 | tee "${merge_log}"
    config_error=0
    # Later fragments win: check only each symbol's last request.
    while IFS= read -r requested; do
        case "${requested}" in
            CONFIG_*=y)
                symbol="${requested%%=*}"
                if ! grep -qx "${symbol}=y" .config; then
                    actual=$(grep -E "^${symbol}=" .config || true)
                    echo "ERROR: ${requested} did not survive Kconfig dependencies; actual: ${actual:-not set}"
                    config_error=1
                fi
                ;;
        esac
    done < <(awk '
        /^[[:space:]]*CONFIG_[A-Z0-9_]+=/ {
            line = $0; sub(/^[[:space:]]*/, "", line)
            sym = line; sub(/=.*/, "", sym)
        }
        /^[[:space:]]*# CONFIG_[A-Z0-9_]+ is not set/ { sym = $2; line = sym "=n" }
        sym != "" {
            if (!(sym in last)) order[++n] = sym
            last[sym] = line; sym = ""
        }
        END { for (i = 1; i <= n; i++) print last[order[i]] }' "${frag}")
    if [ "${config_error}" -ne 0 ] || grep -q "not in final .config" "${merge_log}"; then
        rm -f "${frag}" "${merge_log}"
        echo "ERROR: a requested CONFIG didn't survive Kconfig deps (see above); check the depends-on chain."
        exit 1
    fi
    rm -f "${frag}" "${merge_log}"
fi

# An overlay may set CONFIG_LOCALVERSION; kernelrelease only sees it once
# include/config is in sync with the merged .config.
[ "${#OVERLAY_DIRS[@]}" -eq 0 ] || make "${MAKE_ARGS[@]}" -s syncconfig >/dev/null
KVER=$(make "${MAKE_ARGS[@]}" -s kernelrelease)
echo "==> Kernel version: ${KVER}"
echo "==> Compiler: $(sed -n 's/^CONFIG_CC_VERSION_TEXT="\(.*\)"$/\1/p' .config)"

# ---------- 5. Build ----------
echo "==> Building Image + dtbs + modules"
make "${MAKE_ARGS[@]}" Image dtbs modules

# Out-of-tree Gen5 speaker module. The symbol is a make variable only: the
# driver's Kconfig is never merged into the kernel tree. Build from a fresh
# per-run copy under WORK_DIR so the vendored tree on the NAS stays clean.
AW_OOT_SRC="${REPO_ROOT}/out-of-tree/aw882xx"
AW_OOT_BUILD="${WORK_DIR}/aw882xx-build"
if [ ! -d "${AW_OOT_SRC}" ]; then
    echo "ERROR: out-of-tree aw882xx source missing: ${AW_OOT_SRC}" >&2
    exit 1
fi
if [ -z "${WORK_DIR}" ] || [ "${WORK_DIR}" = "/" ]; then
    echo "ERROR: refusing to clear ${AW_OOT_BUILD}: unsafe WORK_DIR" >&2
    exit 1
fi
rm -rf "${AW_OOT_BUILD}"
mkdir -p "${AW_OOT_BUILD}"
cp -a "${AW_OOT_SRC}/." "${AW_OOT_BUILD}/"
echo "==> Building out-of-tree aw882xx module"
make "${MAKE_ARGS[@]}" M="${AW_OOT_BUILD}" CONFIG_SND_SOC_AW882XX=m modules

command -v ccache >/dev/null 2>&1 && ccache -s || true
size vmlinux 2>/dev/null || true

# ---------- 6. Stage outputs ----------
STAGE="${WORK_DIR}/staging-${KVER}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}/lib/modules/${KVER}/dtb/qcom"

echo "==> Staging vmlinuz + modules + DTBs"
cp arch/arm64/boot/Image "${STAGE}/lib/modules/${KVER}/vmlinuz"
make "${MAKE_ARGS[@]}" INSTALL_MOD_PATH="${STAGE}" INSTALL_MOD_STRIP=1 modules_install >/dev/null

echo "==> Staging out-of-tree aw882xx module to extra/aw882xx"
make "${MAKE_ARGS[@]}" M="${AW_OOT_BUILD}" CONFIG_SND_SOC_AW882XX=m \
    INSTALL_MOD_PATH="${STAGE}" INSTALL_MOD_DIR="extra/aw882xx" \
    INSTALL_MOD_STRIP=1 modules_install
installed_ko=$(find "${STAGE}/lib/modules/${KVER}/extra/aw882xx" -maxdepth 1 \
    -name 'snd-soc-aw882xx.ko*' -print -quit || true)
if [ -z "${installed_ko}" ]; then
    echo "ERROR: snd-soc-aw882xx module was not installed into ${STAGE}" >&2
    exit 1
fi

rm -f "${STAGE}/lib/modules/${KVER}/build" "${STAGE}/lib/modules/${KVER}/source"

# Regenerate the stage index after the out-of-tree install. A missing depmod
# is a hard failure, not a silent skip.
if ! command -v depmod >/dev/null 2>&1; then
    echo "ERROR: depmod not found; cannot index ${STAGE}" >&2
    exit 1
fi
depmod -b "${STAGE}" "${KVER}"

for dts in "${DTS_DIR}"/*.dts; do
    base=$(basename "${dts}" .dts)
    dtb_src="arch/arm64/boot/dts/qcom/${base}.dtb"
    if [ -f "${dtb_src}" ]; then
        cp "${dtb_src}" "${STAGE}/lib/modules/${KVER}/dtb/qcom/"
    else
        echo "  WARN: built DTB missing: ${dtb_src}"
    fi
done
# Overlay DTBs: every "dtb-... += <name>.dtb" target of the overlay .mk files
for ovdir in "${OVERLAY_DIRS[@]}"; do
    for dtb in $(cat "${ovdir}"/dts/*.mk 2>/dev/null | sed -n 's/^dtb-.*+=[[:space:]]*\([^[:space:]]*\.dtb\)[[:space:]]*$/\1/p'); do
        if [ -f "arch/arm64/boot/dts/qcom/${dtb}" ]; then
            cp "arch/arm64/boot/dts/qcom/${dtb}" "${STAGE}/lib/modules/${KVER}/dtb/qcom/"
        else
            echo "  WARN: built overlay DTB missing: ${dtb}"
        fi
    done
done

cat > "${STAGE}/lib/modules/${KVER}/.armada-source" <<EOF
Source: linux-${KERNEL_VERSION} (kernel.org stable)
Built: armada-builder on ${HOST_ARCH}
Patches applied: ${APPLIED:-?} (from patches/series)
DTBs included: $(ls ${DTS_DIR}/*.dts | wc -l) boards (SM8250 + SM8550 + SM8650 + SM8750 + SM8850)
Repackaged for: armada
EOF
[ -z "${KERNEL_OVERLAYS}" ] || echo "Overlays: ${KERNEL_OVERLAYS}" >> "${STAGE}/lib/modules/${KVER}/.armada-source"

# ---------- 7. Package ----------
OUT_NAME="armada-kernel-${KVER}.tar.zst"
OUT_PATH="${OUT_DIR}/${OUT_NAME}"
echo "==> Compressing → ${OUT_PATH}"
cd "${STAGE}"
tar --owner=0 --group=0 -cf - lib | zstd -f -10 -T0 -o "${OUT_PATH}"
cd "${REPO_ROOT}"
(
    cd "${OUT_DIR}"
    sha256sum "${OUT_NAME}" > "${OUT_NAME}.sha256"
)

echo ""
echo "==> Done."
ls -lh "${OUT_PATH}" "${OUT_PATH}.sha256"
cat "${OUT_PATH}.sha256"
