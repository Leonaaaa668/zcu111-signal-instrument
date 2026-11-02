# 真实沟通节选 / c06_coverage

节选保留原问答内容，去掉原始XML重复副本、替换私有IP/用户目录；不是重写对话。历史陈述仅代表当时状态。

## 记录0552 · 用户 · 2026-10-28 19:57:13
来源：zcu111_chat_history_20260923_20261029.md；原记录正文SHA256：a902632862bb75df32a1a69fd9b999c1d72dd72aae7d0f7f8850716a2ec3e882

150Msps的话DMA带宽能够吗？是不是只能按帧来取了？这是否符合pi能谱仪的工作要求？

---

## 记录0995 · 用户 · 2026-10-31 11:34:44
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：5e6ff10261d70f7933eed6712ed9b302f47a31ef5838d814d3ad6400a9665761

**确认问题：** 保持信号源1.300 GHz、−40 dBm、RF ON不变。把板端Span改为1000 MHz（RX center会自动回到1800 MHz），FFT选8192、Average1，点Run Single并等扫描完成。请确认横轴是否为1300–2300 MHz，以及左边缘约1300 MHz处能否看到信号峰；扫描模式主要看Marker位置，Tone Estimate会显示Sequential sweep。

**用户回答：** 横轴是1300–2300 MHz，左边缘约1300 MHz能看到信号峰；扫描模式是什么，怎么开？按Marker按钮能看到1300MHz处出现蓝色虚线，Tone Estimate显示Sequential sweep。

**确认问题：** 完成上一项1.3 GHz扫描核对后，再验收上边缘：先RF OFF，把SMB100A改为2.300 GHz、−40.00 dBm、Level Offset 0，再RF ON；板端保持Span1000 MHz、中心1800 MHz、FFT8192、Average1，点Run Single。请报告右边缘约2300 MHz处是否有信号峰。完成后点RX STOP并将SMB切回RF OFF，也请确认关闭状态。

**用户回答：** 右边缘约2300 MHz处是否有信号峰 ；RX STOP按钮是灰色的，已将SMB切回RF OFF

## 记录0996 · 助手（进度沟通） · 2026-10-31 11:35:01
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：c6d6e4ce1343fe725ecccb24e770552afe553fe2e056518f5717ff023d15a4a8

你已经进入扫描模式了：Span大于150 MHz后会自动分段扫描，标题里的“SWEPT”和“Sequential sweep”就是标识，不需要另外开启。Marker只是频率光标，蓝色虚线不负责启动扫描。

Run Single完成一轮后自动停止，所以RX STOP变灰是正常的；Run Cont.才会连续扫，此时RX STOP可以点击。RF OFF已收到，我现在从保存的数据核对2.3 GHz峰值，你暂时不用再开RF。

---

## 记录0999 · 助手（回答） · 2026-10-31 11:48:49
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：ed348b7bb2bd64c685cbdcae76ca9717fd6f4198f4ad5521e88fb44fd13e85a3

**版本11已完成并在板上运行，1.3 GHz、2.3 GHz两端实测通过。** FPGA/XSA未改，版本10保留用于回退。

现在默认显示1 GHz扫描视图：

- **Run Cont.**：连续扫描；点 **RX STOP** 停止。
- **Run Single**：只扫一轮，完成后停止键变灰。
- **Marker**：频率光标，不负责启动扫描。

FFT8192、平均1帧时，后台约 **0.15秒/轮**，界面约 **4轮/秒**。采用13段分时采集，全频段功率标定尚未完成，曲线保持dBFS。

[操作说明]([user-home]/Documents/Codex/2026-09-23/ni/work/zcu111_adc3_2400/linux/apps/instrument_cpp/TUNING_SWEEP_20261031_11.md) · [完整源码与运行包]([user-home]/Documents/Codex/2026-09-23/ni/work/zcu111_adc3_2400/delivery/zcu111_instrument_cpp_20261031_11.zip)

运行包也已存入SD卡。当前RF已关闭，可以直接点Run Cont.观察噪声底。

---
