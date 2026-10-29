export image_name := env("IMAGE_NAME", "armada")
export default_tag := env("DEFAULT_TAG", "latest")
export bib_image := env("BIB_IMAGE", "quay.io/centos-bootc/bootc-image-builder:latest")

import? 'Justfile.local'

alias build-vm := build-qcow2
alias rebuild-vm := rebuild-qcow2
alias run-vm := run-vm-qcow2

[private]
default:
    @just --list

[group('Just')]
check: test
    #!/usr/bin/bash
    find . -type f -name "*.just" | while read -r file; do
    	echo "Checking syntax: $file"
    	just --unstable --fmt --check -f $file
    done
    echo "Checking syntax: Justfile"
    just --unstable --fmt --check -f Justfile

[group('Test')]
test:
    #!/usr/bin/env bash
    set -euo pipefail
    shopt -s nullglob
    for test_file in tests/*-test.sh; do
        echo "Running test: ${test_file}"
        bash "${test_file}"
    done
    python3 -m unittest discover -s tests -p test_plugin_sources.py

# EDL filesystem tests; needs privileged containers, separate from `just test`
[group('Test')]
test-edl-package:
    #!/usr/bin/env bash
    set -euo pipefail

    source="$(git rev-parse --show-toplevel)"
    # A per-invocation tag avoids clobbering or deleting another run's image.
    tools_image="localhost/armada/edl-tools:test-$$"
    build_ok=0
    cleanup() {
        # Only the tag this run created, and only when this run built it; a
        # non-force removal never touches shared layers or other tags.
        if [[ "${build_ok}" -eq 1 ]]; then
            docker image rm "${tools_image}" || true
        fi
    }
    trap cleanup EXIT

    docker build \
        --file "${source}/build_files/Containerfile.edl-tools" \
        --tag "${tools_image}" \
        "${source}/build_files"
    build_ok=1

    # The tests create loop devices and btrfs filesystems, so /work is a local
    # scratch directory inside the checkout rather than a shared tmpfs.
    work="$(mktemp -d "${source}/_build-edl-test.XXXXXX")"
    docker run \
        --rm \
        --privileged \
        --cpus 4 \
        --memory 6g \
        --volume "${source}:/source:ro" \
        --volume "${work}:/work" \
        --env TMPDIR=/work \
        --env PYTHONDONTWRITEBYTECODE=1 \
        --env TEST_ARTIFACT_ROOT=/work \
        --workdir /source \
        "${tools_image}" \
        bash -c 'set -euo pipefail
            python3 -m unittest discover -s tests -p "test_edl*.py"
            python3 tests/test_split_package_archive.py'

[group('Just')]
fix:
    #!/usr/bin/bash
    find . -type f -name "*.just" | while read -r file; do
    	echo "Checking syntax: $file"
    	just --unstable --fmt -f $file
    done
    echo "Checking syntax: Justfile"
    just --unstable --fmt -f Justfile || { exit 1; }

[group('Utility')]
clean:
    #!/usr/bin/bash
    set -eoux pipefail
    touch _build
    find *_build* -exec rm -rf {} \;
    rm -f previous.manifest.json
    rm -f changelog.md
    rm -f output.env
    rm -rf output/

[group('Utility')]
[private]
sudo-clean:
    just sudoif just clean

[group('Utility')]
[private]
sudoif command *args:
    #!/usr/bin/bash
    function sudoif(){
        if [[ "${UID}" -eq 0 ]]; then
            "$@"
        elif [[ "$(command -v sudo)" && -n "${SSH_ASKPASS:-}" ]] && [[ -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ]]; then
            /usr/bin/sudo --askpass "$@" || exit 1
        elif [[ "$(command -v sudo)" ]]; then
            /usr/bin/sudo "$@" || exit 1
        else
            exit 1
        fi
    }
    sudoif {{ command }} {{ args }}

# Registry holding published package images (override to use a fork's)
export pkg_registry := env("ARMADA_PKG_REGISTRY", "ghcr.io/mydanyi/armada/pkg")

# Registry consulted when a package is absent from pkg_registry
# (override to use another source)
export pkg_fallback_registry := env("ARMADA_PKG_FALLBACK_REGISTRY", "ghcr.io/armada-os/armada/pkg")

# Build one package and tag it by content hash, the way CI does
[group('Packages')]
package name:
    #!/usr/bin/env bash
    set -euo pipefail
    source packages/toolchain.env
    source packages/mesa-x86/BASE.env
    [ -d "packages/{{ name }}" ] || { echo "unknown package: {{ name }}" >&2; exit 1; }
    hash="$(packages/package-hash.sh {{ name }})"
    buildah build \
        -f packages/Containerfile \
        --target "out-{{ name }}" \
        --layers \
        --build-arg "BUILDER_IMAGE=${BUILDER_IMAGE}" \
        --build-arg "GUEST_BUILDER_IMAGE=${GUEST_BUILDER_IMAGE}" \
        -t "localhost/armada/pkg/{{ name }}:${hash}" \
        packages/
    echo "==> localhost/armada/pkg/{{ name }}:${hash}"

# Build every package (slow from cold, unchanged ones are layer-cache hits)
[group('Packages')]
packages:
    #!/usr/bin/env bash
    set -euo pipefail
    for dir in packages/*/; do
        pkg="$(basename "${dir}")"
        [ -f "${dir}/build.sh" ] || continue
        echo "==> ${pkg}"
        just package "${pkg}"
    done

