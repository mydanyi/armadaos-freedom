#!/usr/bin/env bash
# Isolated regression tests for build_files/45-install-decky-plugins.sh.
# curl and systemctl are stubbed; a rewritten copy of the production script
# points its hard-coded image paths at a per-case sandbox.
#
# Decky platform policy is preview: the newest non-draft prerelease, taken from
# prefetched release metadata when supplied and otherwise fetched from the exact
# release-list endpoint. /releases/latest is never used.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/build_files/45-install-decky-plugins.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

LIST_URL='https://api.github.com/repos/SteamDeckHomebrew/decky-loader/releases?per_page=100'
LATEST_URL='https://api.github.com/repos/SteamDeckHomebrew/decky-loader/releases/latest'
TARGET_TAG='v3.3.0-pre1'

FAKE="$WORK/fake"
FIXTURES="$WORK/fixtures"
BIN="$WORK/bin"
CURL_LOG="$WORK/curl.log"
SYSTEMCTL_LOG="$WORK/systemctl.log"
mkdir -p "$BIN" "$FIXTURES"

# Rewrite only the hard-coded production destinations/source roots. The
# install_plugin calls and release logic themselves stay untouched.
sed \
    -e "s|/ctx/decky/|$FAKE/ctx/decky/|g" \
    -e "s|/packages/decky-dist|$FAKE/packages/decky-dist|g" \
    -e "s|/packages/decky-store-dist|$FAKE/packages/decky-store-dist|g" \
    -e "s|/usr/share/decky-plugins|$FAKE/usr/share/decky-plugins|g" \
    -e "s|/usr/share/decky-loader|$FAKE/usr/share/decky-loader|g" \
    -e "s|/usr/lib/decky-loader/armada-decky-sync|$FAKE/usr/lib/decky-loader/armada-decky-sync|g" \
    -e "s|/etc/systemd/system/plugin_loader.service|$FAKE/etc/systemd/system/plugin_loader.service|g" \
    -e "s|/run/secrets/decky_releases|$FAKE/run/secrets/decky_releases|g" \
    "$SRC" >"$WORK/build-under-test.sh"
chmod +x "$WORK/build-under-test.sh"

cat >"$BIN/curl" <<'EOF'
#!/usr/bin/env bash
set -u
out=""
url=""
while (($#)); do
    case "$1" in
        -o|--output) out=$2; shift 2 ;;
        --retry|--retry-delay|--connect-timeout|--max-time|-m|-w|--write-out|-H|--header|-A|--user-agent) shift 2 ;;
        -*) shift ;;
        *) url=$1; shift ;;
    esac
done
[[ -n "$url" ]] || { echo "curl stub: no URL" >&2; exit 2; }
printf '%s\n' "$url" >>"$CURL_LOG"

emit() {
    if [[ -n "$out" ]]; then cp -- "$1" "$out"; else cat -- "$1"; fi
}

case "$url" in
    "https://api.github.com/repos/SteamDeckHomebrew/decky-loader/releases?per_page=100")
        emit "$FIXTURE_DIR/releases.json"
        exit 0
        ;;
    "https://api.github.com/repos/SteamDeckHomebrew/decky-loader/releases/latest")
        echo "curl stub: refusing releases/latest; preview policy requires the release list" >&2
        exit 22
        ;;
    https://api.github.com/repos/SteamDeckHomebrew/decky-loader/*)
        echo "curl stub: refusing non-contract release-list URL $url" >&2
        exit 22
        ;;
esac

