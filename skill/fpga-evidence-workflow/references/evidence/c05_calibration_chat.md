# 真实沟通节选 / c05_calibration

节选保留原问答内容，去掉原始XML重复副本、替换私有IP/用户目录；不是重写对话。历史陈述仅代表当时状态。

## 记录0954 · 用户 · 2026-10-30 21:38:12
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：3955a03a7c5a117f3d398033e7331ccdaeb36ea98c3921b947a140ef80dab256

**确认问题：** 版本10已在板上打开，现在做最后一次实时界面验收：① SMB保持1.790 GHz、Level Offset 0，设−45.00 dBm并RF ON；② 在Spectrum选Linux RX device、FFT65536，将Average设为16，点Run Single。请告诉我下方“CW INPUT (J1)”显示多少dBm，以及鼠标是否正常。预计约−45.77 dBm；这次不用再接4025G。

**用户回答：** 已SMB保持1.790 GHz、Level Offset 0，设−45.00 dBm并RF ON，“CW INPUT (J1)”显示  -45.735dBm，鼠标正常

## 记录0955 · 助手（进度沟通） · 2026-10-30 21:38:23
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：0be7c86776772313dfa39049e36df7ba9025ded654a3f6c73577ad8c8c0a861a

实时界面验收通过：显示 −45.735 dBm，相对这档 4025G 参考值 −45.707 dBm，误差约 −0.028 dB；鼠标也正常。我会把这次结果补进标定记录。

---

## 记录1026 · 用户 · 2026-10-31 14:45:16
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：a9c2550860ae82a07285c5a4dc61a97af70a58e66ec9609639c766b6170ef82d

**确认问题：** 下一档测−30 dBm：保持接线、4025G设置、CW1790 MHz及dBm单位不变。在Generator将Target level改为−30.00，重新GENERATE IQ→UPLOAD TO DDR4，校验通过后RF PLAY。找主峰，稳定后间隔约2秒记录三次Marker功率，再点RF STOP。请回复主峰频率、三次dBm读数及已停播确认；若出现报错则停在该步并发提示。

**用户回答：** 主峰频率1.789999950GHz，三次dBm读数依次为-30.11/-30.11/-30.12，已停播

## 记录1038 · 用户 · 2026-10-31 14:57:29
来源：zcu111_chat_history_increment_20261030_20261101.md；原记录正文SHA256：524ef79a7cf1ca1ffbb93de7d2f57f7c5e961a0cb0ac806345199586749a7fe0

**确认问题：** 原表四点复核已完成；现在补测候选修正后的实际效果。目标是输出−25.00 dBm，现有软件尚未加补偿，所以请暂把Target level设为−24.94 dBm（多给0.06 dB），其余仍为CW1790 MHz及相同4025G设置。重新GENERATE IQ→UPLOAD TO DDR4→RF PLAY，稳定后间隔约2秒读三次Marker功率，再RF STOP。请回复主峰频率、三次dBm读数及停播确认；该−25dBm点未参与补偿值估计。

**用户回答：** 主峰频率1.789999950GHz，三次dBm读数依次为-25.01/-25.01/-25.00.已确认停播
