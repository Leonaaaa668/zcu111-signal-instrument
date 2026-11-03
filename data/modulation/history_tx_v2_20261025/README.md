# 星座图对应的TX v2来源

原始版本为2026-10-25的Integrated TX v2，64 MiB缓冲、10 ms长波形、A53裸机与XSCT/JTAG控制。
本目录保存原Vitis启动源码、归档中的Python波形生成器和命令脚本、QAM16/QAM64/CPFSK2原始元数据，以及哈希追溯结果。

详见[代码与照片对应说明](../../../docs/constellation_code_trace.md)和[trace.json](trace.json)。
vector_tx.c与三个自写TX链路相关RTL已经在源码目录中，核对与历史备份一致，故不重复复制。
这些文件用于历史来源核查；当前板端继续使用Linux/C++ Qt，不在Linux下执行裸机/JTAG脚本。

开发电脑离线生成参考可显式指定独立输出目录：`python3 tx_waveforms.py generate --output /absolute/new_historical_waveforms`。这会生成12份48 MB波形及参考数据，需要足够磁盘空间；不访问硬件。capture/selftest模式依赖历史ILA输入，不作为本目录的独立验收入口。
