# 仿真与验证脚本、结果

命令从仓库根目录执行。当前V17测试见sim/README.md及results/v17。下表主体为V16及冻结硬件的历史证据，保留原日期；不把旧11项测试日志写成当前12项全部重跑结果。

| 验证内容 | 脚本/源文件 | 已有结果 |
|---|---|---|
| IQ拆分次序 | sim/rtl/tb_iq_split4.v | current/tb_iq_split4/simulation.log |
| TX停播/静音/欠载 | tb_dac_stream_guard.v | current对应子目录simulation.log |
| 质量与背压 | tb_sa_quality_control.v | current对应日志 |
| 前端请求/复位/配置 | tb_sa_frontend_control.v | current对应日志 |
| 双bank快照与包 | tb_sa_snapshot.v、sim/python/verify_sa_snapshot_sim.py | 6包含65536点精确比较，14种损坏/截断拒绝；current/snapshot_packet_verification.json |
| AXI结果控制 | tb_sa_result_control.v | current对应日志 |
| 真实AMD FFT模型 | tb_sa_fft.v、sim/python/verify_sa_fft.py | fft/narrow09_1024_numeric.json、fft/narrow09_8192_65536_numeric.json |
| V16历史C++业务/交互契约 | src/linux/apps/instrument_cpp/tests；历史SDK构建 | 11/11；report/validation/cpp_ctest.log |
| V17当前软件契约 | build_all.sh的Linux＋Qt配置12项；本机非Qt配置7项 | 本机7/7；sim/results/v17/host_ctest.log，Qt入口见sim/v17_validation.md |
| Python开发参考 | python3 -m unittest discover -s sim/python -p 'test_*.py' | 20/20；report/validation/python_tests.log |
| 收录证据重放 | python3 sim/replay_evidence.py | 原始包解码/SHA、FFT误差、模型残差、功率读数及刷新率重算 |
| Skill验证 | skill/fpga-evidence-workflow/scripts/self_test.py | 30行为测试及6CLI案例，见skill/validation |

表中current、fft均位于sim/results下。六项Verilog由sim/run_rtl_tests.ps1统一运行，命令见根README。它检查返回码、错误文本、PASS标志，随后运行快照独立数值与破坏测试。自写平台均为.v。

## 数值正确性的依据

FFT与NumPy参考比较复数分量，8192点最大误差约1.2563LSB、65536点约1.1680LSB，容差16LSB；功率平方和逐bin精确一致。结果在sim/results/fft，完整后续日志large_fft_validation.log。

旧status.txt是较早全点数任务暂停的原始状态，后续narrow09已完成1024/8192/65536点，新增明确结果为当前索引依据；其他未逐项确认的点数不写成已验收。

C++测试涵盖解析、波形、物理量、功率模型、Marker/VBW、流生命周期和TX事务等。快照逐样本比较确保输出不仅“有数据”，还与激励相符；损坏测试确保错误被识别。

控制仿真、FFT参考、软件契约、实板DMA/界面与仪器读数分别验证不同层次。数字波形参考EVM不代替RF EVM，实际射频结果见board/measurements.md。

波形入口、历史64QAM星座与当前C++ RF验收范围见[矢量信号源能力](../report/vector_signal_source.md)。FSK属于自定义I/Q扩展说明，没有新增或声称已通过FSK射频测试。
