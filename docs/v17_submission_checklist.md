# V17工程提交说明

当前版本为V17/0.11.0，沿用narrow09硬件；上板验证通过按队伍确认记录。下表只列当前工程与测量材料，原版本的归档记录保留原日期。

| 内容 | 当前入口 | 范围 |
|---|---|---|
| 平台简介与多调制能力 | [首页](../README.md)、[架构](platform_overview.md) | C++ Qt本机控制、Linux驱动、可编程I/Q矢量产生与接收分析 |
| 生产源码 | [src](../src)、[版本清单](../board/releases/v17.json) | V17应用、narrow09 RTL/约束/系数、设备驱动与PetaLinux配置 |
| 构建与复现 | [构建入口](../build/README.md)、[完整复现](../build/reproduce.md) | Vivado恢复、SDK构建、SD运行包与start_v17.sh生成 |
| 测试 | [验证入口](../sim/README.md) | 本机7项非Qt CTest及三频点完整I/Q检查；历史仿真按原记录日期 |
| 硬件资源与时序 | [原始实现报告](../build/reports/fpga) | V17沿用narrow09逻辑，半带模块仅修正一条过时注释 |
| 板端流程 | [上板步骤](../board/bringup.md)、[版本清单](../board/releases/v17.json) | 默认停播、上传回读、显式播放/停止及接收操作 |
| 多调制测量材料 | [图册](waveform_gallery.md)、[照片与代码追溯](constellation_code_trace.md) | 64QAM、脉冲、LFM为相应测试阶段实测，不改写原始数值 |

V17源码包不含SDK、工具许可、BIT/XSA、ELF/KO或完整Linux镜像；按构建入口生成。正式报告与视频应按实际测量对象引用指标：多调制矢量信号源是主线，QAM星座是其中一项发射链路验证，不把4.35%扩展成全频段整机EVM保证。
