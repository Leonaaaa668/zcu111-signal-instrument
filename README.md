# 信号处理及产生综合平台 · ZCU111 V16

一套独立运行在 AMD RFSoC ZCU111 上的可编程矢量信号源与频谱分析系统，集信号产生、采集、频谱分析与交互显示于一体。平台把高速 ADC/DAC、FPGA 并行信号处理、ARM Linux 控制与原生 C++ Qt 仪器界面集成在同一块板卡上，面向信号处理教学、通信波形验证和射频实验。连接 DP 显示器与 USB 鼠标即可操作，无需上位机参与日常采集、波形生成和播放控制。

本仓库对应 **2026-11-01 冻结的 V16 / 应用 0.10.2 / narrow09 硬件基线**，材料整理于 2026-11-02，矢量信号源与频率范围说明更新于 2026-11-03；本次文档更新未改变冻结的 V16 生产代码。仓库提供源码、配置、构建脚本和可核查的测试证据，Vivado 工程与运行文件按步骤重新生成。

## 设计亮点

- **从高速数据到仪器操作的完整链路。** 单路 ADC/DAC 均配置为 2.4 GS/s；PL 完成抽取、Hann 加窗、FFT 与功率运算，再将结果送给 ARM。界面直接支持调谐、RBW选择、视频平均、扫描时间、峰值搜索和 Marker 比较。
- **软硬件各取所长。** 高频率、重复性强的运算放在 FPGA，波形生成、数据管理、扫描拼接、标定模型和交互放在 ARM。接收复采样率为 150 MS/s，65536 点频谱包仅 524544 字节，降低 CPU 数据搬运与显示负担。
- **频段覆盖与细节分析兼顾。** NCO 调谐和重叠分段拼接实现 1.3–2.3 GHz 的 1 GHz 扫描视图；局部分析支持 1024、8192、65536 点 FFT。65536 点时频率格间隔约 2.289 kHz、Hann 等效噪声带宽约 3.433 kHz。
- **可编程 I/Q 矢量波形和可验证的播放链路。** ARM 本机生成 CW、脉冲、LFM、脉冲 LFM、Barker13、PSK、QAM；10 ms 波形以 48 MB I/Q 文件上传到 PL DDR4，回读及 SHA256 验证后播放，配套停止、静音和欠载保护。播放链路以兼容格式的 I/Q 文件为输入，可用自定义 I/Q 描述 FSK、AM/FM 等其他波形；FSK 当前没有内置界面预设，扩展波形仍需数字及射频验证。
- **从一次实现走向可复用工程。** 功率模型保留参考面、原始读数与独立检查点；构建、启动配套接口契约和哈希回执；附带可独立使用的 FPGA 证据工作流技能包。

## 能力与实测摘要

| 项目 | V16 能力 / 已有证据 |
|---|---|
| 独立操作 | C++17 / Qt5 Widgets，SA3000布局，Spectrum / Generator，DP1080p、鼠标操作人工确认 |
| 接收处理 | ADC2.4 GS/s → RFDC抽取2 → PL抽取8 → 150 MS/s复数；Hann / FFT / 功率谱 / DMA |
| 连续频谱 | 持久打开RX设备、后台取帧、最新帧显示；N65536、平均1，V16记录82次显示/8.416 s，约9.74次/s |
| 频段扫描 | 1.3–2.3 GHz，13段NCO顺序扫描；N8192、平均1的两次历史端点测试每轮约2.65 s |
| Marker | 主页面M1–M4；峰值搜索、跟踪、点击定位、参考Marker及频率/幅度差值 |
| 物理量操作 | FFT等效RBW、线性功率平均与等效VBW、最小扫描停留时间；CW dBm/dBW模型 |
| 波形播放 | 本机产生10 ms波形，48 MB上传/回读/SHA256，显式RF PLAY / RF STOP；CW已有仪器读数，当前C++16QAM有实测频谱照片 |
| 资源与时序 | LUT72273 / Registers113196 / BRAM Tile602.5 / DSP183；WNS+0.140 ns、WHS+0.010 ns、WPWS+0.014 ns |

1 GHz 指顺序扫描范围，150 MS/s 是处理复采样率。连续模式用于连续刷新频谱；当前未提供全带宽无间断原始 I/Q 录像。**1790 MHz 是 CW 功率模型的标定频点，不是 CW 唯一可发射频点。** V16 Generator 在 Digital peak 数字幅度模式下可设 RF 1300–2300 MHz；CW model 的 dBm/dBW 适用条件另行限制，不能由此推断硬件只支持一个频点。其他调制使用数字幅度模式，接收全频段谱线使用 dBFS/bin。频率端点及全带幅度平坦度尚需按发射条件复核。详细指标见 [硬件与性能](report/hardware_performance.md) 和 [实测比对](board/measurements.md)。

更多波形、历史星座证据、V16 与候选版本的区别，以及后级变频扩展方案，见 [矢量信号源能力说明](report/vector_signal_source.md)。

## 系统结构

```mermaid
flowchart LR
    A[XM500 J1] --> B[ADC 2.4 GS/s]
    B --> C[RFDC DDC / 抽取2]
    C --> D[PL 抽取8 / Hann / FFT / 功率]
    D --> E[快照 / DMA / Linux RX]
    E --> F[ARM C++ Qt 仪器]
    F --> G[本机波形产生 / 上传与回读]
    G --> H[PL DDR4 / DMA / 播放保护]
    H --> I[RFDC 插值2 / DUC]
    I --> J[DAC 2.4 GS/s / XM500 J7]
```

