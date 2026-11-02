# 路径约定与固定文件名

仓库主目录src/sim/build/board/data/skill/report与新增说明文件采用小写英文、数字、下划线或连字符；没有中文路径、空格或本机磁盘路径依赖。

指南文本提出小写英文，同时推荐树明确使用README.md。本包沿用README.md以及构建工具/Skill约定固定名CMakeLists.txt、Makefile、SKILL.md、LICENSE、SHA256SUMS.txt；第三方字体、时钟和许可文件保留原生名。Yocto的linux-xlnx_%.bbappend/u-boot-xlnx_%.bbappend是配方通配文件，百分号具有构建语义。

这些例外逐项列于path_exceptions.csv，不将它们隐藏为“全小写已经通过”。改名后需同步引用与构建入口，且第三方许可来源应可核查。其余新增材料遵守推荐命名。整个包可在大小写敏感Linux上按原生工具规则构建。