rest=""
case "$url" in
    https://raw.githubusercontent.com/SteamDeckHomebrew/decky-loader/*)
        rest=${url#https://raw.githubusercontent.com/SteamDeckHomebrew/decky-loader/}
        ;;
    https://github.com/SteamDeckHomebrew/decky-loader/raw/*)
        rest=${url#https://github.com/SteamDeckHomebrew/decky-loader/raw/}
        ;;
esac
if [[ -n "$rest" ]]; then
    [[ "$rest" == refs/tags/* ]] && rest=${rest#refs/tags/}
    tag=${rest%%/*}
    path=${rest#*/}
    if [[ "$tag" == "$(<"$FIXTURE_DIR/tag.txt")" && "$path" == "dist/plugin_loader-prerelease.service" ]]; then
        emit "$FIXTURE_DIR/service-prerelease.txt"
        exit 0
    fi
    echo "curl stub: refusing service URL $url" >&2
    exit 22
fi

case "${url##*/}" in
    PluginLoader-arm64) emit "$FIXTURE_DIR/arm64.bin"; exit 0 ;;
    PluginLoader) emit "$FIXTURE_DIR/x86.bin"; exit 0 ;;
esac
echo "curl stub: refusing URL $url" >&2
exit 22
EOF
chmod +x "$BIN/curl"

cat >"$BIN/systemctl" <<'EOF'
#!/usr/bin/env bash
set -u
printf '%s\n' "$*" >>"${SYSTEMCTL_LOG:?}"
EOF
chmod +x "$BIN/systemctl"

make_fixtures() {
    local dir=$1 mode=$2
    rm -rf "$dir"
    mkdir -p "$dir"
    python3 - "$dir" "$mode" <<'PY'
import hashlib
import json
import os
import struct
import sys

out, mode = sys.argv[1], sys.argv[2]
tag = "v3.3.0-pre1"
base = "https://github.com/SteamDeckHomebrew/decky-loader/releases/download/" + tag


def elf(machine, marker):
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8
    header = ident + struct.pack(
        "<HHIQQQIHHHHHH",
        2,        # e_type: ET_EXEC
        machine,  # e_machine: 183 AArch64, 62 x86-64
        1,        # e_version
        0x400000, 64, 0, 0, 64, 56, 1, 0, 0, 0,
    )
    phdr = struct.pack("<IIQQQQQQ", 1, 4, 0, 0x400000, 0x400000, 0, 0, 0x1000)
    return header + phdr + marker + b"\n"


def sha256(body):
    return hashlib.sha256(body).hexdigest()


def asset(name, body, body_digest=None):
    return {
        "name": name,
        "size": len(body),
        "digest": "sha256:" + (body_digest or sha256(body)),
        "browser_download_url": base + "/" + name,
    }


arm = elf(183, b"arm64-fixture")
x86 = elf(62, b"x86-fixture")
served_arm = x86 if mode == "wrong-arch" else arm

if mode == "success":
    assets = [asset("PluginLoader", x86), asset("PluginLoader-arm64", served_arm)]
elif mode == "missing-arm64":
    assets = [asset("PluginLoader", x86)]
elif mode == "digest-mismatch":
    assets = [
        asset("PluginLoader", x86),
        asset("PluginLoader-arm64", served_arm, sha256(b"not the served bytes")),
    ]
elif mode == "wrong-arch":
    assets = [asset("PluginLoader", x86), asset("PluginLoader-arm64", served_arm)]
elif mode == "no-preview":
    assets = [asset("PluginLoader", x86), asset("PluginLoader-arm64", served_arm)]
else:
    raise SystemExit("unknown fixture mode: " + mode)


def release(tag_name, published_at, prerelease, draft):
    # Non-target entries reuse the target assets: selecting the wrong release
    # is detected by the version and service expectations, not by asset URLs.
    return {
        "tag_name": tag_name,
        "published_at": published_at,
        "draft": draft,
        "prerelease": prerelease,
        "assets": assets,
    }


if mode == "no-preview":
    # Stable plus a draft prerelease only: no eligible preview release.
    releases = [
        release("v3.3.0", "2026-10-20T12:00:00Z", False, False),
        release("v3.4.0-pre1", "2026-10-21T09:30:00Z", True, True),
    ]
else:
    releases = [
        # Newest stable, listed first: preview filtering must skip it.
        release("v3.3.0", "2026-10-20T12:00:00Z", False, False),
        # Older prerelease before the target: latest-by-published_at must win,
        # not the first preview in array order.
        release("v3.2.10-pre1", "2026-09-02T03:04:05Z", True, False),
        # Target: newest published non-draft prerelease.
        release("v3.3.0-pre1", "2026-10-18T01:21:08Z", True, False),
        # Still newer draft prerelease: drafts must be filtered out.
        release("v3.4.0-pre1", "2026-10-21T09:30:00Z", True, True),
    ]

with open(os.path.join(out, "releases.json"), "w") as handle:
    json.dump(releases, handle, indent=2)
with open(os.path.join(out, "arm64.bin"), "wb") as handle:
    handle.write(served_arm)
with open(os.path.join(out, "x86.bin"), "wb") as handle:
    handle.write(x86)
with open(os.path.join(out, "tag.txt"), "w") as handle:
    handle.write(tag + "\n")
with open(os.path.join(out, "service-prerelease.txt"), "w") as handle:
    handle.write(
        "[Unit]\n"
        "Description=Decky Loader service fixture " + tag + "\n"
        "X-Armada-Fixture=service-prerelease.txt@" + tag + "\n"
        "After=network-online.target\n"
        "\n"
        "[Service]\n"
        "Type=simple\n"
        "ExecStart=${HOMEBREW_FOLDER}/services/PluginLoader\n"
        "Restart=always\n"
        "\n"
        "[Install]\n"
        "WantedBy=multi-user.target\n"
    )
PY
}

CASE=""
CASE_FAILED=0
FAILED=0

case_fail() { printf '  FAIL: %s\n' "$*" >&2; CASE_FAILED=1; }

assert_exists() { [[ -e "$1" ]] || case_fail "missing $1"; }
assert_absent() { [[ ! -e "$1" ]] || case_fail "unexpected $1"; }

# Every case must prove the preview policy was exercised: the exact release
# list with per_page=100 was requested and releases/latest never was.
assert_preview_release_list() {
    grep -qxF "$LIST_URL" "$CURL_LOG" ||
        case_fail "did not request the preview release list"
    if grep -qF "$LATEST_URL" "$CURL_LOG"; then
        case_fail "requested releases/latest instead of the release list"
    fi
}

assert_no_release_api() {
    if grep -qF 'https://api.github.com/repos/SteamDeckHomebrew/decky-loader/' "$CURL_LOG"; then
        case_fail "used the release API despite a prefetched release list"
    fi
}

install_prefetched_releases() {
    local source=$1
    mkdir -p "$FAKE/run/secrets"
    case "$source" in
        prefetch) cp -- "$FIXTURES/$CASE/releases.json" "$FAKE/run/secrets/decky_releases" ;;
        prefetch-empty) : >"$FAKE/run/secrets/decky_releases" ;;
        prefetch-malformed) printf 'not a release list' >"$FAKE/run/secrets/decky_releases" ;;
        *) case_fail "unknown prefetch source $source" ;;
    esac
}

