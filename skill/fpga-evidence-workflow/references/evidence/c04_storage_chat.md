# 真实沟通节选 / c04_storage

节选保留原问答内容，去掉原始XML重复副本、替换私有IP/用户目录；不是重写对话。历史陈述仅代表当时状态。

## 记录0838 · 用户 · 2026-10-30 11:20:13
来源：zcu111_chat_history_increment_20261029_20261030.md；原记录正文SHA256：9f4b6762226a7232395ced622814969e9c77ef2ba9740c6a28c0ef37c1e22732

**确认问题：** 新版 Qt 已在板上启动，当前停播。请点 `GENERATOR`，保持默认 CW / RF 1790 MHz / Digital peak 2048，点 `GENERATE ON PS / SAVE IQ FILE` 并保存默认文件名；生成完成后点 `UPLOAD TO DDR4`，选刚生成的文件。请告诉我鼠标是否正常，以及最后是否显示回读/SHA256 通过、`NOT playing`。先不点 RF PLAY。

**用户回答：** 已GENERATE ON PS / SAVE IQ FILE 并保存默认文件名  ，但是完成后点 UPLOAD TO DDR4  并没有找到刚才生成的文件。鼠标正常

## 记录0841 · 用户 · 2026-10-30 11:23:35
来源：zcu111_chat_history_increment_20261029_20261030.md；原记录正文SHA256：680549d4c5c4c1d7ef8bface079de3902415c705d8cbb8fc357b990cd3fe0f77

**确认问题：** 我检查到板上还没有生成新的波形文件。请点 `SYSTEM / LOG`，把最后一条生成相关提示或 ERROR 的文字发来。与此同时，我会把“上传”改为直接使用本次成功生成的文件，减少重复选文件。

**用户回答：** Local receiver: unavailable
Linux TX interface:/dev/zcul11_tx
Linux TX board check: 10ms upload/readback and 1s play/stop passed
RF frequency, modulation quality and output power still need receiver/instrument measurement
RFDC/clock readiness: must be initialized by the board service
Center: 1.8GHz/ z0om0/ ADC 2.4GS/s/ RX 150MS/s
Storage: /home/petalinux/zcul11_measurements
No Python, PYNQ. JTAG control, or host PC is required by this application.
The GUl never maps /dev/mem or uses bare-metal PS DDR addresses.
19:17:3619:20:44
19:16:53 Generating 10ms waveform on PS; no RF output action.
19:16:53 Insufficient storage for a 10ms IQ record
19:17:36 Generating 10ms waveform on PS; no RF output action.
Insufficient storage for a 10ms IQ record
Generating 1oms waveform on PS; no RF output action.
19:20:44 Insufficient storage for a 10ms 1Q record这是system/log里的全部内容

## 记录0845 · 用户 · 2026-10-30 11:30:56
来源：zcu111_chat_history_increment_20261029_20261030.md；原记录正文SHA256：4dde1612b7bc9c454752b9c02bda769c21d62d1d28ed1e44b60c12b0d71a8440

**确认问题：** 请在新版 `GENERATOR` 页保持默认 CW / 1790 MHz / 2048，点“生成”，等按钮下方出现 `Generated: /tmp/...bin`，再点 `UPLOAD TO DDR4`。现在是否显示上传校验通过且 `NOT playing`？先不点 RF PLAY。

**用户回答：** 是的，有显示No validated acquisition yetTX DDR round-trip and SHA256 passed; loaded, NOT playing.
