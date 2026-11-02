# 推荐目录与材料清单

V16，2026-11-02整理；与指南3.3.5.4的七个目录对应。下面列出主要入口，完整逐文件清单见file_inventory.csv。

```text
zcu111_v16_materials_20261102/
├── README.md                         项目简介、亮点、复现入口
├── src/
│   ├── fpga/                         18份Verilog/头文件、BD、约束、系数与时钟
│   ├── linux/                        C++Qt、驱动、RFDC、PetaLinux配置与SDT
│   └── baremetal_reference/          历史接口参考
├── sim/
│   ├── verification_catalog.md       仿真/验证方法与结果索引
│   ├── replay_evidence.py            离线样本/标定/性能重算
│   ├── run_rtl_tests.ps1              六项Verilog回归
│   ├── rtl/                          测试平台
│   ├── python/                       黄金参考与20项测试
│   └── results/                      原始仿真/FFT/软件数值结果
├── build/
│   ├── README.md                     构建入口
│   ├── reproduce.md                  从零复现完整命令
│   ├── fpga/                         恢复/综合实现/导出/契约检查
│   ├── linux/                        PetaLinux恢复/SDK软件编译/boot脚本
│   ├── make_sd_runtime.py            新SD包和哈希绑定启动入口
│   ├── verify_manifest.py            SHA核验
│   ├── check_submission.py           结构/冻结源码/报告检查
│   ├── package_source.py             源码压缩包生成
│   └── reports/                      综合/实现/资源/时序/CDC/DRC与旧版对照
├── board/
│   ├── bringup.md                    接线、启动、操作及验收
│   ├── measurements.md               实测与参考比对说明
│   ├── runtime_layout/               初始化/Weston/运行脚本
│   ├── reference/                    V16冻结入口与回执
│   └── results/                      大点数及频段端点实板结果
├── data/
│   ├── README.md                     原始数据索引
│   ├── comparison/                   CSV实测比对、功率读数和指标JSON
│   ├── calibration/                  原始参考、模型、实时独立验收
│   ├── rx_loop65536/                 实板原始频谱包及测量
│   ├── tuning/                       NCO调谐1300MHz实测
│   └── modulation/                   原始16QAM照片及参数
├── skill/                            完整可复用技能包
│   └── README.md
├── report/
│   ├── submission_checklist.md        3.3.5.1逐项映射
│   ├── source_completeness.md         源码完整性与依赖
│   ├── project_design.md             架构与软硬件划分
│   ├── hardware_performance.md       资源、时钟、时序与性能
│   ├── optimization_comparison.md    旧版比较口径与取舍
│   ├── status_and_scope.md           已实现与待完善
│   ├── directory_map.md              本清单
│   ├── source_identity.json          223份src文件冻结哈希
│   ├── file_inventory.csv            逐文件分类和大小
│   ├── path_conventions.md           原生固定文件名说明
│   └── validation/                   历史验证与本次检查回执
├── LICENSE
├── THIRD_PARTY_NOTICES.md
└── SHA256SUMS.txt
```

实际源码和原始证据均已放入相应目录；不包含Vivado缓存/SDK/大波形/编译二进制。正式报告作者分工、最终视频、公开仓库与完整沟通脱敏存档仍由队伍完善，见submission_checklist.md。