setup_case() {
    CASE=$1
    CASE_FAILED=0
    rm -rf "$FAKE"
    mkdir -p "$FAKE/ctx/decky" "$FAKE/packages" \
        "$FAKE/usr/share/decky-loader" "$FAKE/usr/lib/decky-loader" \
        "$FAKE/etc/systemd/system"

    local plugin dist
    for plugin in armada-control armada-store; do
        mkdir -p "$FAKE/ctx/decky/$plugin/py_modules"
        printf '{"name":"%s"}\n' "$plugin" >"$FAKE/ctx/decky/$plugin/plugin.json"
        printf '{"name":"%s","version":"1.0.0"}\n' "$plugin" >"$FAKE/ctx/decky/$plugin/package.json"
        printf 'main:%s\n' "$plugin" >"$FAKE/ctx/decky/$plugin/main.py"
        printf 'module:%s\n' "$plugin" >"$FAKE/ctx/decky/$plugin/py_modules/__init__.py"
    done

    # Synced upstream source: the Freedom Control dependency now provides the
    # armada-control plugin at ctx/decky/freedom-control/decky/armada-control.
    # Its markers are unique. The legacy ctx/decky/armada-control fixture above
    # is kept unchanged so an installer that still copies it keeps running and
    # fails only the freedom-control comparisons in success_case.
    local fc="$FAKE/ctx/decky/freedom-control/decky/armada-control"
    mkdir -p "$fc/py_modules"
    printf '{"name":"Freedom Control"}\n' >"$fc/plugin.json"
    printf '{"name":"freedom-control","version":"1.0.0"}\n' >"$fc/package.json"
    printf 'main:freedom-control\n' >"$fc/main.py"
    printf 'module:freedom-control\n' >"$fc/py_modules/__init__.py"

    for dist in decky-dist decky-store-dist; do
        mkdir -p "$FAKE/packages/$dist"
        printf 'js:%s\n' "$dist" >"$FAKE/packages/$dist/index.js"
        printf 'map:%s\n' "$dist" >"$FAKE/packages/$dist/index.js.map"
    done
    printf '#!/bin/sh\nexit 0\n' >"$FAKE/usr/lib/decky-loader/armada-decky-sync"
    chmod 0755 "$FAKE/usr/lib/decky-loader/armada-decky-sync"

    : >"$CURL_LOG"
    : >"$SYSTEMCTL_LOG"
}

