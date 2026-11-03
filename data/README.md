# 数据、参考与比对索引

| 目录 | 内容与用途 |
|---|---|
| rx_loop65536 | 实板频谱原始包、回执、采集元数据和C++测量结果；开发电脑可重放 |
| tuning/tone1300 | 1300MHz实板原始包/回执/测量/验收，支持NCO端点验证 |
| calibration | 原始手动读数points.csv、训练/保留点模型评估；historical_fsva为早期TX表 |
| calibration/rx_live | 最终RX正式模型与实时GUI验收 |
| calibration/tx_current | 当前4025G条件下TX补偿与独立点；生产资源中的相同JSON副本 |
| comparison | 本次整理的频率/功率/幅度差CSV、指标JSON及其来源 |
| modulation | 当前C++16QAM原始频谱/IQ设置照片与波形元数据；RF EVM验收待补 |

照片使用原始测量图，不把AI清理图当测量证据。原始文件中的板端2025时间为未校准RTC，验收日期以客户端及对应记录为准。data不是完整原始IQ档案，频谱bin为处理结果；不要把bin扩展名自动解释成时域IQ。

历史64QAM原始照片和手工转录的测量条件见[history_qam64_20261025](modulation/history_qam64_20261025)，用于说明历史矢量播放能力，不替代当前C++版本的RF验收。

## 多调制与频率规划补充

新增[脉冲总览/放大](modulation/history_pulse_20261025)、[LFM频谱与既有I/Q分析](modulation/history_lfm_20261025)，与历史64QAM星座共同组成[实测图册](../docs/waveform_gallery.md)。照片不改读数，不把历史测量标成V16新验收。
[频率规划结果](bandplan/offline_check.json)由`sim/check_bandplan.py`离线核对，区分1 GHz工作带、960 MHz规格推导TX通带、150 MS/s复采样率及100 MHz扫描窗口。
