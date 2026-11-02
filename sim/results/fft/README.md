# FFT 历史记录阅读顺序

`status.txt` 是较早全点数任务暂停的原始状态；`narrow09_1024_numeric.json` 与 `narrow09_8192_65536_numeric.json` 是之后完成的 narrow09 数值验证。`large_fft_validation.log` 包含8192/65536点模型输出及比较通过记录。

已验证1024、8192、65536点。功率平方和逐bin精确一致，FFT复数分量在记录容差内。此次整理只回读这些结果，没有重新执行耗时FFT模型。说明和重新仿真入口见 `sim/verification_catalog.md`。