run_bundle() {
    PATH="$BIN:$PATH" \
    FIXTURE_DIR="$FIXTURES/$CASE" \
    CURL_LOG="$CURL_LOG" \
    SYSTEMCTL_LOG="$SYSTEMCTL_LOG" \
        "$WORK/build-under-test.sh" >"$WORK/$CASE.log" 2>&1
}

success_case() {
    local name=${1:-success} source=${2:-api}
    setup_case "$name"
    make_fixtures "$FIXTURES/$name" success
    if [[ "$source" != api ]]; then
        install_prefetched_releases "$source"
    fi

    local rc=0
    run_bundle || rc=$?
    if [[ $rc -ne 0 ]]; then
        case_fail "script exited $rc"
        tail -n 20 "$WORK/$CASE.log" >&2 || true
        return
    fi

    if [[ "$source" == api ]]; then
        assert_preview_release_list
    else
        assert_no_release_api
    fi

    local loader="$FAKE/usr/share/decky-loader/PluginLoader"
    local version_file="$FAKE/usr/share/decky-loader/.loader.version"
    assert_exists "$loader"
    if [[ -f "$loader" ]] && ! cmp -s "$FIXTURES/$name/arm64.bin" "$loader"; then
        case_fail "installed loader is not the PluginLoader-arm64 asset"
    fi

    if [[ -f "$version_file" ]]; then
        [[ "$(<"$version_file")" == "$TARGET_TAG" ]] || case_fail "wrong recorded loader version"
    else
        case_fail "missing .loader.version"
    fi

    local service="$FAKE/etc/systemd/system/plugin_loader.service"
    if [[ -f "$service" ]]; then
        grep -qF '/var/home/armada/homebrew/services/PluginLoader' "$service" ||
            case_fail "HOMEBREW_FOLDER was not substituted"
        if grep -qF '${HOMEBREW_FOLDER}' "$service"; then
            case_fail "service still contains \${HOMEBREW_FOLDER}"
        fi
        grep -qF "Decky Loader service fixture $TARGET_TAG" "$service" ||
            case_fail "service did not come from the $TARGET_TAG tag"
        grep -qF "X-Armada-Fixture=service-prerelease.txt@$TARGET_TAG" "$service" ||
            case_fail "service did not come from the prerelease fixture"
        if [[ -f "$service" ]] && ! cmp -s "$service" "$WORK/service.expected"; then
            case_fail "installed service differs from the exact-tag prerelease fixture"
        fi
    else
        case_fail "missing plugin_loader.service"
    fi

    grep -qxF 'enable armada-decky-sync.service' "$SYSTEMCTL_LOG" ||
        case_fail "armada-decky-sync.service not enabled"
    grep -qxF 'enable plugin_loader.service' "$SYSTEMCTL_LOG" ||
        case_fail "plugin_loader.service not enabled"

    local plugin rel
    for plugin in armada-control armada-store; do
        for rel in plugin.json package.json main.py py_modules/__init__.py dist/index.js; do
            assert_exists "$FAKE/usr/share/decky-plugins/$plugin/$rel"
        done
        assert_absent "$FAKE/usr/share/decky-plugins/$plugin/dist/index.js.map"
    done

    # The installed armada-control must be bundled from the Freedom Control
    # dependency source, not the legacy ctx/decky/armada-control tree.
    local fc_src="$FAKE/ctx/decky/freedom-control/decky/armada-control"
    local installed="$FAKE/usr/share/decky-plugins/armada-control"
    local fc_rel
    for fc_rel in plugin.json package.json main.py py_modules/__init__.py; do
        if [[ ! -f "$installed/$fc_rel" ]]; then
            case_fail "missing installed armada-control/$fc_rel"
        elif ! cmp -s "$fc_src/$fc_rel" "$installed/$fc_rel"; then
            case_fail "installed armada-control/$fc_rel is not from the freedom-control source"
        fi
    done
    if [[ -f "$installed/plugin.json" ]] &&
        ! grep -qE '"name"[[:space:]]*:[[:space:]]*"Freedom Control"' "$installed/plugin.json"; then
        case_fail "installed armada-control plugin.json does not name Freedom Control"
    fi
    # Keep the installed directory name armada-control for runtime/settings
    # compatibility: no parallel freedom-control destination may appear.
    assert_absent "$FAKE/usr/share/decky-plugins/freedom-control"

    local downloads
    downloads=$(grep -c '/releases/download/' "$CURL_LOG" || true)
    [[ "$downloads" -eq 1 ]] || case_fail "expected exactly one release asset download, got $downloads"
    grep -qxF "https://github.com/SteamDeckHomebrew/decky-loader/releases/download/$TARGET_TAG/PluginLoader-arm64" "$CURL_LOG" ||
        case_fail "did not download PluginLoader-arm64 from $TARGET_TAG"
    if grep -qE '/PluginLoader$' "$CURL_LOG"; then
        case_fail "downloaded the x86 PluginLoader asset"
    fi

    grep -qE "(raw\.githubusercontent\.com/SteamDeckHomebrew/decky-loader/|github\.com/SteamDeckHomebrew/decky-loader/raw/)(refs/tags/)?v3\.3\.0-pre1/dist/plugin_loader-prerelease\.service$" "$CURL_LOG" ||
        case_fail "service not fetched from the $TARGET_TAG prerelease service"
    if grep -qF 'plugin_loader-release.service' "$CURL_LOG"; then
        case_fail "fetched the stable plugin_loader-release.service"
    fi
    if grep -qE 'decky-loader/(main|refs/heads/main)/' "$CURL_LOG"; then
        case_fail "fetched the service from main"
    fi
}

