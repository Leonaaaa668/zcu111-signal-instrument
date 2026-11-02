# 指南3.3.5.2逐项对应

| 要求 | 交付位置 | 如何检查 |
|---|---|---|
| 放在skill目录并有README | skill/README.md | 解压即能找到，含快速上手、项目矩阵和迁移方式 |
| 适用范围 | README矩阵、SKILL路由、workflows | 不要求本队板卡；硬件动作与纯软件检查明确区分 |
| 使用方法 | README命令、run_demo、自检、四种模板 | 只有Python标准库即可运行演练；新结果写入指定新目录 |
| 失效条件 | workflows每节、pitfalls、脚本scope | 频率/波形/参考面、功能不一致、未知约束等会拒绝 |
| 已验证效果 | casebook、evidence、validation | 人工问答、工具日志、原/共享SHA相互对照，非仅助手自述 |
| 从哪些失败与观察总结 | 八案例闭环 | DMA保留时钟失败、复位接口被拒绝、RRC索引、鼠标、存储、资源不可比等保留纠错 |
| 提示词工作流 | assets/templates/prompt_pack.md | 六组输入/输出/判据模板；注明为后续提炼而非历史逐字提示词 |
| 案例模板 | project_contract、verification_plan、decision_record、release_receipt | 可直接填入另一队的板卡/工具/接口与记录 |
| 校验脚本 | engineering_checks.py、self_test.py、run_demo.py | 30项行为检查、六个命令行演练；正反例均覆盖 |
| 踩坑清单 | references/pitfalls.md | 按触发条件、下一项检查、停止点与验证范围组织 |
| 可复用性 | 参数化工具与新项目使用说明 | 无硬编码IP/登录/驱动加载；真实板卡参数仅保留在案例证据中 |

没有声称陌生队/PYNQ迁移已经实测。可在其他框架复用文件/报告/数值检查，但是否符合通用PYNQ额外加分应由实际实现与评审判定，本包不虚构该项。
本包自身新增脚本的测试与项目已有板测分开陈述；软件回归不代表模拟RF性能。高分目标来自可用性和可核查性，不承诺分数。