# Show each package's content hash and whether it is built locally
[group('Packages')]
packages-status:
    #!/usr/bin/env bash
    set -euo pipefail
    failed=0
    for dir in packages/*/; do
        pkg="$(basename "${dir}")"
        [ -f "${dir}/build.sh" ] || continue
        hash="$(packages/package-hash.sh "${pkg}")"
        if podman image exists "localhost/armada/pkg/${pkg}:${hash}" 2>/dev/null; then
            printf '  %-24s %s  local\n' "${pkg}" "${hash}"
            continue
        fi
        status=0
        actual="$(bash packages/resolve-package-ref.sh "${pkg}" "${hash}" "${pkg_registry}" "${pkg_fallback_registry}")" || status=$?
        if [ "${status}" -eq 0 ]; then
            printf '  %-24s %s  %s\n' "${pkg}" "${hash}" "${actual}"
        elif [ "${status}" -eq 3 ]; then
            printf '  %-24s %s  missing\n' "${pkg}" "${hash}"
            failed=1
        else
            printf '  %-24s %s  lookup-error (exit %s)\n' "${pkg}" "${hash}" "${status}"
            failed=1
        fi
    done
    exit "${failed:-0}"

build $target_image=image_name $tag=default_tag:
    #!/usr/bin/env bash
    set -euo pipefail

    # Resolve the latest bundled plugin sources before any package lookup or
    # image build, so the layers never come from a cached earlier resolution.
    python3 build_files/resolve-plugin-sources.py

    # Resolve every package to an image: one built locally if present, else
    # the published one for that content hash.
    REFS="$(mktemp)"
    trap 'rm -f "${REFS}"' EXIT
    for dir in packages/*/; do
        pkg="$(basename "${dir}")"
        [ -f "${dir}/build.sh" ] || continue
        hash="$(packages/package-hash.sh "${pkg}")"
        var="$(echo "${pkg}" | tr '[:lower:]-' '[:upper:]_')_REF"
        local_ref="localhost/armada/pkg/${pkg}:${hash}"
        if podman image exists "${local_ref}" 2>/dev/null; then
            echo "${var}=${local_ref}" >> "${REFS}"
            continue
        fi
        status=0
        actual="$(bash packages/resolve-package-ref.sh "${pkg}" "${hash}" "${pkg_registry}" "${pkg_fallback_registry}")" || status=$?
        if [ "${status}" -eq 0 ]; then
            echo "${var}=${actual}" >> "${REFS}"
        elif [ "${status}" -eq 3 ]; then
            echo "${pkg}:${hash} is not built locally and is not published; run 'just package ${pkg}' first" >&2
            exit 1
        else
            echo "Failed to resolve ${pkg}:${hash} from published registries (exit ${status})" >&2
            exit "${status}"
        fi
    done
    expected=$(grep -c '^ARG [A-Z0-9_]*_REF$' Containerfile)
    got=$(wc -l < "${REFS}")
    [ "${expected}" -eq "${got}" ] || { echo "Containerfile wants ${expected} refs, resolved ${got}" >&2; exit 1; }

    ARMADA_VERSION="$(TZ=America/New_York date +%Y%m%d).$(git rev-parse --short HEAD)"
    ARMADA_REVISION="$(git rev-parse HEAD)"

    SECRET_ARGS=()
    if [[ -n "${GITHUB_TOKEN:-}" ]]; then
        SECRET_ARGS+=("--secret" "id=GITHUB_TOKEN,env=GITHUB_TOKEN")
    fi

    podman build \
        --build-arg "ARMADA_VERSION=${ARMADA_VERSION}" \
        --build-arg "ARMADA_REVISION=${ARMADA_REVISION}" \
        --build-arg-file "${REFS}" \
        "${SECRET_ARGS[@]}" \
        --platform linux/arm64 \
        --pull=newer \
        --target armada-rootfs \
        --tag "${target_image}:${tag}" \
        .

