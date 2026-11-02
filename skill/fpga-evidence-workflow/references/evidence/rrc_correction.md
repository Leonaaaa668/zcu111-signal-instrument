# C++周期RRC成形的修正与回归

本轮对板上C++生成的数字IQ文件做接收端匹配滤波验证时，发现PSK输出的数字文件误差约53.2%。此前仅检查长度、幅度和文件存在不足以发现该问题。问题文件没有用于DAC播放。

根因是发送端同时使用 `symbols[pos-k]` 与 `h(phase-k)`，符号延迟方向和滤波器相位不一致。

正确关系为：`n/T-(pos-k)=phase/T+k`，因此滤波系数应为 `h(phase/T+k)`。已在 `src/waveform.cpp` 修正，并重新编译ARM64 ELF、在ZCU111重新生成8PSK和64QAM。

修正后开发电脑用独立的接收端RRC匹配滤波对板上生成文件进行数字验证：8PSK约0.12036%、64QAM约0.12064%。这只是数字文件及有限长滤波器的误差，**不是RF仪器实测EVM或整机精度**。

`tests/waveform_tests.cpp` 增加了不依赖Python的原生C++接收端匹配滤波回归，验证生成文件恢复后的PSK圆形点集、QAM网格点集。其判断依据是输出信号的调制性质，而不是只比较文件长度或复制发送端卷积索引。

相关数据见 `arm_waveform_numeric.json`，最终板端C++回归结果见 `arm_native_waveform_contract.json`。
