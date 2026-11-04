# V17源码完整性与V16历史基线

> 当前生产源码为V17/0.11.0，沿用narrow09硬件；上板验证通过按队伍确认记录。V16原始日志及固定启动脚本保留为历史基线，新构建使用生成的start_v17.sh。见[V17说明](../docs/v17_release.md)。

V16自写生产源码、配置与工程恢复输入已完整收录。原V16归档曾核对88个生产文件；V17包含6个应用文件改动、新测试及1条RTL注释修正，216个其余源码文件保持一致。source_identity.json保留原V16技术基线；V17当前版本见board/releases/v17.json，结构及基线差异由build/check_submission.py检查，不附全文件清单。

| 层级 | 已有内容 | 构建入口 |
|---|---|---|
| PL | 18个自写.v/.vh；完整BD Tcl；DDR/CDC约束；滤波与Hann系数、时钟表 | build/fpga/create_project.tcl |
| Linux内核 | TX/RX驱动、UAPI、Makefile、运行时overlay | build/linux/build_all.sh |
| 控制与启动 | RFDC初始化、NCO调谐、时钟/复位顺序、权限、Weston配置 | src/linux/*/admin、board/runtime_layout |
| 仪器应用 | C++17 Qt、频谱测量、波形生成、物理量模型、Marker/VBW/扫描、事务和测试 | src/linux/apps/instrument_cpp/CMakeLists.txt |
| 操作系统 | 实际PetaLinux2025.2配置/meta-user、内核参考.config、冻结SDT和PS初始化 | build/linux/prepare_petalinux.sh |
| 验证 | Verilog平台、黄金参考、C++测试、实测样本与原始报告 | sim/verification_catalog.md |

完整工程以Tcl恢复，不依赖作者机器的已有XPR缓存。新目录恢复XPR/BD/wrapper已验证。AMD商业工具/IP、编译器、Linux/Qt依赖通过已列版本与PetaLinux recipe获取，自写源代码不依赖未提交的私人库。

## V16历史基线的已验证复现层级

1. 冻结文件身份一致：88文件检查与18个RTL/头文件硬件冻结哈希。
2. 新目录Vivado2025.2恢复和接口/地址契约通过。
3. V16在SDK新输出目录重编译Qt、CLI、两个模块、辅助程序及overlay，11项CTest通过。V17当前Linux＋Qt配置为12项；本机非Qt路径7项通过，详见sim/README.md。
4. 新PetaLinux工程导入实际SDT/config/meta-user通过。
5. 原硬件routed报告、V16板端界面/连续接收与既有收发实测随包保存。

完整新Linux镜像构建、新布局布线BIT与新镜像冷启动/RF验收应按步骤再执行；这与“自写源码是否收齐”分别记录。不同主机布线不保证BIT字节和时序值相同。

未收录巨大工程缓存、SDK、DCP/BIT/XSA、ELF/KO、BOOT/image和48MB波形；都可由构建或波形生成入口产生。未收录许可证、私钥及共享凭据。早期裸机参考用于接口对照，不在当前Linux下执行固定RAM/JTAG控制。
