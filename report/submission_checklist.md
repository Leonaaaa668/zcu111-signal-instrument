# V16 工程包提交对照清单

整理日期2026-11-02；对应用户提供指南3.3.5.1和3.3.5.4。路径相对仓库根目录。

| 要求 | 文件与目录 | 整理结果 |
|---|---|---|
| 简介＋复现 | README.md、build/reproduce.md、board/bringup.md | 已写；工具、恢复、编译、启动和验收逐步说明 |
| 完整工程与源码 | src/fpga、src/linux、src/baremetal_reference | 自写生产源码及配置已收齐；见source_completeness.md |
| 仿真与正确性 | sim/verification_catalog.md、sim/results、report/validation | 控制仿真、真实FFT数值、包校验、C++/Python及日志 |
| 综合/实现/资源/时钟 | report/hardware_performance.md、build/reports/fpga | 补31份可取得的综合报告；整机routed、时序、CDC、DRC、布线日志 |
| 上板/实测/参考比对 | board/bringup.md、board/measurements.md、board/results、data/comparison | 冻结脚本、V16日志、原始样本、频率和功率比对 |
| 可复现构建 | build/README.md、build/fpga、build/linux、make_sd_runtime.py | 恢复、综合实现导出、Linux软件、SD包与校验 |
| 技能包 | skill/README.md及其子目录 | 保留工具、模板、技术案例总结和测试附件 |

## 工程交付方式

BD Tcl、Verilog、约束、系数、配置和构建入口构成源码工程。XPR/BD、IP输出、BIT/XSA、ELF/KO与BOOT/image由脚本重新生成，不装入巨大缓存和SDK。原冻结运行包名称与哈希在board/reference中可核对。

当前源码为V17，历史V16记录保留；本次不访问板卡、不新跑综合实现或完整Linux镜像，离线校验保留实际日期。重新构建后使用新哈希、报告和板测回执。

## 另需队伍完成的比赛材料

- 正式设计报告作者、学校、分工及最终排版；本包架构、性能和验证文档可作技术依据。
- 最终3分钟内MP4/H.264视频与大小检查。
- 公开源码地址：https://github.com/Leonaaaa668/zcu111-signal-instrument；最终提交前核对公开环境的README/依赖。
- 指南要求的协作交互材料由队伍按提交渠道单独准备；公开Skill保留技术总结，不捆绑原问答正文。

解压后将七个目录及根README.md直接放到仓库根。ZIP另作下载附件即可。