refusal_case() {
    local name=$1 mode=${2:-$1} source=${3:-api}
    setup_case "$name"
    make_fixtures "$FIXTURES/$name" "$mode"
    if [[ "$source" != api ]]; then
        install_prefetched_releases "$source"
    fi

    local rc=0
    run_bundle || rc=$?
    if [[ $rc -eq 0 ]]; then
        case_fail "script accepted a release it should refuse"
        tail -n 20 "$WORK/$CASE.log" >&2 || true
    fi

    if [[ "$source" == api ]]; then
        assert_preview_release_list
    else
        assert_no_release_api
        case "$source" in
            prefetch-empty | prefetch-malformed)
                if grep -qE '/releases/download/|/PluginLoader(-arm64)?$|plugin_loader-(release|prerelease)[.]service' "$CURL_LOG"; then
                    case_fail "attempted downloads after an invalid prefetched release list"
                fi
                ;;
        esac
    fi
    assert_absent "$FAKE/usr/share/decky-loader/.loader.version"
    if [[ -s "$SYSTEMCTL_LOG" ]]; then
        case_fail "enabled services after refusing the release"
    fi

    case "$mode" in
        missing-arm64)
            if grep -qE '/PluginLoader$' "$CURL_LOG"; then
                case_fail "fell back to the x86 PluginLoader asset"
            fi
            if [[ -f "$FAKE/usr/share/decky-loader/PluginLoader" ]] &&
                cmp -s "$FAKE/usr/share/decky-loader/PluginLoader" "$FIXTURES/$name/x86.bin"; then
                case_fail "installed the x86 loader"
            fi
            ;;
        digest-mismatch)
            grep -qxF "https://github.com/SteamDeckHomebrew/decky-loader/releases/download/$TARGET_TAG/PluginLoader-arm64" "$CURL_LOG" ||
                case_fail "did not attempt the ARM64 asset"
            ;;
        wrong-arch)
            grep -qxF "https://github.com/SteamDeckHomebrew/decky-loader/releases/download/$TARGET_TAG/PluginLoader-arm64" "$CURL_LOG" ||
                case_fail "did not attempt the ARM64 asset"
            if [[ -f "$FAKE/usr/share/decky-loader/PluginLoader" ]] &&
                cmp -s "$FAKE/usr/share/decky-loader/PluginLoader" "$FIXTURES/$name/x86.bin"; then
                case_fail "installed a non-AArch64 ELF"
            fi
            ;;
        no-preview)
            local downloads
            downloads=$(grep -c '/releases/download/' "$CURL_LOG" || true)
            [[ "$downloads" -eq 0 ]] ||
                case_fail "attempted $downloads asset download(s) without an eligible preview"
            if grep -qE '/PluginLoader(-arm64)?$' "$CURL_LOG"; then
                case_fail "attempted a PluginLoader download without an eligible preview"
            fi
            if grep -qE 'plugin_loader-(release|prerelease)\.service' "$CURL_LOG"; then
                case_fail "attempted a service fetch without an eligible preview"
            fi
            assert_absent "$FAKE/usr/share/decky-loader/PluginLoader"
            assert_absent "$FAKE/etc/systemd/system/plugin_loader.service"
            ;;
    esac
}

