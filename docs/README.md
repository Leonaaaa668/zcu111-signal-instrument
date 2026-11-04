# 平台能力、技术栈与外部测量

本组说明补充仓库首页，更新于2026-11-03；当前生产源码入口为V17/0.11.0，沿用narrow09硬件和驱动；V17上板验证通过由队伍确认。

- [V17源码交付说明](v17_release.md)：版本差异、功率范围、构建与验收状态。
- [星座照片与代码追溯](constellation_code_trace.md)：TX v2原BIT/ELF、Vitis C源码、原波形生成器及时间线。
- [平台架构与实现对应](platform_overview.md)：Linux、Weston、C++ Qt与FPGA如何组成独立仪器。
- [频率规划、带宽与测量范围](bandplan_and_bandwidth.md)：1.3–2.3 GHz的由来，以及1 GHz工作带、150 MS/s、扫描窗口的区别。
- [多调制波形与实测图册](waveform_gallery.md)：64QAM、脉冲、LFM的照片、数值结果与适用范围。
- [离线频率规划核对结果](../data/bandplan/offline_check.json)：源码参数、FIR系数、13段扫描与接口速率的交叉核对。

正式提交材料见[设计报告DOCX](../report/51957_design_report_v17.docx)和[大模型协作记录Markdown](../report/llm_collaboration.md)。精选问答仅在这两份材料中保留，其他文档使用技术总结。

推荐展示主线：独立Linux仪器 → 可编程矢量波形 → 外部64QAM/脉冲/LFM测量 → 接收分析与1 GHz分段覆盖 → 后级变频扩展。