_rootful_load_image $target_image=image_name $tag=default_tag:
    #!/usr/bin/bash
    set -eoux pipefail

    if [[ -n "${ARMADA_IMAGE_DIGEST:-}" ]]; then
        if ! [[ "${ARMADA_IMAGE_DIGEST}" =~ ^sha256:[a-f0-9]{64}$ ]]; then
            echo "Invalid ARMADA_IMAGE_DIGEST: expected sha256 followed by 64 lowercase hex digits" >&2
            exit 1
        fi
        just sudoif podman pull "${target_image}@${ARMADA_IMAGE_DIGEST}"
        # Pin build content while keeping the installed system on its channel.
        just sudoif podman tag "${target_image}@${ARMADA_IMAGE_DIGEST}" "${target_image}:${tag}"
        exit 0
    fi

    if [[ -n "${SUDO_USER:-}" || "${UID}" -eq "0" ]]; then
        # Always re-pull a remote tag so the disk uses the freshly published
        # image, not a stale cached one; localhost builds are already loaded.
        if [[ "${target_image}" != localhost/* ]]; then
            sudo podman pull "${target_image}:${tag}"
        fi
        exit 0
    fi

    set +e
    resolved_tag=$(podman inspect -t image "${target_image}:${tag}" | jq -r '.[].RepoTags.[0]')
    return_code=$?
    set -e

    USER_IMG_ID=$(podman images --filter reference="${target_image}:${tag}" --format "'{{ '{{.ID}}' }}'")

    if [[ $return_code -eq 0 ]]; then
        ID=$(just sudoif podman images --filter reference="${target_image}:${tag}" --format "'{{ '{{.ID}}' }}'")
        if [[ "$ID" != "$USER_IMG_ID" ]]; then
            # BIB runs as root, so rootful podman needs the image.
            just sudoif podman image scp ${UID}@localhost::"${target_image}:${tag}" root@localhost::"${target_image}:${tag}"
        fi
    else
        just sudoif podman pull "${target_image}:${tag}"
    fi

_build-bib $target_image $tag $type $config: (_rootful_load_image target_image tag)
    #!/usr/bin/env bash
    set -euo pipefail

    args="--type ${type} "
    args+="--use-librepo=True "
    args+="--rootfs=btrfs "
    args+="--target-arch arm64"

    # BIB leaves /var/tmp scratch when overlay umount races --rm.
    sudo rm -rf /var/tmp/podman[0-9]* /var/tmp/buildah-cache-[0-9]* || true
    # A failed build leaves its ~12G BUILDTMP behind (removed only on success).
    sudo rm -rf "${PWD}"/_build-bib.* || true

    BUILDTMP=$(mktemp -p "${PWD}" -d -t _build-bib.XXXXXXXXXX)

    # x86_64 qemu-user hosts need runc; crun trips a memfd bug there.
    EXTRA_MOUNTS=()
    if [ -d /etc/containers/containers.conf.d ]; then
        EXTRA_MOUNTS+=("-v" "/etc/containers/containers.conf.d:/etc/containers/containers.conf.d:ro")
    fi
    if [ -f /tmp/armada-runc/runc-arm64 ]; then
        EXTRA_MOUNTS+=("-v" "/tmp/armada-runc/runc-arm64:/usr/bin/runc:ro")
    fi

    # Ubuntu 24.04 AppArmor blocks mknod in nested crun.
    tty_args=()
    if [[ -t 0 ]]; then
        tty_args+=("-it")
    fi

    sudo podman run \
      --rm \
      "${tty_args[@]}" \
      --privileged \
      --pull=newer \
      --net=host \
      --platform linux/arm64 \
      --security-opt label=type:unconfined_t \
      --security-opt apparmor=unconfined \
      --cap-add=CAP_MKNOD \
      -v $(pwd)/${config}:/config.toml:ro \
      -v $BUILDTMP:/output \
      -v /var/lib/containers/storage:/var/lib/containers/storage \
      "${EXTRA_MOUNTS[@]}" \
      "${bib_image}" \
      ${args} \
      "${target_image}:${tag}"

    # BIB writes output as root.
    if [ -x /opt/armada/bin/reown ]; then
        sudo /opt/armada/bin/reown "$BUILDTMP"
    else
        sudo chown -R "$(id -u):$(id -g)" "$BUILDTMP"
    fi
    # mv cannot overwrite a non-empty dir from the previous build.
    mkdir -p output
    rm -rf output/image
    mv -f $BUILDTMP/* output/
    rmdir $BUILDTMP

    # Each rebuild leaves untagged layers in both storages.
    sudo podman image prune -f 2>&1 | tail -3 || true
    podman image prune -f 2>&1 | tail -3 || true

_rebuild-bib $target_image $tag $type $config: (build target_image tag) && (_build-bib target_image tag type config)

[group('Build Virtual Machine Image')]
build-qcow2 $target_image=("localhost/" + image_name) $tag=default_tag: && (_build-bib target_image tag "qcow2" "disk_config/disk.toml")

[group('Build Virtual Machine Image')]
build-raw $target_image=("localhost/" + image_name) $tag=default_tag: && (_build-bib target_image tag "raw" "disk_config/disk.toml")

[group('Build Virtual Machine Image')]
build-iso $target_image=("localhost/" + image_name) $tag=default_tag: && (_build-bib target_image tag "iso" "disk_config/iso.toml")

# Output: ./output/armada-<version>.img.gz  (version = container label, date.sha)
[group('Armada')]
build-armada-image $target_image=("localhost/" + image_name) $tag=default_tag: (build-raw target_image tag)
    #!/usr/bin/env bash
    set -euo pipefail
    echo "Finalizing the freshly-built raw image..."
    version=$(podman inspect -t image "${target_image}:${tag}" \
                | jq -r '.[0].Config.Labels["org.opencontainers.image.version"] // empty')
    ./post_process/make-bootimg.sh output/image/disk.raw
    # Name from the container's version so a flashed device traces to its build.
    if [[ -n "$version" && "$version" != unknown ]]; then
        export OUT="output/armada-${version}.img.gz"
    fi
    ./post_process/finalize-armada-image.sh output/image/disk.raw

# Build the 9008/EDL package from IMAGE (OCI ref with channel, or an absolute
# OCI layout directory), DIGEST, REVISION and VERSION; ORIGIN is required for a
# layout directory. Output: ./output/edl/<version>/
[group('Armada')]
build-edl-package $image $digest $revision $version $origin="":
    #!/usr/bin/env bash
    set -euo pipefail

    source="$(git rev-parse --show-toplevel)"
    # The wrapper needs a fresh work path that does not exist yet, so create
    # only the parent; both names are covered by the _build* ignore rule.
    build_parent="$(mktemp -d "${source}/_build-edl.XXXXXX")"
    output="${source}/output/edl/${version}"

    # The wrapper owns Docker, its tools/runtime images, the OCI pull and the
    # volume split; the recipe only supplies the source, work and output paths.
    bash "${source}/build_files/build-edl-package.sh" \
        --oci "${image}" \
        --manifest-digest "${digest}" \
        --source-commit "${revision}" \
        --version "${version}" \
        ${origin:+--origin "${origin}"} \
        --source "${source}" \
        --work "${build_parent}/work" \
        --output "${output}"

# Build the 9008/EDL package from a locally built image (just build), using the
# image's own version and revision labels. ORIGIN is the signed origin the
# devices update from (ostree-image-signed:docker://ghcr.io/mydanyi/
# armadaos-freedom:stable|beta|testing). Output: ./output/edl/<version>/
[group('Armada')]
build-edl-package-local $origin $target_image=("localhost/" + image_name) $tag=default_tag:
    #!/usr/bin/env bash
    set -euo pipefail

    source="$(git rev-parse --show-toplevel)"
    command -v skopeo >/dev/null || { echo "skopeo is required to export the local image" >&2; exit 1; }
    ref="containers-storage:${target_image}:${tag}"
    # Check the labels before exporting the image.
    version="$(skopeo inspect --format '{{{{ index .Labels "org.opencontainers.image.version" }}' "${ref}")"
    revision="$(skopeo inspect --format '{{{{ index .Labels "org.opencontainers.image.revision" }}' "${ref}")"
    if [[ -z "${version}" || "${version}" == unknown || "${version}" == "<no value>" || ! "${revision}" =~ ^[0-9a-f]{40}$ ]]; then
        echo "${target_image}:${tag} has no version/revision labels (version '${version}', revision '${revision}'); rebuild it with 'just build'" >&2
        exit 1
    fi
    # The OCI copy is several GB; it is only needed while the package is built.
    oci_parent="$(mktemp -d "${source}/_build-edl-oci.XXXXXX")"
    trap 'rm -rf "${oci_parent}"' EXIT
    oci="${oci_parent}/oci"
    # The producer reads the image as oci:<dir>:latest.
    skopeo copy "${ref}" "oci:${oci}:latest"
    digest="$(skopeo inspect --format '{{{{ .Digest }}' "oci:${oci}:latest")"
    just build-edl-package "${oci}" "${digest}" "${revision}" "${version}" "${origin}"

[group('Build Virtual Machine Image')]
rebuild-qcow2 $target_image=("localhost/" + image_name) $tag=default_tag: && (_rebuild-bib target_image tag "qcow2" "disk_config/disk.toml")

[group('Build Virtual Machine Image')]
rebuild-raw $target_image=("localhost/" + image_name) $tag=default_tag: && (_rebuild-bib target_image tag "raw" "disk_config/disk.toml")

[group('Build Virtual Machine Image')]
rebuild-iso $target_image=("localhost/" + image_name) $tag=default_tag: && (_rebuild-bib target_image tag "iso" "disk_config/iso.toml")

_run-vm $target_image $tag $type $config:
    #!/usr/bin/bash
    set -eoux pipefail

    image_file="output/${type}/disk.${type}"
    if [[ $type == iso ]]; then
        image_file="output/bootiso/install.iso"
    fi

    if [[ ! -f "${image_file}" ]]; then
        just "build-${type}" "$target_image" "$tag"
    fi

    port=8006
    while grep -q :${port} <<< $(ss -tunalp); do
        port=$(( port + 1 ))
    done
    echo "Using Port: ${port}"
    echo "Connect to http://localhost:${port}"

    run_args=()
    run_args+=(--rm --privileged)
    run_args+=(--pull=newer)
    run_args+=(--publish "127.0.0.1:${port}:8006")
    run_args+=(--env "CPU_CORES=4")
    run_args+=(--env "RAM_SIZE=8G")
    run_args+=(--env "DISK_SIZE=64G")
    run_args+=(--env "TPM=Y")
    run_args+=(--env "GPU=Y")
    run_args+=(--device=/dev/kvm)
    run_args+=(--volume "${PWD}/${image_file}":"/boot.${type}")
    run_args+=(docker.io/qemux/qemu)

    (sleep 30 && xdg-open http://localhost:"$port") &
    podman run "${run_args[@]}"

[group('Run Virtual Machine')]
run-vm-qcow2 $target_image=("localhost/" + image_name) $tag=default_tag: && (_run-vm target_image tag "qcow2" "disk_config/disk.toml")

[group('Run Virtual Machine')]
run-vm-raw $target_image=("localhost/" + image_name) $tag=default_tag: && (_run-vm target_image tag "raw" "disk_config/disk.toml")

[group('Run Virtual Machine')]
run-vm-iso $target_image=("localhost/" + image_name) $tag=default_tag: && (_run-vm target_image tag "iso" "disk_config/iso.toml")

[group('Run Virtual Machine')]
spawn-vm rebuild="0" type="qcow2" ram="6G":
    #!/usr/bin/env bash

    set -euo pipefail

    [ "{{ rebuild }}" -eq 1 ] && echo "Rebuilding the ISO" && just build-vm {{ rebuild }} {{ type }}

    systemd-vmspawn \
      -M "bootc-image" \
      --console=gui \
      --cpus=2 \
      --ram=$(echo {{ ram }}| /usr/bin/numfmt --from=iec) \
      --network-user-mode \
      --vsock=false --pass-ssh-key=false \
      -i ./output/**/*.{{ type }}

lint:
    #!/usr/bin/env bash
    set -eoux pipefail
    if ! command -v shellcheck &> /dev/null; then
        echo "shellcheck could not be found. Please install it."
        exit 1
    fi
    /usr/bin/find . -iname "*.sh" -type f -exec shellcheck "{}" ';'

format:
    #!/usr/bin/env bash
    set -eoux pipefail
    if ! command -v shfmt &> /dev/null; then
        echo "shfmt could not be found. Please install it."
        exit 1
    fi
    /usr/bin/find . -iname "*.sh" -type f -exec shfmt --write "{}" ';'
