# 证据索引与可信边界

对话检索覆盖三个既有存档1111条消息及本次续接日志中截止采集快照的可见消息，范围从MATLAB/BRAM入门、RFSoC硬件、Linux/Qt、标定到V16提交。只精选与技能推导有关的内容，不导出系统/开发者消息、内部分析或完整私密聊天。

- [corpus_manifest.json](evidence/corpus_manifest.json)：存档文件SHA256、消息数、续接快照范围。不是把聊天的每句话当作当前事实。
- [artifact_sources.json](evidence/artifact_sources.json)：技术附件来源、原文件SHA、共享文件SHA、脱敏说明。
- c01..c07_chat.md：原记录编号/角色/时间及选定问答，去掉重复XML展开内容，替换私有IP和用户路径；数值及操作结果保留。
- c08_recent_chat.md：后续V16确认、源码包与资源对比的可见消息，时间使用原始UTC并保留源行号。

| 类别 | 主要原始记录 | 可支持的结论 |
|---|---|---|
| DMA | dma_register_diagnostic.txt、dma_after_reset.txt、dma_native_upload.txt、dma_native_play_stop.txt | 本板初始化顺序修正后数字链路通过；不是RF验收 |
| RRC | rrc_correction.md、rrc_digital_numeric.json、rrc_arm_native_contract.json | 索引错误纠正及修后数字回归；旧53.2%仅有纠错记录，未附旧完整IQ |
| 鼠标 | c03_cursor_chat.md、cursor_result.json | 该显示配置下人工交互恢复，不证明普适性能 |
| 存储 | c04_storage_chat.md | 日志报告空间不足→调整路径后用户确认上传通过；没有完整故障文件系统快照 |
| 标定 | c05_calibration_chat.md、assets/demo/power | 该次预留点相对参考读数的一致性；不是全频段绝对精度 |
| 覆盖 | known_endpoints.json、c06_coverage_chat.md | 两端已知输入的顺序扫描结果；不是1GHz瞬时带宽 |
| 发布 | v16_frozen.json、v16_board_test.log、source_recreation.json | 各回执写明的层级与范围，不合并成“任何场景全部通过” |
| 资源 | assets/demo/reports四份完整报告 | 可审查当前资源和旧失败时序；配置不同故严格对比拒绝 |
| 数值 | assets/demo/numeric及snapshot_packet_verification.json | 真实RTL仿真载荷样本和完整包验证记录；注入反例明确标合成 |

自检中为测试检查器而新造的“synthetic-device”“synthetic_setup”等只用于行为测试，不是其他队伍试用、真实器件测量或新的资源优化实验。
完整项目的V16源码与编译日志在同次源代码交付中；技能包可脱离工程运行离线演练，但不能脱离工程独自重编译仪器或验证RF。
