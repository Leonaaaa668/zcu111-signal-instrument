# 上板实测与参考结果比对

下表均来自已有真实记录，2026-11-02重新计算并汇总。频率/功率读数保留各自版本和参考条件；V16沿用同一narrow09硬件和已纳入的算法/模型。此次没有重新接板测量。

| 项目 | 参考/设置 | 实测/分析 | 差值与意义 | 原始来源 |
|---|---|---|---|---|
| 1790MHz单音回环，N65536 | 名义1790000000Hz | 插值1789999986.616339Hz | -13.383661Hz，单次估计 | data/rx_loop65536/measurement/measurement.json及原始bin |
| 1300MHz窄带，RXcenter1310MHz | SMB1300000000Hz | 1300000046.6104405Hz | +46.6104405Hz，仪器/源参考未做绝对校准 | data/tuning/tone1300 |
| 1300MHz顺序扫描，N8192 | SMB1300000000Hz | 峰格1300009765.625Hz | +9765.625Hz，18.31kHz格距量级 | board/results/tuning_20261031/known_endpoints_acceptance.json |
| 2300MHz顺序扫描，N8192 | SMB2300000000Hz | 峰格2299990234.375Hz | -9765.625Hz | 同上 |
| 回环数字幅度减半 | 理论-6.02363036dB | -5.91934267dB | +0.10428769dB；原容差0.5dB | board/results/large_fft_20261029/summary.json |
| RX CW离线独立点 | 4025G同根线末端参考约-45.70665dBm | 模型保留点残差-0.06435dB | 未参与拟合；模型筛选0.1dB以内 | data/calibration/fit_affine_assessment.json |
| RX CW实时独立点 | 同参考-45.70664875dBm | GUI用户读-45.735；保存值-45.73527678 | 保存值差-0.02862803dB | data/calibration/rx_live/live_acceptance.json |
| TX CW补偿独立点 | 目标-25.00dBm | -25.01/-25.01/-25.00dBm | 算术均值-25.006667，差-0.006667dB | data/calibration/tx_current/tx_cw_4025g_verification.json |

## 功率参考条件

RX：SMB100A产生1790MHzCW，用4025G先测同根线接收端功率，再将接收端接J1；FFT65536、Hann、中心1800MHz、平均16的实时检查有16包SHA核对记录。-60dBm档参考读数散布超门限而排除，原读数仍保留。原始训练表不是所有行都已测完，以status列及正式模型为准。

TX：J7→同一线缆→4025G，1790MHzCW；中心1790MHz、Span50kHz、RBW/VBW1kHz、参考0dBm、衰减20dB、前放关、幅度偏移0。原表-40/-30/-20dBm三档与预留-35dBm读数完整保留；基于现有参考条件增加+0.06dB命令补偿，另用-25dBm独立点复核。该点当时旧界面命令-24.94dBm；新版模型自动处理此补偿，不再手工多加。

本表为给定条件下与参考设备的一致性，参考设备证书/不确定度未齐，不作全频段绝对0.1dB承诺。修改频率、线缆、波形、FFT或参考面后重新验证模型适用性。

## 软件与交互证据

report/validation/v16_20261101/board_test.log含Power、Marker与连续RX；V16实测82次显示、81次绘制、8.416s。用户确认鼠标、CW model标签及Run Single正常，冻结回执在board/reference。GUI平均/VBW/FFT会影响刷新，不把加速录像当作实时指标。

当前C++16QAM有原始频谱照片和参数，位于data/modulation；RF EVM/星座和一般调制平均功率待补。2026-10-25由开发电脑离线生成I/Q、再上传板卡播放的星座测试属于历史发射链路验证，不作为当前C++生成路径的完整RF验收。当前V17的内置波形在板端ARM上生成，不需要上位机或板端Python。

可机器读取的汇总为data/comparison/measurement_comparison.csv、tx_power_readings.csv、metrics.json。在开发电脑上离线重算用python3 sim/replay_evidence.py；这不是板端仪器运行依赖。

载频与模型范围说明：1790 MHz限定的是本节CW功率参考条件。当前V17在Digital peak和CW model模式均接受1300–2300 MHz，非1790 MHz的CW model显示ESTIMATED；可编程I/Q矢量发射、波形种类与变频扩展见[能力说明](../report/vector_signal_source.md)。
