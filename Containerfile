ARG CHUNKAH_IMAGE=quay.io/coreos/chunkah@sha256:ff8b8b466a942ec6000445d4001fc661e2fc5a952ad9ee29b4de9ab09d1d1708
ARG BASE_IMAGE=quay.io/fedora/fedora-bootc:44

# Package images, resolved by content hash. The Packages workflow publishes each
# as ghcr.io/<owner>/armada/pkg/<name>:<tag>, tagged by packages/package-hash.sh
# from that package's sources, and passes the refs in as build args.

ARG STEAM_BOOTSTRAP_REF
ARG FEX_REF
ARG MESA_REF
ARG MANGOHUD_REF
ARG GAMESCOPE_REF
ARG GAMESCOPE_SESSION_REF
ARG GAMESCOPE_SESSION_STEAM_REF
ARG KWIN_REF
ARG PLASMA_MOBILE_REF
ARG POWERDEVIL_REF
ARG PROTONTRICKS_REF
ARG KERNEL_REF
ARG INPUTPLUMBER_REF
ARG STEAMOS_MANAGER_REF
ARG NETWORKMANAGER_REF
ARG WPA_SUPPLICANT_REF
ARG SCX_SCHEDS_REF
ARG JUPITER_HW_SUPPORT_REF
ARG MESA_ANDROID_REF
ARG LEPTON_REF
ARG MESA_X86_REF
ARG EXTEST_REF
ARG ARMADA_SPLASH_REF
ARG ARMADA_RGB_REF
ARG UMTP_RESPONDER_REF
ARG KSHIM_TOOLS_REF
ARG BTOP_CN_REF

FROM ${STEAM_BOOTSTRAP_REF} AS steam-bootstrap

FROM ${FEX_REF} AS fex

FROM ${MESA_REF} AS mesa

FROM ${MANGOHUD_REF} AS mangohud

FROM ${GAMESCOPE_REF} AS gamescope

FROM ${GAMESCOPE_SESSION_REF} AS gamescope-session

FROM ${GAMESCOPE_SESSION_STEAM_REF} AS gamescope-session-steam

FROM ${KWIN_REF} AS kwin

FROM ${PLASMA_MOBILE_REF} AS plasma-mobile

FROM ${POWERDEVIL_REF} AS powerdevil

FROM ${PROTONTRICKS_REF} AS protontricks

FROM ${KERNEL_REF} AS kernel

FROM ${INPUTPLUMBER_REF} AS inputplumber

FROM ${STEAMOS_MANAGER_REF} AS steamos-manager

FROM ${NETWORKMANAGER_REF} AS networkmanager

FROM ${WPA_SUPPLICANT_REF} AS wpa_supplicant

FROM ${SCX_SCHEDS_REF} AS scx-scheds

FROM ${JUPITER_HW_SUPPORT_REF} AS jupiter-hw-support

FROM ${MESA_ANDROID_REF} AS mesa-android

FROM ${LEPTON_REF} AS lepton

FROM ${MESA_X86_REF} AS mesa-x86

FROM ${EXTEST_REF} AS extest

FROM ${ARMADA_SPLASH_REF} AS armada-splash

FROM ${ARMADA_RGB_REF} AS armada-rgb

FROM ${UMTP_RESPONDER_REF} AS umtp-responder

FROM ${KSHIM_TOOLS_REF} AS kshim-tools

FROM ${BTOP_CN_REF} AS btop-cn

FROM docker.io/library/node:22-slim AS decky-build
WORKDIR /build/freedom-control
COPY decky/freedom-control/decky/armada-control/package.json decky/freedom-control/decky/armada-control/package-lock.json ./
RUN npm ci
COPY decky/freedom-control/decky/armada-control/ ./
RUN npm test && npm run build
WORKDIR /build/armada-store
COPY decky/armada-store/package.json decky/armada-store/package-lock.json ./
RUN npm ci
COPY decky/armada-store/ ./
RUN npm run build

