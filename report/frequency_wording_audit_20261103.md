# 载频与功率表述审查 2026年11月3日

全仓库文本扫描包含文档、生产C++、测试、元数据和标定记录。未发现主说明明确断言CW硬件只能1790 MHz；容易误解的标定范围说明已补充为载频与模型分别说明。

生产代码中的1790 MHz报错和检查限定CW功率模型，保留它们以保持冻结版本可复现；原始测量、历史失败和模型资源不改数值。它们不构成CW数字幅度模式的频率限制。FSK为自定义I/Q扩展，当前未增加FSK内置发生器或RF验收。

| 检查文件 | 处理说明 |
|---|---|
| `README.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `board/bringup.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `board/measurements.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `build/reproduce.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `data/README.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `report/hardware_performance.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `report/materials_provenance.json` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `report/status_and_scope.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `sim/verification_catalog.md` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `skill/README.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `data/modulation/digital_window_check.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `data/modulation/photo_settings.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `data/modulation/plan.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `data/calibration/rx_live/calibration.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `data/calibration/rx_live/live_acceptance.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `data/calibration/tx_current/tx_cw_4025g_verification.json` | 历史数据或回执：保留频率、读数、波形身份和验证边界 |
| `sim/results/cpp/arm_native_waveform_contract.json` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `sim/results/cpp/arm_waveform_head_checks.json` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `sim/results/cpp/arm_waveform_numeric.json` | 已核对上下文；相关入口补充矢量能力、CW载频与标定区别 |
| `skill/fpga-evidence-workflow/SKILL.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/validation/rtl_payload.json` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/validation/synthetic_one_bit_fault.json` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/casebook.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/pitfalls.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/workflows.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/assets/templates/prompt_pack.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/c02_rrc_chat.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/c04_storage_chat.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/c05_calibration_chat.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/c07_freeze_chat.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/rrc_arm_native_contract.json` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/rrc_correction.md` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `skill/fpga-evidence-workflow/references/evidence/rrc_digital_numeric.json` | 可复用边界/真实案例：保留不将CW功率直接推广为调制平均功率的检查 |
| `src/linux/apps/instrument_cpp/resources/rx_cw_calibration.json` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/resources/tx_catalog.json` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/resources/tx_cw_4025g_verification.json` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/src/gui.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/src/rx_power.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/src/tx_power.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/tests/cw_context_tests.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/tests/physical_tests.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |
| `src/linux/apps/instrument_cpp/tests/waveform_tests.cpp` | 冻结生产实现/测试/模型：保留原始条件；对外范围在vector_signal_source.md说明 |

新增历史64QAM照片及条件，保留其旧生成路径身份；新16QAM错误中心照片只在修订Word中作排错案例。视频说明稿在report/video，修订Word独立交付；依用户最新要求，视频仅交修改建议，不交MP4，也不在源码仓库装入大型媒体。

V16硬件与C++生产源码未改。本次源码身份和原始证据哈希由build/check_submission.py核对。
