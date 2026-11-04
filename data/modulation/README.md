# 调制波形与测量材料的运行位置

当前V17在ZCU111的ARM处理器上运行原生C++/Qt，内置CW、脉冲、LFM、相位编码、PSK和QAM波形在板上生成，经DDR4上传校验后播放。日常生成、控制和接收显示不依赖开发电脑、Python或PYNQ。

`history_*_20261025`与`history_tx_v2_20261025`保留早期实验：当时由开发电脑离线生成I/Q，再上传早期裸机/JTAG版本播放。历史Python脚本及照片用于来源追溯，不是当前板端运行入口，也不能改写为V17重新测得的EVM。

`plan.json`是2026-10-31的测量计划和当轮状态记录。其中host Python指计划在开发电脑上分析外部频谱仪导出的I/Q；当轮导出受阻，因此保留未完成状态。该分析方法不代表板端信号生成依赖上位机。

当前应用资源中的历史`tx_catalog.json`保留旧波形目录字段，`python_only`描述旧目录的生成工具；当前原生生成实现位于`src/linux/apps/instrument_cpp/src/waveform.cpp`。这些归档字段不用于定义当前运行环境。

照片、原始参数、历史状态及测量数值均保留；各自版本和验证范围见[实测图册](../../docs/waveform_gallery.md)与[代码追溯](../../docs/constellation_code_trace.md)。