FROM scratch AS ctx
COPY build_files /build_files/
COPY decky /decky/
COPY system_files /system_files/
COPY --chmod=0755 system_files/usr/bin/armada-reboot-edl /system_files/usr/bin/armada-reboot-edl
# TB323FU's two connectors share one controller. Reuse its tested routing
# components in the common image; the helper checks the board compatible.
COPY devices/lenovo-tb323fu/rootfs/usr/libexec/tb323fu/tb323fu-usb-port /system_files/usr/libexec/tb323fu/tb323fu-usb-port
COPY devices/lenovo-tb323fu/rootfs/etc/udev/rules.d/71-tb323fu-usb-host.rules /system_files/usr/lib/udev/rules.d/71-tb323fu-usb-host.rules
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/udev/70-tb323fu-usb-port.rules /system_files/usr/lib/udev/rules.d/70-tb323fu-usb-port.rules
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/systemd/system/tb323fu-usb-port.service /system_files/usr/lib/systemd/system/tb323fu-usb-port.service
# TB323FU audio: the public ALSA UCM verb, the device topology, the vendor
# platform's mixer defaults and its speaker gain/protection pair, plus the
# fixed per-model ADSP firmware bundle. tb323fu-audio-prepare stages them on
# the exact lenovo,baldur board only; the bundle is verified at build time
# against devices/lenovo-tb323fu/firmware/audio/SHA256SUMS.
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/alsa/ucm2/conf.d/kaanapali/LENOVO-TB323FU.conf /tb323fu-audio/ucm2/conf.d/kaanapali/LENOVO-TB323FU.conf
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/alsa/ucm2/Lenovo/TB323FU/HiFi.conf /tb323fu-audio/ucm2/Lenovo/TB323FU/HiFi.conf
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/firmware/audio/LENOVO-TB323FU-tplg.bin /tb323fu-audio/LENOVO-TB323FU-tplg.bin
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/libexec/tb323fu-audio-defaults /tb323fu-audio/tb323fu-audio-defaults
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/udev/91-tb323fu-audio.rules /tb323fu-audio/91-tb323fu-audio.rules
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/systemd/system/tb323fu-audio.service /tb323fu-audio/tb323fu-audio.service
COPY devices/lenovo-tb323fu/kernel/tb323fu-linux-11a25bf8/tb323fu-linux-11a25bf89b65fff48112a91a0b25bc247e7736d8/userspace/platform/etc/tb323fu/audio.conf /tb323fu-audio/audio.conf
COPY devices/lenovo-tb323fu/firmware/audio/ /tb323fu-audio/firmware/
COPY devices/lenovo-tb323fu/rootfs/usr/libexec/tb323fu/tb323fu-speaker-gain /tb323fu-audio/tb323fu-speaker-gain
COPY devices/lenovo-tb323fu/rootfs/usr/lib/systemd/user/tb323fu-speaker-gain.service /tb323fu-audio/tb323fu-speaker-gain.service
COPY devices/lenovo-tb323fu/rootfs/usr/share/tb323fu/pipewire/tb323fu-speakers-protection.json /tb323fu-audio/tb323fu-speakers-protection.json
COPY devices/lenovo-tb323fu/rootfs/usr/share/wireplumber/wireplumber.conf.d/61-tb323fu-speakers-dsp.conf /tb323fu-audio/61-tb323fu-speakers-dsp.conf
# Selected SM8850 companions come from Freedom Control; shared defaults and
# performance helpers remain owned by this repository.
COPY decky/freedom-control/system_files/usr/libexec/armada/armada-powerd /system_files/usr/libexec/armada/armada-powerd
COPY decky/freedom-control/system_files/usr/lib/armada/devices/lenovo-tb323fu.conf /system_files/usr/lib/armada/devices/lenovo-tb323fu.conf
COPY decky/freedom-control/system_files/usr/share/armada/power-profiles-SM8850.conf /system_files/usr/share/armada/power-profiles-SM8850.conf

