# FPGA 综合、实现与关键性能报告

硬件narrow09，ZCU111 xczu28dr-ffvg1517-2-e，Vivado2025.2；设计adc3_capture_wrapper。V16及当前V17更新用户界面与软件功能，沿用该硬件。下列综合/实现结果来自2026-10-28至29日的冻结构建，此次整理核对其比特流SHA与冻结硬件记录一致。

## 整机资源（Routed）

| 资源 | 使用 | 器件可用 | 占用 |
|---|---:|---:|---:|
| CLB LUTs | 72273 | 425280 | 16.99% |
| CLB Registers | 113196 | 850560 | 13.31% |
| Block RAM Tile | 602.5 | 1080 | 55.79% |
| DSP | 183 | 4272 | 4.28% |
| URAM | 0 | 80 | 0% |

来源：build/reports/fpga/utilization.rpt。这里Registers使用原报告整行统计，不将其中1个AND/OR寄存器误写成FlipFlop。

## 综合证据

build/reports/fpga/synthesis/保存31份目前可取得的顶层与IP综合资源报告，及top_synthesis.log。顶层采用OOC/IP黑盒，报告LUT为0是阶段口径，不代表整机无逻辑。IP逐项报告用于分析局部结构；整机资源统一取链接并布线后的报告，不把子模块数值简单求和当作最终值。

顶层综合日志记录0 Errors、0 Critical Warnings；实现日志implementation_run.log记录Bitgen Completed Successfully。报告/日志含历史告警；不以阶段成功替代告警审查。

## 时序与布线

| 指标 | 原始结果 |
|---|---:|
| WNS / TNS | +0.140 ns / 0 ns |
| WHS / THS | +0.010 ns / 0 ns |
| WPWS / TPWS | +0.014 ns / 0 ns |
| setup / hold / pulse失败端点 | 0 / 0 / 0 |
| 未约束内部最大延时端点 | 0 |

原始timing_summary.rpt给出“All user specified timing constraints are met”。route_status、postroute_signoff、bus_skew、drc、methodology、cdc_details一并保存；具体DDR最小偏斜修复与复核见postroute_signoff.rpt和narrow09_report_review.json。

仍保留RFDC/clock-wizard主时钟方法学提示、未启用hard-DAC时钟引脚、DDR RESET_N输出延时及XPM/ILA相关CDC检查项，复核依据在narrow09_report_review.json。不是“零告警”报告。

## 时钟与采样率

| 位置 | 设计配置/数据率 | 证据 |
|---|---|---|
| ADC与DAC | 2.4 GS/s | BD配置、时钟表、RFDC初始化日志 |
| RFDC与PL流接口 | 每周期4个复数点，名义300 MHz | BD、clock wizard配置 |
| RX经过RFDC抽取2 | 1.2 Gcomplex samples/s | RFDC配置 |
| RX经过PL抽取8 | 150 Mcomplex samples/s | RTL/系数、包元数据 |
| PS AXI控制 | 100 MHz | clk_pl_0 |
| DDR用户/UI域 | 约300.21 MHz | clocks.rpt、timing_summary.rpt |

STA摘要中sample_clock和dac_sample_clock是299.985 MHz、RFADC1_CLK/RFDAC1_CLK是149.993 MHz，由报告6.667ns等有限小数约束表示；该表不是采样时钟频率计实测，不能将该舍入差当作板上ADC采样率误差。硬件名义配置2.4GS/s与数据契约独立核对。

## 仪器关键性能与条件

| 项目 | 结果与条件 | 来源 |
|---|---|---|
| FFT | 已验收1024/8192/65536点 | sim/results/fft与板测样本 |
| N65536频率间隔 | 150e6/65536 = 2288.818359375 Hz | 公式与measurement.json |
| Hann RBW | N65536 ENBW约3433.2277 Hz | 窗系数统计；实际RBW有量化档位 |
| VBW | 线性功率多帧平均的等效量，最多256帧 | spectrum_tools.cpp；M=1取RBW，M>1取min(RBW,0.38*RBW/(M-0.8)) |
| Sweep time | Live且Span>150 MHz时设置最小采集停留预算；窄带/流显示使用Auto，实际整轮耗时另显示 | GUI与扫描实现 |
| V16连续显示 | 82 displayed / 81 painted / 8.416s，N65536、平均1 | report/validation/v16_20261101/board_test.log |
| 1GHz顺序扫描 | 13段；N8192、平均1端点两轮约2.654/2.657s | board/results/tuning_20261031 |
| 10ms发射文件 | 1.2G复数点/s × 0.01s × 4字节 = 48000000字节 | 波形元数据与上传回读日志 |
| RX CW独立点 | 参考-45.70665dBm，实时GUI -45.735dBm | data/calibration/rx_live/live_acceptance.json |
| TX CW补偿独立点 | 目标-25dBm，3次-25.01/-25.01/-25.00dBm | data/calibration/tx_current |

刷新约9.74次/s是这段条件下的显示观测；不是保证所有参数下的帧率，也不是无损原始IQ速率。1GHz为顺序扫描范围。CW功率一致性限定1790MHz/相同参考设备与线缆；表中独立点结果不推广成全频段绝对精度。

发射为可编程I/Q矢量链路，CW频率能力与1790 MHz功率模型范围分别说明，见[矢量信号源能力](vector_signal_source.md)。全带发射幅度平坦度和新增变频链不作为已有指标。
