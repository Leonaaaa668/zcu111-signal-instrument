# 星座照片对应哪套上板代码

已追溯到**2026-10-25 Integrated TX v2：64 MiB缓冲、10 ms长波形的裸机/JTAG版本**。这套代码早于Linux C++ Qt的V16/V17。仓库首页清晰的8×8簇图片是64QAM，屏显EVM 4.35%；不能把它标成16QAM。

## 代码与文件对应

| 环节 | 当时使用的代码 / 文件 | 本仓库核查入口 |
|---|---|---|
| 开发电脑波形生成 | Python/NumPy tx_waveforms.py；12种10 ms波形 | [原生成器](../data/modulation/history_tx_v2_20261025/tx_waveforms.py) |
| 裸机启动与RFDC/时钟初始化 | 原Vitis工程adc3_startup.c，8027字节 | [原启动源码](../data/modulation/history_tx_v2_20261025/adc3_startup.c) |
| A53裸机TX控制 | vector_tx.c；协议2、64 MiB、4 MiB CDMA分块、DDR回读与播放控制 | [vector_tx.c](../src/baremetal_reference/vector_tx.c)、[内存契约](../src/baremetal_reference/tx_memory_map.h) |
| 主机上传/播放命令 | 原tx_command.tcl，检查协议2与地址契约 | [历史命令脚本](../data/modulation/history_tx_v2_20261025/tx_command.tcl) |
| FPGA流通路与保护 | adc_iq_split4.v、dac_stream_guard.v、tx_reset_sync.v | [当前RTL目录](../src/fpga/rtl)；三文件与当时备份逐字节相同 |
| 当时固件 | adc3_capture.bit＋adc3_startup.elf | [哈希、归档及时间线](../data/modulation/history_tx_v2_20261025/trace.json) |

原Vitis工程仍保有这份ELF和C源码，ELF与同日板测快照、17:50运行包的SHA256一致。原Python生成器与命令脚本从该运行包提取；归档清单146个文件全部通过SHA256核对。当前src/baremetal_reference/adc3_startup.c后来增加了RX条件分支，不能当成8027字节的原始启动文件；TX控制C文件本身则字节相同。

## 二进制与波形身份

| 文件 | SHA256 |
|---|---|
| adc3_capture.bit | `f078554663c7d22f9cdf3ed6703cb3cac0c8d5fd905dcffacc949711014824c6` |
| adc3_startup.elf | `95ca3e6aa3b65edb1f9babb3ada5a29146e464353da7211416e3cb6a3facfb97` |
| qam16_5msps_10ms.bin | `0925e71cae4a827600800d79acd36955c16970b21a194485b983bfd5fe319fe5` |
| qam64_5msps_10ms.bin | `9059e860a1021dc51c48b687dda6812a4ca3516f8b8ff33e614e74cb124eca7f` |

16QAM与64QAM均为1.2 GS/s复数、10 ms、48 MB、5 MSym/s、RRC滚降0.35；生成种子分别1709与1710。原始元数据随[历史来源目录](../data/modulation/history_tx_v2_20261025)保留，48 MB二进制不重复放入源码仓库。

## 64QAM照片的时间线

1. 17:26：v2 ELF编译完成；启动串口明确回报协议2、64 MiB和新的源/回读地址。
2. 17:44：保存BIT、ELF、16QAM波形哈希；同日重复板测中48 MB往返校验、16384对I/Q的ILA比对通过。
3. 17:50：归档同一BIT/ELF、命令脚本、Python生成器和波形元数据。
4. 18:32:35：装载qam64_5msps_10ms.bin，DMA往返校验通过。
5. 18:33:47：显式播放命令成功。
6. 18:35:19：CEYEAR 4025K照片，64QAM、中心1.8 GHz、8×8星座簇、EVM 4.35%。

照片与固件基线由同日归档、波形及播放日志关联；拍照瞬间没有另做固件哈希回读。完整对应关系以trace.json为准，不把照片日期当作Qt版本号。

## 这是否影响照片使用

**照片可以用于展示平台的矢量发射能力，版本来源需要标对。** 当时的三个自写流通路RTL仍原样沿用，因此不是另一个无关硬件项目。后来的变化包括Linux驱动、本机C++波形产生、Qt界面及功率设置策略；历史照片不改标为V17新测，也不自动证明每一种当前波形、频率和功率条件下的EVM。

推荐图注：**“ZCU111矢量播放链路历史实测：64QAM，1.8 GHz，5 MSym/s，RRC α=0.35，256符号窗口，RMS EVM 4.35%；2026-10-25 TX v2版本。”**

另找回的原生成器确实包含CPFSK2（1 MSym/s、±2 MHz频偏）及PSK/QAM/LFM/脉冲/相位编码等12种波形。它是开发电脑上的历史参考，不要求板端安装Python；V17原生Qt仍通过C++内置生成或导入兼容I/Q文件操作。

历史JTAG脚本与裸机程序仅供追溯，不在当前Linux运行时直接执行，也不替代当前驱动接口。

## 报告引用建议

正文可写：“在平台发射链路的阶段性验证中，系统播放64QAM复数I/Q波形，经DDR4缓存、DMA传输与RF-DAC输出，由外部矢量分析仪观察到清晰8×8星座。在1.8 GHz、5 MSym/s、RRC滚降0.35及256符号窗口下，仪器显示RMS EVM 4.35%，验证了平台QAM矢量波形播放与模拟输出能力。该测试所用的自写流通路RTL沿用于当前系统。”

图注建议：“平台64QAM发射链路阶段性实测星座图：1.8 GHz、5 MSym/s，RMS EVM 4.35%。”

指标表对应“64QAM发射链路阶段性实测，RMS EVM 4.35%（256符号窗口）”，不改写为V16或V17整机EVM指标，也不外推为全频段最坏值。

本图用于多种调制信号发射能力的一个例证，与PSK、FSK、LFM、脉冲及相位编码能力并列展示，不把QAM单列为平台唯一或主要功能。
