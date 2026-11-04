# 大模型协作记录

## 项目与记录范围

项目：基于RFSoC的信号发生与频谱分析一体化教学实验平台。

当前交付为V17、应用0.11.0，硬件沿用narrow09；以下历史片段保持当时的角色、记录编号、日期与验证对象。本文件从设计报告第6章单独整理，是精选协作记录，不是全量聊天导出。仅在本文件及设计报告保留这些经确认公开的片段；其他仓库说明使用技术总结。

协作形式为交互式模型与工具配合：人提出目标、约束及实板反馈，助手参与RTL、C++、参考验证与脚本实现，编译器、仿真、数据记录和仪器读数提供检验。报告没有列明各轮完整模型版本，因此不补填历史型号。

## 一 软件部署约束与提示词

来源：设计报告6.1节；用户输入，记录0716，2026年10月29日。

> 请直接写上板的程序吧，我希望这个程序是单独运行在zcu111上的，由ps来完整管理整个仪器设备，脱离上位机单独运行的情况。咱们不是PYNQ环境，没有Python，请用C++ Qt

这条输入约束了部署位置与软件栈。后续波形生成和频谱控制使用原生C++/Qt，Linux驱动负责DMA，Weston提供本地显示与输入；开发电脑上的Python参考仍用于独立检查。

该节未逐字列出这一轮的完整模型回复，本文件不将后续技术总结伪装成当时的回答。

## 二 DMA首次访问无响应与自我纠正

来源：设计报告6.2节。现象为DMA寄存器读取不返回，随后出现RCU stall；GPIO或RFDC能响应仍不足以证明另一AXI通路就绪。

### 记录0829 2026年10月30日10时44分

模型回答关键句：

> DMA读取仍未返回，说明仅保留时钟没有解决问题。

观察与动作：仅保留时钟的假设未解决问题，继续核对100 MHz控制域、300 MHz DDR域及复位依赖。

### 记录0833 同日10时56分

模型回答关键句：

> 这次复位请求被固件接口拒绝了

工程观察与动作：本工程通过EMIO95输出pl_resetn0，按实际连接核查复位链和资源占用，不能只沿用被拒绝的固件请求。

### 记录0834 同日10时59分

模型回答关键句：

> CDMA和MM2S状态寄存器都能正常读取

后续动作：在控制时钟与本板复位顺序正确后继续核对DDR和RFDC；寄存器可读只是分层验证中的一步。

### 记录0835 同日11时03分

模型回答关键句，省略号沿用报告中的关键句摘录：

> 10 ms、48 MB……回读比较和SHA256校验

后续动作：先以10 ms、48 MB上传与回读核对数据搬运，再进入播放。

### 记录0836 同日11时07分

模型回答关键句：

> 播放期间DMA持续供数，保护状态未报欠载

当时结果：1 s播放/停止检查通过，停止后恢复数字静音。该结果是数字链路与状态检查，不能单独推出RF频率、功率或调制质量。

### 提炼结果与技术附件

启动依赖固化为控制时钟、实际复位链、DDR就绪、RFDC与收发使能的顺序。一次最小实验检验一个假设，保留失败分支，按MMIO、回读、播放停止分别建立判据。EMIO95是本板连接，迁移时须重新检查。

- [首次DMA诊断](../skill/fpga-evidence-workflow/references/evidence/dma_register_diagnostic.txt)
- [复位后状态](../skill/fpga-evidence-workflow/references/evidence/dma_after_reset.txt)
- [上传与回读记录](../skill/fpga-evidence-workflow/references/evidence/dma_native_upload.txt)
- [播放停止检查](../skill/fpga-evidence-workflow/references/evidence/dma_native_play_stop.txt)

## 三 RRC相位与符号索引

来源：设计报告6.3节；模型回答，记录0727，2026年10月29日21时45分。

> 补充的数字波形检查发现了RRC成形的索引问题：符号卷积方向与采样相位的符号不一致，会影响PSK/QAM质量。我已定位到代码，正在修正并重新验证。

