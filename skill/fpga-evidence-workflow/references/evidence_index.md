# 技术附件索引

本索引仅定位工程技术记录与案例配套数据，不包含原问答、聊天语料目录或对话编号。旧实验按原日期解释，当前V17状态以项目版本清单为准。

| 类别 | 附件 | 支持的结论与边界 |
|---|---|---|
| DMA | dma_register_diagnostic.txt、dma_after_reset.txt、dma_native_upload.txt、dma_native_play_stop.txt | 初始化依赖定位及数字链路检查，不等同RF验收 |
| RRC | rrc_correction.md、rrc_digital_numeric.json、rrc_arm_native_contract.json | 数学索引修正与数字回归，不等同RF EVM |
| 输入 | cursor_result.json | 特定显示配置下的交互结果，不证明普适性能 |
| 存储 | 案例集C04 | 生成产物与上传对象关联的方法总结 |
| 标定 | ../assets/demo/power | 特定参考条件的预留点一致性，不外推绝对精度 |
| 覆盖 | known_endpoints.json | 顺序扫描端点检查，不是1 GHz同时接收 |
| 发布 | v16_frozen.json、v16_board_test.log、source_recreation.json | 历史版本的构建与验证范围 |
| 资源 | ../assets/demo/reports | 完整报告和配置用于可比性演练 |
| 数值 | ../assets/demo/numeric、snapshot_packet_verification.json | 数值检查与标注为合成的故障注入 |

技能包可独立执行离线演练，但不能脱离实际工程重编译仪器或验收RF。合成样例只用于验证检查器行为。