后续可在 J7 后连接本振、混频与滤波等变频模块，扩展可调射频中心频率范围，形成更广频段的信号源。该方案尚未集成验收；实际覆盖范围由模块决定，变频不会自动增加瞬时调制带宽。

## 推荐目录与入口

| 目录 | 内容 | 阅读入口 |
|---|---|---|
| `src/` | RTL、BD Tcl、约束/系数/时钟；Qt、驱动、RFDC辅助程序；PetaLinux/SDT | [源码完整性](report/source_completeness.md) |
| `sim/` | Verilog测试平台、黄金参考、C++测试说明和原始结果 | [验证索引](sim/verification_catalog.md) |
| `build/` | 工程恢复、综合实现、Linux/SDK、SD包构建脚本；原始综合实现报告 | [完整复现](build/reproduce.md)、[构建入口](build/README.md) |
| `board/` | 启动/运行脚本、冻结回执、实板结果、验收流程 | [上板步骤](board/bringup.md)、[实测比对](board/measurements.md) |
| `data/` | 原始频谱包、参考读数、标定模型、比对表及测量照片 | [数据索引](data/README.md) |
| `skill/` | 工作流、真实故障案例、提示词、模板和校验工具 | [技能包](skill/README.md) |
| `report/` | 要求对照、源码审计、架构/性能、优化过程、材料清单及验证回执 | [提交清单](report/submission_checklist.md) |

## 从零复现

以下命令均在仓库根目录执行。Windows可将python3替换为python。建议短英文路径，例如C:/fpga/zcu111_v16。

### 1. 工具与完整性

- Vivado **2025.2**、RFSoC/ZCU111器件和板卡支持、有效器件综合实现许可。
- Linux编译机：PetaLinux **2025.2**及匹配SDK；已验收环境Qt5.15.13、GCC13.4、Linux6.12.40-xilinx。
- 开发电脑Python3.10+；数值参考依赖见sim/python/requirements.txt。板端不依赖Python/PYNQ。

```sh
python3 build/verify_manifest.py
python3 build/check_submission.py
python3 sim/replay_evidence.py
```

### 2. 恢复并构建 FPGA

```sh
vivado -mode batch -source build/fpga/create_project.tcl -tclargs C:/fpga/v16_hw_new
```

在Vivado打开新目录下的zcu111_v16.xpr查看完整BD。上述命令恢复并检查工程，完整综合/实现/BIT/XSA另执行：

```sh
vivado -mode batch -source build/fpga/build_bitstream.tcl -tclargs C:/fpga/v16_hw_new/zcu111_v16.xpr C:/fpga/v16_exports
```

Linux使用绝对Linux输出路径。选择新目录，采用新BIT前核对该轮时序和布线结果。

### 3. 跑仿真与参考测试

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\sim\run_rtl_tests.ps1 -VivadoBin 'D:/Vivado/2025.2/Vivado/bin' -OutputDir 'C:/fpga/v16_rtl_new'
```

```sh
python3 -m pip install -r sim/python/requirements.txt
python3 -m unittest discover -s sim/python -p 'test_*.py'
```

11项CTest在下一步build_all.sh中执行。测试对象、预期及原始日志见 [验证索引](sim/verification_catalog.md)。

### 4. 构建 PetaLinux / SDK / 板端软件

加载PetaLinux2025.2环境后执行：

```sh
bash build/linux/prepare_petalinux.sh /absolute/new_petalinux_project
```

在新工程中设置自己的网络、用户凭据，构建镜像与SDK。使用同轮准备好的内核目录：

```sh
export SDK_ENV=/absolute/sdk/environment-setup-cortexa72-cortexa53-amd-linux
export PLNX_PROJECT=/absolute/new_petalinux_project
export KERNEL_BUILD=/absolute/prepared_kernel_build_directory
bash build/linux/build_all.sh /absolute/v16_linux_output
```

KERNEL_BUILD须包含目标内核.config和Module.symvers。PetaLinux镜像/SDK构建、SDT导入、BOOT打包的完整命令在 [build/reproduce.md](build/reproduce.md) 第4–6节，按顺序执行。

### 5. 生成 SD 文件并启动

```sh
python3 build/make_sd_runtime.py --runtime /absolute/v16_linux_output/zcu111_instrument_qt5_20261101_16 --boot-bin /absolute/new_petalinux_project/images/linux/BOOT.BIN --output /absolute/v16_sd_files
```

将新运行包/start_v16.sh与同轮BOOT.BIN、image.ub、boot.scr放到SD第一FAT分区。板卡启动、root登录后：

```sh
sh /run/media/mmcblk0p1/start_v16.sh
```

先验证显示、鼠标与单次接收，再按 [上板验收](board/bringup.md) 检查刷新、Marker、上传回读与显式播放/停止。board/reference/start_v16.sh的固定哈希只适配原冻结包；重编译后使用上述生成器。

## 验证与扩展方向

已有全新Vivado恢复、SDK软件重编译、6项Verilog仿真、11项CTest、20项参考测试与实板连续频谱证据。后续包括全频段幅度标定、一般调制平均功率与RF EVM、1800 MHz小杂散定位和更完整的原始I/Q采集，见 [版本范围](report/status_and_scope.md)。此次整理没有改变冻结生产代码；新BIT/新镜像仍需完成对应构建与板测。

## 路径与许可

主目录使用小写英文。README.md、CMakeLists.txt、Makefile、SKILL.md与第三方原生文件名保留约定大小写，详见 [路径约定](report/path_conventions.md)。第一方应用MIT、内核模块按GPL-2.0-only声明，第三方资源按随附许可使用，见 [LICENSE](LICENSE) 和 [第三方来源](THIRD_PARTY_NOTICES.md)。