文件长度、幅度和SHA256通过不能证明成形滤波数学正确。定位关系为：符号项使用`symbols[pos−k]`时，`n/T−(pos−k)=phase/T+k`，滤波系数须按对应方向取值。修正后重编译、在ARM生成数字IQ，并由独立接收参考进行匹配滤波与符号检查。报告记载记录0728在同日22时03分反馈回归通过；未提供该条完整逐字回答，本文件不补写。

| 当时数字验证对象 | 归一化误差 | 范围 |
|---|---:|---|
| 8PSK | 0.12036018% | ARM数字IQ与独立匹配滤波 |
| 64QAM | 0.12064460% | ARM数字IQ与独立匹配滤波 |

以上不是RF EVM；早期64QAM外部解调的4.35%属于另一射频测量。技术记录见[数字回归](../skill/fpga-evidence-workflow/references/evidence/rrc_digital_numeric.json)及[索引修正说明](../skill/fpga-evidence-workflow/references/evidence/rrc_correction.md)。

## 四 生成文件与上传对象关联

以下叙述出自设计报告6.4节；引号内沿用报告标注的用户反馈与日志，其余为技术过程总结，不补造缺失的模型回答。

记录0838中用户反馈：“完成后点UPLOAD TO DDR4并没有找到刚才生成的文件。”继续读取System/Log，记录0841出现“Insufficient storage for a 10ms IQ record”。据此把定位方向从上传列表转到生成任务和可写空间，随后使用/tmp下成功生成的唯一文件，上传复用同一路径。

记录0845用户确认：“TX DDR round-trip and SHA256 passed; loaded, NOT playing.” 这条闭环把按钮触发、文件成功写入、上传校验和实际播放区分开，最终形成容量预检查、任务状态反馈与成功文件直接关联的应用行为。

技术证据见[RRC修正说明](../skill/fpga-evidence-workflow/references/evidence/rrc_correction.md)、[数字回归](../skill/fpga-evidence-workflow/references/evidence/rrc_digital_numeric.json)、[ARM契约检查](../skill/fpga-evidence-workflow/references/evidence/rrc_arm_native_contract.json)和[案例C04技术总结](../skill/fpga-evidence-workflow/references/casebook.md)。不引用已从当前分支删除的聊天节选文件。

## 五 协作经验转为可复用技能

来源：设计报告6.5至6.7节。先从工程案例抽取判断问题，再形成模板、检查器和正确/错误输入，最后用行为回归确认工具能接受正确输入，也能拒绝不满足条件的输入。

| 工作流 | 可复用判断 | 替换到新项目时需要重填 |
|---|---|---|
| M1 数据预算 | 区分原始IQ、谱包、显示率与存储需求 | 采样率、位宽、容器和包头 |
| M2 启动依赖 | 一次实验检验一个时钟、复位或所有权假设 | 板卡连接、DDR、地址和内核 |
| M3 独立数值参考 | 不以文件传输成功替代DSP正确 | 黄金参考、数值域、容差 |
| M4 标定上下文 | 固定参考面，训练与独立检查分开 | 频率、波形、线缆、仪器和量程 |
| M5 比较可比性 | 先核对功能/约束再比较资源和时序 | 工具、器件、阶段及配置 |
| M6 版本与交付 | 分开记录源码、硬件、运行包和验证层级 | 版本、构建入口及新验收结果 |

30项离线行为测试、六项演练及预期拒绝分支见[Skill说明](../skill/README.md)和[演练汇总](../skill/validation/summary.json)。这些检查器的效果不等同于新板卡迁移或新的RF测量。

以下命令在开发电脑的仓库`skill`目录执行，用于离线验证协作工具，不属于ZCU111仪器运行依赖：

```sh
python fpga-evidence-workflow/scripts/self_test.py --work-root ./test_workspace
python fpga-evidence-workflow/scripts/run_demo.py --output-dir ./demo_results
```

## 六 与当前交付的关系

历史协作片段用于解释设计取舍与纠错，不改写为V17新测数据。当前版本与复现入口见[版本清单](../board/releases/v17.json)、[V17说明](../docs/v17_release.md)及[完整复现步骤](../build/reproduce.md)。

本文件为UTF-8 Markdown，可作为大模型协作记录单独提交。没有附带未筛选的全量聊天、密码、私钥或本机聊天存档路径。
