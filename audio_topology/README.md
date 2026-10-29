# Audio Topology

Source files for Armada's Qualcomm AudioReach topology blobs.

Only the generated topology blobs under the following directories are copied
into the firmware image; the `.m4` sources remain in this directory.

```text
system_files/usr/lib/firmware/qcom/sm8750/
system_files/usr/lib/firmware/qcom/sm8650/
```

The TB710FU topology is derived from audioreach-topology (BSD-3-Clause,
Linaro). Its notice is `sm8650/LICENSE`, and the same file is installed next
to the blob as `qcom/sm8650/LICENSE.audioreach-topology`.

To regenerate a topology using [audioreach-topology](https://git.codelinaro.org/linaro/qcomlt/audioreach-topology):

```sh
m4 -I <audioreach-topology> sm8750/SM8750-AYN.m4 > SM8750-AYN.conf
alsatplg -c SM8750-AYN.conf -o SM8750-AYN-tplg.bin
```

Run the commands from this directory: `sm8650/Lenovo-TB710FU.m4` also includes
the local `sm8650/` TDM templates (audioreach-topology has none for TDM).

```sh
m4 -I . -I <audioreach-topology> sm8650/Lenovo-TB710FU.m4 > Lenovo-TB710FU.conf
alsatplg -c Lenovo-TB710FU.conf -o Lenovo-TB710FU-tplg.bin
```

The TB710FU blob was generated with audioreach-topology
`1d4c4e337df6bad4d52c4f7b3e15fff3f485be62` and alsa-topology-utils 1.2.16.

Replace the corresponding `-tplg.bin` in `system_files` after regeneration.
