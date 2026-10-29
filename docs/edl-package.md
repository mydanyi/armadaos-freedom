# EDL package build

**English** · [简体中文](edl-package.zh-CN.md)

The EDL package is the 9008 deployment path: it carries the raw images,
kernels, device trees, and metadata that the Windows installer writes over
Qualcomm 9008 (EDL) mode. It is built as a separate artifact from the flashable
SD card image, and it does not replace the existing SD image path.

## Requirements

- A Linux host with Docker, where containers may use loop devices and
  `SYS_ADMIN` for filesystem mounts, with btrfs support on the host.
- Python 3 on the host. The wrapper runs a host helper before it invokes
  Docker.
- `just` and `git`, with submodules checked out recursively.
- Free disk space for the raw images, their ZIP volumes, the pulled container
  image, and the container images the build creates locally.

```console
$ git submodule update --init --recursive
```

## Build a package

```console
$ just build-edl-package \
    docker://ghcr.io/mydanyi/armadaos-freedom:testing \
    sha256:DIGEST \
    FULL_SOURCE_COMMIT \
    EXACT_OCI_VERSION
```

| Argument | Value |
|---|---|
| `IMAGE` | Registry reference that includes a real accepted channel (`stable`, `beta`, or `testing`), e.g. `docker://ghcr.io/mydanyi/armadaos-freedom:testing`. A `docker://` prefix is accepted but not required, because the wrapper also takes a bare registry reference. The build derives the signed image origin from the channel, and `staging` cannot derive an accepted signed origin. |
| `DIGEST` | Exact `sha256:` OCI manifest digest of that reference, supplied separately from the tag. |
| `REVISION` | Full 40-character lowercase commit SHA the image was built from. |
| `VERSION` | Exact `org.opencontainers.image.version` label value of that image. |

All four arguments must describe one build. The checked-out `HEAD` must equal
`REVISION`, the digest must resolve to the requested image, and the image's
`org.opencontainers.image.revision` and `org.opencontainers.image.version`
labels must match `REVISION` and `VERSION`. The build stops on a mismatch.
A channel name or tag is not a version, and an older commit may not be paired
with a newer image.

The recipe takes exactly these four positional arguments and forwards no model
or configuration options, so it packages every profile registered in
`build_files/edl-devices.json`. It writes the package to `output/edl/<version>/`
and uses a fresh `_build-edl.*` work directory under the checkout, which is
ignored by Git.

To package a subset, or to use another configuration file, call the wrapper
directly with the same four values:

```console
$ bash /PATH/TO/repo/build_files/build-edl-package.sh \
    --oci docker://ghcr.io/mydanyi/armadaos-freedom:testing \
    --manifest-digest sha256:DIGEST \
    --source-commit FULL_SOURCE_COMMIT \
    --version EXACT_OCI_VERSION \
    --source /PATH/TO/repo \
    --devices-config /PATH/TO/repo/build_files/edl-devices.json \
    --work /PATH/TO/edl-work \
    --output /PATH/TO/edl-output \
    --model TB322FC
```

`--model` is repeatable, and only the named profiles are selected; without it
every profile in the configuration is packaged. `--devices-config` defaults to
`build_files/edl-devices.json` in the source tree.

`--work` and `--output` must be absolute and must be new or empty. Neither may
be the source root or contain the source tree, and the two must not be equal or
nested inside each other. Fresh directories inside the source tree are allowed,
which is how the `just` recipe uses `_build-edl.*` and `output/edl` there. The
full ZIP stays in the work directory; only the split volumes, their checksums,
and the package README are written to the output directory.

Each selected model needs one runtime. If `--runtime MODEL=ABSOLUTE_FILE` is
given, it must cover every selected model exactly once and no runtime is built;
otherwise every selected runtime is built in a container from the runtime
snapshot that the configuration names.

## Package a locally built image

An image built with `just build` carries its version label and a revision
label, the commit checked out when it was built. Package it with

```console
$ just build-edl-package-local ORIGIN [IMAGE] [TAG]
```