run_case() {
    local name=$1 fn=$2
    shift 2
    printf '\n== %s ==\n' "$name"
    CASE=$name
    CASE_FAILED=0
    "$fn" "$@"
    if [[ $CASE_FAILED -eq 0 ]]; then
        printf 'PASS %s\n' "$name"
    else
        printf 'FAIL %s\n' "$name" >&2
        FAILED=$((FAILED + 1))
    fi
}

# Expected service content is identical for every case; build it once.
make_fixtures "$FIXTURES/_expected" success
sed 's#${HOMEBREW_FOLDER}#/var/home/armada/homebrew#g' \
    "$FIXTURES/_expected/service-prerelease.txt" >"$WORK/service.expected"

run_case success success_case
run_case prefetched-success success_case prefetched-success prefetch
run_case missing-arm64 refusal_case missing-arm64
run_case digest-mismatch refusal_case digest-mismatch
run_case wrong-arch refusal_case wrong-arch
run_case no-preview refusal_case no-preview
run_case prefetched-empty refusal_case prefetched-empty success prefetch-empty
run_case prefetched-malformed refusal_case prefetched-malformed success prefetch-malformed
run_case prefetched-digest-mismatch refusal_case prefetched-digest-mismatch digest-mismatch prefetch
run_case prefetched-wrong-arch refusal_case prefetched-wrong-arch wrong-arch prefetch

if [[ $FAILED -ne 0 ]]; then
    printf '\n%d case(s) failed\n' "$FAILED" >&2
    exit 1
fi
printf '\ndecky bundle tests passed\n'
