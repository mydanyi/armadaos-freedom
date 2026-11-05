#!/bin/bash
set -euxo pipefail

# Copy dist from the image build stage, not the source tree. The destination
# keeps the historical armada-control directory name; the source defaults to the
# in-tree plugin and can be overridden for dependency-owned sources.
install_plugin() {
    local name=$1 dist=$2 src=${3:-/ctx/decky/$1} dest=/usr/share/decky-plugins/$1
    install -d -m 0755 "${dest}"
    cp -a "${src}/plugin.json" "${src}/package.json" "${src}/main.py" "${dest}/"
    cp -a "${src}/py_modules" "${dest}/"
    [[ ! -f "${src}/catalog.json" ]] || cp -a "${src}/catalog.json" "${dest}/"
    [[ ! -d "${src}/templates" ]] || cp -a "${src}/templates" "${dest}/"
    cp -a "${dist}" "${dest}/dist"
    rm -f "${dest}/dist/"*.map
    find "${dest}" -name __pycache__ -type d -prune -exec rm -rf {} +
}
install_plugin armada-control /packages/decky-dist /ctx/decky/freedom-control/decky/armada-control
install_plugin armada-store /packages/decky-store-dist
chmod 0755 /usr/lib/decky-loader/armada-decky-sync

decky_release_jq='
            [.[] | select(.draft != true and .prerelease == true)]
            | if length == 0
              then error("no Decky Loader preview release found")
              else max_by(.published_at)
              end
        '
if [[ -e /run/secrets/decky_releases ]]; then
    decky_release="$(jq -ce "${decky_release_jq}" /run/secrets/decky_releases)"
else
    decky_release="$(
        curl --retry 12 --retry-delay 10 -fsSL \
            'https://api.github.com/repos/SteamDeckHomebrew/decky-loader/releases?per_page=100' |
            jq -ce "${decky_release_jq}"
    )"
fi
decky_version="$(jq -er '.tag_name | select(type == "string" and length > 0)' <<<"${decky_release}")"
[[ "${decky_version}" =~ ^[A-Za-z0-9._+-]+$ ]]
[[ "$(jq -r '.draft // false' <<<"${decky_release}")" == "false" ]]
[[ "$(jq -r '.prerelease // false' <<<"${decky_release}")" == "true" ]]

decky_asset="$(
    jq -c '[.assets[]? | select(.name == "PluginLoader-arm64")] | if length == 1 then .[0] else empty end' \
        <<<"${decky_release}"
)"
[[ -n "${decky_asset}" ]]
decky_url="$(jq -er '.browser_download_url | select(type == "string" and length > 0)' <<<"${decky_asset}")"
decky_digest="$(jq -er '.digest | select(type == "string" and length > 0)' <<<"${decky_asset}")"
[[ "${decky_digest}" =~ ^sha256:[0-9a-fA-F]{64}$ ]]
decky_service_url="https://raw.githubusercontent.com/SteamDeckHomebrew/decky-loader/${decky_version}/dist/plugin_loader-prerelease.service"

install -d -m 0755 /usr/share/decky-loader
decky_loader_tmp="$(mktemp)"
decky_service_tmp="$(mktemp)"
cleanup_decky_tmp() {
    rm -f "${decky_loader_tmp}" "${decky_service_tmp}"
}
trap cleanup_decky_tmp EXIT

curl --retry 12 --retry-delay 10 -fL -o "${decky_loader_tmp}" "${decky_url}"
printf '%s  %s\n' "${decky_digest#sha256:}" "${decky_loader_tmp}" | sha256sum -c -
python3 - "${decky_loader_tmp}" <<'PY'
import struct
import sys

with open(sys.argv[1], "rb") as handle:
    header = handle.read(20)
if (
    len(header) != 20
    or header[:4] != b"\x7fELF"
    or header[4] != 2
    or header[5] != 1
    or struct.unpack_from("<H", header, 18)[0] != 183
):
    raise SystemExit("Decky PluginLoader payload is not a 64-bit little-endian AArch64 ELF")
PY

install -m 0755 "${decky_loader_tmp}" /usr/share/decky-loader/PluginLoader
printf '%s\n' "${decky_version}" > /usr/share/decky-loader/.loader.version

curl --retry 12 --retry-delay 10 -fsSL "${decky_service_url}" |
    sed 's#${HOMEBREW_FOLDER}#/var/home/armada/homebrew#g' \
        >"${decky_service_tmp}"
install -D -m 0644 "${decky_service_tmp}" /etc/systemd/system/plugin_loader.service

systemctl enable armada-decky-sync.service
systemctl enable plugin_loader.service