`ORIGIN` is the signed origin the installed system updates from. A local
image has no channel to derive it from, and the wrapper only accepts
`ostree-image-signed:docker://ghcr.io/mydanyi/armadaos-freedom:` followed by
`stable`, `beta` or `testing` (optionally `@sha256:...`). `IMAGE` and `TAG`
default to the image `just build` writes. The recipe reads the version and
revision labels with `skopeo`, copies the image from local container storage
into a temporary OCI layout (the producer reads it as `oci:<dir>:latest`, and
the copy, several GB, is removed afterwards), and calls `build-edl-package`
with the layout, its manifest digest, the labels and `ORIGIN`. As there, the
checked-out `HEAD` must be the commit the image was built from, and every
registered profile is packaged. An image without the labels, for example one
built by a plain `podman build`, is rejected before anything is copied.

Local hosts differ from the CI runner in a few ways:

- `skopeo` must be installed. The wrapper runs `docker` with
  `--cap-add SYS_ADMIN` and loop devices; on a podman host, a `docker` command
  that calls **rootful** podman (for example `podman-docker` in a rootful
  podman machine) works.
- The producer needs two unused `/dev/loopN` devices and stops with "two
  unused /dev/loopN devices are required ...; found N" otherwise. A minimal VM
  may only have `/dev/loop-control`; loop devices can be added through it, for
  example
  `sudo python3 -c 'import fcntl,os; fd=os.open("/dev/loop-control",os.O_RDWR); [fcntl.ioctl(fd,0x4C80,i) for i in range(4)]'`
  (`LOOP_CTL_ADD` for loop0-3).
- On an SELinux host (for example a Fedora CoreOS podman machine), the
  bind-mounted paths (source, work, and any `--devices-config` or `--runtime`
  files) either need the `container_file_t` label, or the containers need to
  run with `label = false`. Importing the image into ostree also writes the
  image's own `security.selinux` attributes: the producer container needs
  `CAP_MAC_ADMIN` for that (otherwise `EACCES`), and while the host enforces a
  policy that does not know those labels the write still fails with
  `Invalid argument`, so put the host in permissive mode (`setenforce 0`) for
  the build. A `containers.conf` override with `label = false` and
  `MAC_ADMIN` added to `default_capabilities` covers the container side; set
  `CONTAINERS_CONF_OVERRIDE` only for this command, since it applies to every
  container.

## Device configuration

`build_files/edl-devices.json` uses `schemaVersion` 1, with one global
`runtimeSource` and a `profiles` object keyed by model ID:

| Field | Meaning |
|---|---|
| `schemaVersion` | Must be `1`. |
| `runtimeSource` | Source-tree-relative path of the runtime snapshot shared by every profile. |
| `profiles` | Model ID to profile. Registered model IDs select profiles; the pipeline has no fixed model list. |
| `displayName` | Human-readable model name recorded in the package README. |
| `compatible` | `soc` and `device` compatibility strings written into the per-model manifest. |
| `dtb` | Device tree path, relative to the deployed image's DTB directory. |
| `required` | Required DTB compatibility strings. |
| `forbidden` | Excluded DTB compatibility strings. |
| `extraArgs` | Extra kernel arguments for that model's normal boot line. |
| `runtimeConfig` | Runtime build configuration, relative to the runtime snapshot. |
| `runtimeCmakeArgs` | CMake arguments used to build that model's runtime. |
| `bootPartitionBytes` | Declared boot partition size recorded in the model manifest. |
| `diagnosticMode` | `rd-break` or `verbose`. |

Packaging an adapted device still requires working Linux kernel support and a
matching device tree in the deployed image, a runtime profile in the runtime
snapshot, and a matching model registration in the Windows installer. A
configuration entry alone does not make arbitrary Android hardware supported.

## Bundled plugin sources

Plugin versions are decided when the OS image is built, not when an EDL package
is packaged. Every supported OS image build — the `just build` recipe, the
`build_push` image job, and the PR image build — first resolves all bundled
plugins from the catalogue in `decky/plugin-sources.json`:

- release entries resolve to the latest public release and its single matching
  asset, verified against the announced SHA-256 digest; no pinned older binary
  is used.
- git entries resolve the configured branch's current tip to one immutable
  40-character commit SHA and build that exact checkout.
