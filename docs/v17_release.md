# V17 / 0.11.0 发射频率推广版

当前生产源码入口为V17，沿用narrow09硬件。用户于2026-11-03确认队友已完成上板验证、结果通过；[版本清单](../board/releases/v17.json)保留验证来源。本次提供当前源码、构建输入与复现入口。

## 功能改进

- CW dBm/dBW设置支持1300–2300 MHz；Digital peak模式原本已有此范围。
- 1790 MHz使用原参考模型；其他频点显式采用该模型估算，Qt和波形元数据标记ESTIMATED。该推广不等于全带频响校正。
- 保留功率范围：4025G/当前线缆−40至−20 dBm，原FSVA/线缆−50至−10 dBm。其他调制的平均功率不能直接套CW标定。
- RF/IQ频率步进100 Hz，与10 ms记录相干；参数改变撤销旧上传有效性，上传与回读通过后显式RF PLAY。
- 板端继续使用C++17 Qt和Linux驱动，RFDC、ADC/DAC 2.4 GS/s、PL DDR4、DMA、FIR/FFT、驱动接口不变，无须为此软件策略重新综合FPGA。

## 矢量信号源

统一I/Q接口可表达QAM、PSK、LFM、FSK、脉冲、相位编码等波形。内置预设包括CW、脉冲、LFM/脉冲LFM、Barker13、2/4/8PSK、16/64/256QAM；FSK等可定义并导入兼容I/Q，附历史CPFSK2生成参考。任意波形仍受采样率、量化、长度、通带和峰值约束。

平台先完成中频矢量产生与分析核心，接入后级本振、混频、滤波及幅度控制模块后，可扩展为覆盖目标射频频段的完整矢量信号源；实际覆盖和整机指标由模块与标定决定。见[架构](platform_overview.md)和[带宽说明](bandplan_and_bandwidth.md)。

## 版本与证据

本机非Qt C++构建和7项CTest通过；1300/1800/2300 MHz三份48 MB/10 ms生成波形的数字频率、SHA256和循环边界检查通过。[测试索引](../sim/README.md)、[交付回执](validation/v17_source_delivery.json)。本机未重复执行另一台机器的SDK构建，上板通过按队伍确认记录。

历史照片保留各自代码阶段，不更改日期和读数。[星座来源追溯](constellation_code_trace.md)已定位2026-10-25的TX v2固件、原生成器及BIT/ELF身份。旧V16记录、固定启动脚本和源码身份保留用于历史核查与回退。

## 源码重建与运行包

1. 按[完整复现](../build/reproduce.md)在匹配PetaLinux2025.2 SDK中执行build/linux/build_all.sh，使用新输出目录。
2. 构建入口执行12项CTest，另可按[Qt验证入口](../sim/v17_validation.md)运行offscreen检查。
3. 新运行目录为zcu111_instrument_qt5_20261103_17，内含release.json。用build/make_sd_runtime.py及实际BOOT.BIN生成归档和start_v17.sh，哈希绑定本轮文件。
4. 按[上板流程](../board/bringup.md)检查界面、接收、上传回读及显式播放/停止；更换测量链路后沿用相应参考面和幅度范围。

原V16回退入口为已保存的V16运行包/start_v16.sh或提交a63381887d01d801a63d21a0250cc45b9c4dc60f。Vivado工程名zcu111_v16仍对应沿用的narrow09硬件，不表示应用未升级。

本次仅交付源码、构建入口、技术总结与测试附件，不附全项目文件校验清单。

一致性整理另修正了一条RX半带模块的过时接线注释，RTL逻辑、时序约束与硬件基线保持一致；不是新增FPGA功能或重新布局布线。
