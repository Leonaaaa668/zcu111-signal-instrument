# FPGA证据驱动复现与验收技能包

对应指南3.3.5.2：从本项目与大模型协作中的真实观察、失败、修正和验证，提炼出可复用工作流、提示词、模板和校验脚本。不是把项目源码另换一个目录，也不只有文字经验总结。

## 十分钟上手

依赖：开发电脑Python3.10+，仅标准库；Windows/Linux均可。工具不连接板卡/网络或修改硬件。在本目录执行：

```sh
python fpga-evidence-workflow/scripts/self_test.py --work-root ./test_workspace
```

30项行为测试覆盖正确输入和反例；实际日志见validation目录。这些是技能检查器的测试，不是30次硬件验收。临时目录只在指定工作区中创建/清理；test_workspace不属于交付输入。

一键运行六个正反例演练（每次选择新的输出目录）：

```sh
python fpga-evidence-workflow/scripts/run_demo.py --output-dir ./demo_results
```

也可以进入fpga-evidence-workflow后逐条执行四项演练：

```sh
python scripts/engineering_checks.py rate-budget --input-rate-hz 2400000000 --decimation 2,8 --fft-n 65536 --header-bytes 256 --displays-per-second 10
python scripts/engineering_checks.py compare-numeric --reference assets/demo/numeric/reference.csv --actual assets/demo/numeric/actual.csv --mode exact --domain rtl_simulation
python scripts/engineering_checks.py power-check --context assets/demo/power/context.json --request assets/demo/power/request.json --readings assets/demo/power/holdout.csv
python scripts/engineering_checks.py compare-reports --before assets/demo/reports/before_utilization.rpt --after assets/demo/reports/after_utilization.rpt --context assets/demo/reports/contexts.json --before-timing assets/demo/reports/before_timing.rpt --after-timing assets/demo/reports/after_timing.rpt
```

预期：前三项PASS；最后一项NOT_COMPARABLE（退出码2），因为真实旧版保留Zoom0..6、当前只保留Zoom0，且完整约束等价性未证明。这是成功识别错误比较的演练。
将numeric的actual文件换成actual_corrupted_synthetic.csv，应FAIL；将power请求换成wrong_frequency_request.json，应NOT_COMPARABLE。故障注入和场景变更样例不冒充新RF测量。

## 每项的适用范围、用法、效果和来源

| 项目 | 适用场景/使用方法 | 本项目已验证效果 | 来源与失效边界 |
|---|---|---|---|
| M1 信号链契约与数据预算 | 高速信号拟送CPU/DMA；填契约、跑rate-budget、明确实际搬运对象 | 150MS/s原始16bit IQ为600MB/s；65536点谱包524544字节，约10次显示/s是约5.25MB/s的另一种数据流 | C06/实际架构；预算不是DDR实测，使用传输容器位宽 |
| M2 故障定位与初始化顺序 | MMIO读卡死、GUI输入或存储异常；按故障树做一次一个假设的实验 | DMA按控制时钟、正确复位链、DDR就绪顺序后完成48MB回读；1080p/Pixman恢复Qt鼠标 | C01/C03/C04；地址、GPIO和渲染配置不可跨板照抄 |
| M3 独立黄金参考 | 文件长度正常但DSP可能错；独立接收端验证、精确数值检查 | RRC约53.2%数字误差被发现并修正到8PSK/64QAM约0.12%；快照逐值和坏包检查通过 | C02；数字误差不是RF EVM；CSV演练为RTL仿真 |
| M4 有边界标定 | 数字幅度改为dBm；上下文约束、预留点三次读数 | -25dBm预留点-25.01/-25.01/-25.00，均值误差约-0.0067dB | C05；只证明特定参考条件的该点一致，不外推全频段或绝对精度 |
| M5 优化对比与时序证据 | 需要资源前后对比；先对配置再解析报告 | 正确区分旧92932 LUT/555 DSP与现72273 LUT/183 DSP的裁剪差异；旧WNS-0.109、现+0.140 | C08；实际数据不是严格等功能基线；同条件正例仅合成测试 |
| M6 冻结、复现和证据台账 | 防止GUI/驱动/位流混用；SHA清单、干净恢复、分层验收 | V16源码恢复、11项C++测试、6项RTL及20项参考测试，用户确认界面，新源码包校验通过 | C07/C08；V16未重做整镜像构建和断电冷启，不冒用V15确认 |

操作细节见[workflows](fpga-evidence-workflow/references/workflows.md)；真实失败与纠错过程见[casebook](fpga-evidence-workflow/references/casebook.md)。

## 给大模型使用

将fpga-evidence-workflow整个目录交给支持技能的工具，或复制到自己的用户skills目录；入口SKILL.md，Codex可用$fpga-evidence-workflow调用。本交付不会自动安装或改动本机技能配置。
不支持技能加载的模型可先读SKILL.md，再粘贴prompt_pack中的模板与自己的日志。不需要本队IP、账户、板卡或仪器。
换项目先替换板卡、工具版本、通道、位宽/打包、时钟复位、内存和参考面，再依验证计划验收。

## 对应指南

- 提示词工作流：六组可填充模板，要求输出可证伪实验和边界。
- 案例模板：契约、实验计划、决策记录、发布回执，含错误分支。
- 校验脚本：五类检查、30项行为回归、实际案例和故障注入反例。
- 踩坑清单：触发条件→区分检查→停止点→成功判据。
- 来源：检索1111条既有归档消息及后续可见续接消息；公开精选脱敏节选、技术记录和SHA索引，完整私密聊天不打包。

本包追求可复用、可验证和易上手，不承诺评分。没有伪造陌生队试用或PYNQ迁移实测；本项目为原生Linux/C++ Qt，不冒充通用PYNQ额外加分项。

## 文件与校验

fpga-evidence-workflow含SKILL.md、agents元数据、scripts、references流程/证据、assets模板/演练数据。validation含结构检查、行为回归、命令行演练与封装验证记录。

```sh
python fpga-evidence-workflow/scripts/engineering_checks.py verify-manifest --root . --manifest SHA256SUMS.txt
```

新编写说明、模板和脚本采用MIT（见LICENSE）；原始AMD报告版权头及来源记录保留。SHA256SUMS是最终文件清单，本地测试输出不属于交付输入。
