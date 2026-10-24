# 小新 Pad Pro GT 引导构建与打包

适用于 TB710FU / SM8650。kShimC 菜单与原厂 Android 内核、统一 Linux 内核、DTB 和 initramfs 一起打包进 **boot_a**；HyperOS 照常从菜单启动，B 槽不需要改动。设备功能状态见[设备文档](../README.md)。

## 前提

- 本机 Bootloader 未解锁。A 槽的 vbmeta 链把 `boot` 交给 AOSP `testkey_rsa4096` 校验（ZUI 495 免解锁补充包的启动链），所以用该测试密钥重新签名的 boot 能通过 ABL 校验，并保持 `verifiedbootstate=green`。其它启动链的机器不适用。
- 打包输入全部来自**这台机器**：未修改的 boot_a 备份（原厂 GKI 内核，boot v4）和 vbmeta_a 备份。写入前先在 9008 下备份 boot_a 与 vbmeta_a。
- 构建在 Linux／Docker 中进行；脚本本身不访问设备、不联网。

## 1. 构建 4 MiB runtime

先按[共享依赖准备指引](../../lenovo-tb323fu/boot/kshim-success-source/DEPENDENCIES.md)恢复 CrDK、picolibc、LVGL 与 libfdt。

```sh
# K = devices/lenovo-tb323fu/boot/kshim-success-source
python3 tools/build_runtime.py --config configs/tb710fu_android_defconfig --out <runtime-out>
# 产物 <runtime-out>/kShimC.bin，必须恰好 4 MiB
```

`tb710fu_android_defconfig` 基于四代配置：横屏原生方向（旋转 0）、LVGL 缓冲 256000 字节（20 行 3200 像素）；按键为 PMK8550 pwrkey/resin 与 PM8550 GPIO6（音量+），没有触屏；不启用会话设置；启用 `KSHIM_QCOM_REBOOT`（白名单已包含 `qcom,pineapple`）。

## 2. 准备 Linux 载荷

- **Image：** `KERNEL_OVERLAYS=lenovo-tb710fu` 构建产物里的 `lib/modules/7.2.6-armada-tb710fu/vmlinuz`（原始 arm64 Image，约 65 MB）。
- **DTB：** `lib/modules/7.2.6-armada-tb710fu/dtb/qcom/sm8650-lenovo-tb710fu-kshim.dtb`，打包前加上启动参数、initrd 条目号和本机蓝牙地址：

  ```sh
  fdtput -t s linux.dtb /chosen bootargs "console=tty0 loglevel=7 clk_ignore_unused pd_ignore_unused regulator_ignore_unused rdinit=/init"
  fdtput -t u linux.dtb /chosen kshim,initrd-entry 3
  # 本机地址 aa:bb:cc:dd:ee:ff 按小端写入：
  fdtput -t bx linux.dtb /soc@0/geniqup@8c0000/serial@898000/bluetooth local-bd-address ff ee dd cc bb aa
  ```

- **initramfs：** 负责挂载根文件系统的 newc cpio（gzip）。验证时使用的是基于 busybox 的 initramfs：按 GPT 分区名找到根分区，挂载 ext4 后 `switch_root`。统一内核已内建 UFS、ext4 与 loop，initramfs 不需要加载模块。

## 3. 打包

```sh
python3 pack_boot.py \
  --boot boot_a.img --runtime kShimC.bin \
  --linux Image --dtb linux.dtb --initrd initrd.cpio.gz \
  --packer <DualBootKernelPatcher>/build/MultiBootKernelPatcher \
  --avbtool <aosp>/external/avb/avbtool.py --key <aosp>/external/avb/test/data/testkey_rsa4096.pem \
  --linux-name "SteamOS" --default android --out boot.img
```

[`pack_boot.py`](pack_boot.py) 的布局为 `Image-00` 原厂 Android（基座）、`Image-01` Linux、`Image-02` DTB、`Image-03` initramfs（Blob，64 KiB 对齐）。打包后的内核整体 gzip 压缩（解压后约 109 MB，压缩后约 38 MB），重建 boot.img，再添加 AVB hash footer；salt、rollback index 和属性都与原 footer 相同。DualBootKernelPatcher 与五代使用同一版本和补丁，见[五代引导说明](../../lenovo-tb323fu/boot/star-chart-build/README.md)。

`--default linux` 打出的镜像菜单默认进 Linux。脚本会检查：`--boot` 必须是未改过的 boot_a 备份（已含 kShimC 的镜像会被拒绝）；带 `--initrd` 时 DTB 必须有 `kshim,initrd-entry = <3>`；签名成功后才写出 `--out`。不同 zlib 版本得到的 gzip 字节可能不同，解压后的内容一致。

## 4. 校验与写入

```sh
# boot.img 自身的签名（文件名须为 boot.img：avbtool 按分区名找镜像核对哈希）
python3 avbtool.py verify_image --image boot.img --key testkey_rsa4096.pem
# 本机 vbmeta_a 把 boot 链给哪个公钥：应为 Chain Partition descriptor: boot、
# Rollback Index Location 3，Public key (sha1) 与 testkey_rsa4096 一致
python3 avbtool.py info_image --image vbmeta.img
python3 avbtool.py extract_public_key --key testkey_rsa4096.pem --output testkey.bin && sha1sum testkey.bin
```

（不要对 vbmeta.img 用 `--follow_chain_partitions`：它还会去找 recovery、vbmeta_system 等镜像，并用同一把公钥校验，必然报错。）

写入 boot_a 可以在 9008 下用 firehose，也可以在已 root 的 Android 或 Linux 中用 `dd` 写入分区并读回比对。根文件系统中要安装与 Image 版本一致的 `/usr/lib/modules/7.2.6-armada-tb710fu`。出问题时，在菜单中选原厂 Android，或在 9008 下写回 boot_a 备份。