FROM ${BASE_IMAGE} AS armada-rootfs
ARG ARMADA_VERSION=unknown
LABEL org.opencontainers.image.version="${ARMADA_VERSION}"
# CI sets the published labels through the chunkah config; this one is for
# local builds, so build-edl-package-local can pass the image's revision on.
ARG ARMADA_REVISION=unknown
LABEL org.opencontainers.image.revision="${ARMADA_REVISION}"

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=bind,from=steam-bootstrap,source=/steam-bootstrap,target=/packages/steam-bootstrap \
    --mount=type=bind,from=fex,source=/rpms,target=/packages/fex \
    --mount=type=bind,from=mesa,source=/rpms,target=/packages/mesa \
    --mount=type=bind,from=mangohud,source=/rpms,target=/packages/mangohud \
    --mount=type=bind,from=gamescope,source=/rpms,target=/packages/gamescope \
    --mount=type=bind,from=gamescope-session,source=/rpms,target=/packages/gamescope-session \
    --mount=type=bind,from=gamescope-session-steam,source=/rpms,target=/packages/gamescope-session-steam \
    --mount=type=bind,from=kwin,source=/rpms,target=/packages/kwin \
    --mount=type=bind,from=plasma-mobile,source=/rpms,target=/packages/plasma-mobile \
    --mount=type=bind,from=powerdevil,source=/rpms,target=/packages/powerdevil \
    --mount=type=bind,from=protontricks,source=/rpms,target=/packages/protontricks \
    --mount=type=bind,from=kernel,source=/kernel,target=/packages/kernel \
    --mount=type=bind,from=inputplumber,source=/rpms,target=/packages/inputplumber \
    --mount=type=bind,from=steamos-manager,source=/rpms,target=/packages/steamos-manager \
    --mount=type=bind,from=networkmanager,source=/rpms,target=/packages/networkmanager \
    --mount=type=bind,from=wpa_supplicant,source=/rpms,target=/packages/wpa_supplicant \
    --mount=type=bind,from=scx-scheds,source=/rpms,target=/packages/scx-scheds \
    --mount=type=bind,from=jupiter-hw-support,source=/rpms,target=/packages/jupiter-hw-support \
    --mount=type=bind,from=mesa-android,source=/,target=/packages/mesa-android \
    --mount=type=bind,from=lepton,source=/rpms,target=/packages/lepton \
    --mount=type=bind,from=mesa-x86,source=/,target=/packages/mesa-x86 \
    --mount=type=bind,from=extest,source=/,target=/packages/extest \
    --mount=type=bind,from=armada-splash,source=/rpms,target=/packages/armada-splash \
    --mount=type=bind,from=armada-rgb,source=/rpms,target=/packages/armada-rgb \
    --mount=type=bind,from=umtp-responder,source=/rpms,target=/packages/umtp-responder \
    --mount=type=bind,from=kshim-tools,source=/kshim-tools,target=/packages/kshim-tools \
    --mount=type=bind,from=btop-cn,source=/rpms,target=/packages/btop-cn \
    --mount=type=bind,from=decky-build,source=/build/freedom-control/dist,target=/packages/decky-dist \
    --mount=type=bind,from=decky-build,source=/build/armada-store/dist,target=/packages/decky-store-dist \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    mkdir -p /usr/lib/armada && \
    printf '%s\n' "${ARMADA_VERSION}" >/usr/lib/armada/version && \
    /ctx/build_files/build.sh

RUN bootc container lint

FROM ${CHUNKAH_IMAGE} AS chunkah
ARG CHUNKAH_CONFIG_STR
RUN --mount=from=armada-rootfs,target=/chunkah,ro \
    /bin/bash -o pipefail -c ' \
        set -e; \
        start=${SECONDS}; \
        chunkah build --verbose --compressed --compression-level 6 \
            --arch arm64 --max-layers 128 --source-date-epoch 0 \
            --prune /sysroot/ \
            --label ostree.commit- --label ostree.final-diffid- \
            --config-str "${CHUNKAH_CONFIG_STR}" \
            --output oci:/run/src/chunked 2>&1 | tee /run/src/chunkah.log; \
        echo "Chunkah completed in $((SECONDS - start)) seconds" \
    '

FROM armada-rootfs AS armada
