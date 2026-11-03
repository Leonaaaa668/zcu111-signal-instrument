# 构建入口

> 当前生产源码为V17/0.11.0，沿用narrow09硬件；上板验证通过按队伍确认记录。V16原始日志及固定启动脚本保留为历史基线，新构建使用生成的start_v17.sh。见[V17说明](../docs/v17_release.md)。

所有命令在仓库根目录执行，使用新输出目录；完整环境、变量和步骤见reproduce.md。

| 顺序 | 入口 | 结果 |
|---|---|---|
| 1 | python3 build/verify_manifest.py | 全包SHA256核对 |
| 2 | python3 build/check_submission.py | 七目录、源码身份和关键报告检查 |
| 3 | vivado -mode batch -source build/fpga/create_project.tcl -tclargs NEW_DIR | XPR/BD/wrapper和契约检查 |
| 4 | vivado -mode batch -source build/fpga/build_bitstream.tcl -tclargs XPR NEW_EXPORTS | synth/impl、BIT/XSA/LTX及报告 |
| 5 | bash build/linux/prepare_petalinux.sh NEW_PROJECT | PetaLinux配置恢复 |
| 6 | petalinux-build；petalinux-build --sdk | Linux和SDK；BOOT打包见reproduce.md |
| 7 | bash build/linux/build_all.sh NEW_OUTPUT | Qt/CLI、11项CTest、模块、RFDC程序和overlay |
| 8 | python3 build/make_sd_runtime.py --runtime RUNTIME --boot-bin BOOT.BIN --output NEW_SD_DIR | 新运行包和绑定新哈希的启动脚本 |
| 9 | python3 build/package_source.py --output OUTSIDE_ROOT.zip | 源码ZIP、SHA清单与回执 |

硬件构建要求Vivado2025.2+器件许可。Linux软件要求同版本SDK、SDK_ENV、PLNX_PROJECT、准备完成的KERNEL_BUILD。外部库由保存的recipe获取，详见第三方来源。

reports/fpga中是narrow09真实历史报告。synthesis补31份顶层/分模块综合资源报告；顶层为OOC壳，不以其零资源代表整机。整机资源取utilization.rpt。implementation_run.log含成功生成bitstream的记录。reports/comparison为旧版本证据，比较边界见report/optimization_comparison.md。

本次补齐报告和索引，不新跑完整综合实现、不接板、不构建完整新镜像。所有重建产物应查新报告及做对应验收。
