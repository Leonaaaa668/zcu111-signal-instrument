# Source package exclusions

保留：当前生产RTL/C++/内核与辅助程序、BD/约束、系数/时钟表、配置、工具入口、测试平台/黄金参考、少量回放样本及真实报告。
不保留：Vivado .gen/.runs/.cache/ip_user_files/.Xil、OOC输出、DCP、BIT/XSA/LTX二进制、ELF/KO/共享库、SDK/sysroot、PetaLinux build/tmp/downloads/sstate、BOOT.BIN/image.ub、大型波形、重复旧版本包。
没有把AI清理后的频谱照片当作原始测量图放入data。

production文件从冻结包逐一比较，结果见report/package_validation.json。
source_provenance.json记录最初收集的文件来源与当时哈希；参考Python工具为目录搬迁调整了少量路径，改动单独列在reference_tool_path_changes.diff；最终权威完整性清单是根SHA256SUMS.txt。
新建的构建/恢复/校验脚本和说明是源码复现集成，不是声称已经在V16板上运行过的新固件版本。
共享root密码哈希从实际meta-user镜像定制中移除，构建者应自行设置，功能配置保持原结构。未包含Windows/VM/板卡登录凭据、SSH密钥或AMD许可证。

Linux、Qt及AMD工具/库按对应PetaLinux2025.2 recipe与工具版本获取；不要把依赖外部官方工具误解成缺失自写工程源代码。