- in-tree entries use the package in the source tree.

That resolution writes `decky/bundled-plugins/plugins.lock.json`,
`plugins.resolved.json`, and `assets/`, covering one selection. These are
generated artifacts, not committed old binaries. A source that cannot be
resolved, or an asset whose digest does not verify, fails the build; there is no
fallback to an older cache or an older lock.

The selection is fixed for that image build and for the resulting OCI image.
EDL packaging consumes that frozen image: it does not resolve plugins again and
does not silently refresh the bundled contents later.

## Continuous builds

The build also runs automatically. On `main`, the `build_edl_package` job in
[`Build images`](../.github/workflows/build.yml) follows the container image
job and builds the EDL package from the image it just pushed, using the
`testing` channel, that image's digest, its actual source revision, and its
`org.opencontainers.image.version` label. The package is uploaded as the
`edl-package-<run id>` workflow artifact next to the unchanged SD image path.
Nothing is published as a GitHub Release, and the SD image publication is
unchanged.

[`Build EDL package`](../.github/workflows/build-edl-package.yml) can also be
started by hand. It takes the same four values as the recipe, checks out the
requested revision with submodules, and uploads the split volumes, `parts.json`,
`SHA256SUMS`, and the package README as a workflow artifact.

## Package layout

The package uses `schemaVersion` 2 with deployment kind `edl-raw-v1`:

- The boot and root raw images, kernel, and initramfs are stored once and shared
  by every selected model.
- Each model keeps its own runtime plus normal and diagnostic DTB boot images,
  and its own manifest.
- Sharing the payloads does not make boot images interchangeable between
  models; keep using the matching per-model images.

The completed ZIP is split into raw byte volumes of a fixed 1900 MiB, without
recompression, named `<name>.zip.001`, `<name>.zip.002`, and so on, with the
last volume shorter. `<name>.parts.json` and `SHA256SUMS` record the volumes and
their checksums as distribution integrity records. Each volume stays below
2 GiB so it can be a release asset; that is a property of the package, not a
limit of the Actions artifact upload.

## Diagnostic boot modes

Each profile declares one diagnostic mode:

- `rd-break` appends `rd.break=pre-mount` to the normal boot arguments.
- `verbose` removes `quiet`, every `loglevel=` switch, and every `rd.break`
  variant, preserves the other normal arguments including board extras, and
  ensures `loglevel=8 ignore_loglevel printk.time=1 initcall_debug rd.debug`
  are present, appending missing flags in this order. Its output carries no
  `rd.break`.

The Gen 4 profile (TB322FC) uses `rd-break`; the Gen 5 profile (TB323FU) uses
`verbose` and supplies its board arguments through `extraArgs`.

`runtimeCmakeArgs` is a per-profile source option. The Gen 5 profile passes
`-DKSHIM_TB323FU_ANDROID_TOUCH_4M8_TRIAL=ON`, which limits the original Android
touch controller to 4.8 MHz at handoff; the Gen 4 profile passes `OFF`, and the
CMake default is `OFF`.

## Flashing with the Windows installer

Put every volume of a package in the same folder, without renaming them, and
select the `.zip.001` file in the matching installer; it reads the continuous
numbered volumes from that folder. The installer does not read `parts.json`.

A package records its source as provenance: one exact nonzero lowercase
40-character commit SHA, read without trimming or normalizing. The
[Windows installer](https://github.com/mydanyi/armadaos-freedom-installer)
validates that provenance and selects the matching model before writing.

The installer creates the tablet-specific 2 GiB `ARMADA_BACKUP` image from
verified local recovery material. It is not part of the shared Linux payload.
Version 0.7.4 creates recovery images with the v3 settings area in the final
64 KiB; support for reading or saving settings remains model-specific. See the
[recovery image contract](https://github.com/mydanyi/armadaos-freedom-installer/blob/main/docs/package-contract.md).

The legacy SD card image and its manual installation flow remain available.

## Tests

```console
$ just test-edl-package
```

This runs the EDL filesystem tests and needs Docker with privileged access to
loop devices and btrfs. It is kept separate from `just test`.
