# FPGA工程验证与复现技能包

本目录保留可复用工具、工作流模板、技术案例总结及配套测试附件，不包含聊天正文、问答节选或聊天语料索引。案例按观察、实验、修正、验证与边界归纳，不把助手陈述当作独立测量结果。

## 快速上手

开发电脑Python3.10+，仅标准库；无需本队板卡或板端Python。在本目录执行：

```sh
python fpga-evidence-workflow/scripts/self_test.py --work-root ./test_workspace
python fpga-evidence-workflow/scripts/run_demo.py --output-dir ./demo_results
```

测试目录应为可写工作区，演练输出选择新目录。30项行为测试验证检查器的正反例，六项演练覆盖数据预算、数值比对、标定范围、错误拒绝及资源报告比较；不将合成样例当作新硬件实测。

## 工具与适用范围

| 工具 / 方法 | 用途 | 验证与边界 |
|---|---|---|
| rate-budget | 计算采样、抽取、数据容器与快照传输预算 | 算术预算，不是DDR实测；多通道、协议空隙和效率另算 |
| compare-numeric | 连续索引、有限数、整数精确比较和误差范围 | 需要独立参考；字节一致不能代替算法正确 |
| power-check | 核对标定参考面、频率、波形、范围与独立检查点 | 拒绝把跨频估算当作已校准读数；不限制仪器以ESTIMATED方式生成其他频点 |
| compare-reports | 核对工具、器件、功能和约束后比较资源/时序 | 不可比时返回NOT_COMPARABLE，不把功能删减当等功能优化 |
| verify-manifest | 按使用者提供的清单核对文件 | 通用可选工具；本次不提供项目全文件校验清单 |
| 工作流 / 模板 | 信号链契约、最小实验、纠错与发布记录 | 参数应按新板卡替换，案例中的地址/GPIO不可照搬 |

技术总结见[案例集](fpga-evidence-workflow/references/casebook.md)、[工作流](fpga-evidence-workflow/references/workflows.md)和[技术附件索引](fpga-evidence-workflow/references/evidence_index.md)。

## 项目中形成的方法

- 区分150 MS/s复数原始数据与FFT功率快照的数据量，避免把采样率当作显示率。
- 按时钟、复位、DDR、资源所有权逐层定位AXI访问失败。
- 用独立接收算法发现RRC数学错误，再以数字回归检查修正。
- 按USB事件、Weston会话、Qt事件和渲染路径区分输入问题。
- 保留标定上下文和独立检查点，区分点一致性、频率推广估算和绝对精度。
- 先核对功能/约束再比较资源，版本和验证层级分别记录。

## 复用与指南对应

整个fpga-evidence-workflow目录可交给支持技能的工具，入口为SKILL.md；提示词模板位于assets/templates/prompt_pack.md。模板是后续复用方法，不冒充原始交互记录。使用前替换板卡、工具、时钟复位、位宽、内存、接口和测量参考面。

本目录用于指南中的可复用技能包及技术案例提炼。指南另要求的协作交互材料应由队伍按提交渠道单独准备；公开技能包无需包含原问答正文。该项目采用原生Linux/C++ Qt，不宣称已经完成PYNQ或其他板卡迁移实测。

新编写说明、模板与脚本采用MIT；原始AMD技术报告保留相应版权与来源。
