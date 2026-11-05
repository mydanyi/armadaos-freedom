# TB710FU 固件包

小新 Pad Pro GT（TB710FU）专用的原厂固件，做法与 [TB323FU 的固件包](../../lenovo-tb323fu/firmware/audio) 相同：文件随仓库提供，构建时先按 `SHA256SUMS` 校验，再装进镜像。所有文件都放在只有本机设备树会引用的路径下，所以直接装到 `/usr/lib/firmware/`，运行时不需要再复制或切换固件搜索路径。

| 文件 | 用途 | 来源 |
|---|---|---|
| `qcom/sm8650/xiaoxin/gt/adsp.mbn`、`adsp_dtb.mbn` | ADSP：电池、USB-C 角色、声卡 | 原厂 ZUXOS 1.5.04.495 `/vendor/firmware_mnt/image` 的 `adsp.mdt`+`adsp.bNN`、`adsp_dtb.mdt`+`adsp_dtb.bNN` |
| `qcom/sm8650/xiaoxin/gt/cdsp.mbn`、`cdsp_dtb.mbn` | CDSP | 同上，`cdsp*` |
| `qcom/sm8650/xiaoxin/gt/gen70900_zap.mbn` | GPU zap shader（联想签名） | 原厂 `/vendor/firmware` |
| `qcom/sm8650/xiaoxin/gt/aw882xx_acf.bin` | 4 颗 AW882xx 功放的参数（ACF） | 原厂安卓的功放配置（经 [tb710fu-linux](https://github.com/MeteoricmeteorLL/tb710fu-linux) 取得，与本机 `awinic/aw88461_tb710fu.bin` 相同） |
| `novatek/novatek_nt36532_fw.bin`、`novatek_nt36532_mp.bin` | NT36532 触摸固件（开机刷写）与量产测试数据 | 原厂 `/vendor/firmware` |

`.mbn` 是把原厂分段文件合并成的单个文件：`tools/squash-mdt.py` 做的事与 linux-msm 的 [pil-squasher](https://github.com/linux-msm/pil-squasher) 相同（`.mdt` 放在开头，每个有数据的程序段按 `p_offset` 写入对应的 `.bNN`），每个段都与原文件逐字节核对过。

```sh
python3 tools/squash-mdt.py /path/to/adsp.mdt qcom/sm8650/xiaoxin/gt/adsp.mbn
```

不在这里的：
- GPU 的 `qcom/gen70900_sqe.fw`、`qcom/gen70900_aqe.fw`、`qcom/gmu_gen70900.bin`：镜像已自带（linux-firmware）。
- WiFi `board-2.bin`、扬声器 topology：已在 `system_files/usr/lib/firmware/` 下。
- 视频解码的 `qcom/sm8650/xiaoxin/gt/vpu33_4v.mbn`：设备树有引用，但视频编解码（iris）没有启用，所以不提供。
