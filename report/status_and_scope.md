# V17源码交付状态

当前仓库入口：V17 / 应用0.11.0 / narrow09硬件，2026-11-03。用户确认队友已完成V17上板验证并通过，本次提供当前源码；本机未重复执行另一台机器的SDK构建。详见[版本说明](../docs/v17_release.md)与[交付清单](../board/releases/v17.json)。

V17推广CW dBm/dBW到1300–2300 MHz，非1790 MHz明确为ESTIMATED，原功率范围与保护保留；数字I/Q矢量链路继续支持内置多调制和兼容任意I/Q扩展。QAM/PSK/LFM/脉冲/相位编码已有生成实现；FSK等通过定义兼容I/Q扩展，当前无内置FSK预设。后级变频可扩展射频工作频段，尚未集成验收。

FPGA/RFDC/DDR4/DMA、FIR/FFT、内核驱动与接口未改变。build/reports/fpga、原V16 SDK/板测日志和历史RF照片保持日期与原始数值，不改成V17结果。已有窄带150 MS/s复采样、NCO分段覆盖1.3–2.3 GHz、原生C++ Qt、Marker/Delta、FFT等效RBW/VBW和多波形播放为沿用基线。

仍待完善：全频段幅度标定、一般调制平均功率与逐型RF EVM/BER、1800 MHz杂散定位、无遗漏宽带瞬态采集、原始I/Q完整采集。1 GHz工作带、约960 MHz规格推导TX通带和RX顺序扫描应按[带宽说明](../docs/bandplan_and_bandwidth.md)分别表述。

本次不构建新FPGA BIT或完整Linux镜像，不访问板卡；编译、数据复算和RF实测分开记录。正式设计报告、视频、作者分工和最终提交材料由队伍按实际版本补齐。
