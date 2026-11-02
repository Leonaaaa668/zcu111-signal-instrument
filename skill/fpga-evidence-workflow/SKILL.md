---
name: fpga-evidence-workflow
description: "Validate FPGA/SoC signal-processing prototypes using clock/reset and stream contracts, independent numerical references, comparable resource reports, scoped calibration, and reproducible releases. Use for bring-up, verification and delivery with the user's actual board and framework."
---

# FPGA evidence workflow

把用户目标转成可检查的接口、最小实验和交付证据。保留用户选择的RTL语言、框架和现有工程。不要把ZCU111案例的地址、GPIO、时钟表或PetaLinux版本直接移植到别的板卡。

## 按任务选择资料

| 当前任务 | 读取/执行 |
|---|---|
| 算法到PL/PS划分、采样/DMA带宽 | workflows.md A、rate-budget |
| DMA/MMIO卡死、时钟/复位、GUI输入 | workflows.md B、pitfalls.md |
| 定点DSP、IQ顺序、RRC/FFT数值验证 | workflows.md C、compare-numeric |
| dBm设置、标定与预留检查点 | workflows.md D、power-check |
| 资源/时序优化前后对比 | workflows.md E、compare-reports |
| 从源码复现、版本冻结及提交 | workflows.md F、verify-manifest |

入口资料：[工作流](references/workflows.md)、[故障树](references/pitfalls.md)、[真实闭环](references/casebook.md)、[证据索引](references/evidence_index.md)。只加载当前任务需要的部分。

## 实施方式

1. 用[契约模板](assets/templates/project_contract.json)记录目标、数据类型、时钟/复位/所有权、版本和测量参考面。未知项保留未知，不能用案例参数填补。
2. 把观察、假设、修改、验证分开。为下一项实验写成功/失败判据、允许的副作用和停止点；参考[实验计划](assets/templates/verification_plan.md)。
3. 优先用能否证当前假设的最小实验。MMIO首次读已经挂起时不要重复试读；“只读”不保证总线访问不会卡死。
4. 在已有授权范围内完成必要检查。技能脚本不会连接硬件；复位、下载、驱动加载和RF播放的授权及接线由实际任务决定，技能不是额外授权。
5. 用[决策记录](assets/templates/decision_record.md)保留失败假设、原始证据、改动版本和边界。观察不支持时改结论，不改日志或显示坐标来制造一致。
6. 区分仿真、数字文件、板上数字链路、人工交互、RF测量、完整冷启动。一个层级通过不能替代另一个层级。

缺少板卡状态时先推进离线工作，只询问真正影响下一步的缺失条件。答复写入台账，避免重复要求已完成的测试或读数。

## 可执行检查

运行 `python scripts/engineering_checks.py --help`。全部使用Python标准库，不连接JTAG、串口、网络、MMIO或仪器；`--output`只写新文件。

- rate-budget区分原始IQ与频谱快照的字节预算，不把计算值当作DMA吞吐。
- compare-numeric验证连续唯一索引和有限数；精确整数比较不经浮点，避免uint64低位错误被舍入掩盖。
- compare-reports核对器件/工具/阶段及声明的功能/参数；未知或功能变化时输出NOT_COMPARABLE，不生成优化百分比。
- power-check限定频率、波形、参考面、接线场景和电平范围，保留未参与拟合的检查点；不输出绝对精度认证。
- verify-manifest拒绝路径越界/重复，检测缺失或改动；字节一致不等于功能正确。

退出码0=本项PASS，1=数据不满足判据，2=不可比或证据不足。看JSON的status与scope，不把“正确拒绝错误输入”当作工具故障。

## 提示词与迁移

给另一模型派发任务时填充[六组提示词](assets/templates/prompt_pack.md)。模板是从真实经验提炼的后续复用工作流，不冒充当时逐字提示词。引用效果时同时给出问答节选和技术证据，避免仅凭助手说“通过”。
脚本和记录格式可跨板卡/题目复用；硬件顺序须依据新工程的XSA/BD/设备树确定。本项目是原生Linux+C++ Qt，没有PYNQ/其他板卡迁移实测，不宣称通用PYNQ额外加分。
不把分段扫描写成瞬时带宽、数字文件EVM写成RF EVM、Marker峰值写成QAM平均总功率，或功能删减写成严格等功能优化。
