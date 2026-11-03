# 平台能力、技术栈与外部测量

本组说明补充仓库首页，更新于2026-11-03；不改变V16冻结硬件、驱动和Qt生产源码。

- [平台架构与实现对应](platform_overview.md)：Linux、Weston、C++ Qt与FPGA如何组成独立仪器。
- [频率规划、带宽与测量范围](bandplan_and_bandwidth.md)：1.3–2.3 GHz的由来，以及1 GHz工作带、150 MS/s、扫描窗口的区别。
- [多调制波形与实测图册](waveform_gallery.md)：64QAM、脉冲、LFM的照片、数值结果与适用范围。
- [离线频率规划核对结果](../data/bandplan/offline_check.json)：源码参数、FIR系数、13段扫描与接口速率的交叉核对。

推荐展示主线：独立Linux仪器 → 可编程矢量波形 → 外部64QAM/脉冲/LFM测量 → 接收分析与1 GHz分段覆盖 → 后级变频扩展。
