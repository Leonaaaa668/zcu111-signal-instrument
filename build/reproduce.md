# V16 从源码复现完整步骤

所有命令从仓库根目录执行；本文补充根README的完整工具命令。2026-11-02材料版沿用V16冻结源码。

## 工具和硬件

- ZCU111：xczu28dr-ffvg1517-2-e；XM500；单路ADC3及DAC Tile1/DAC0。
- Vivado2025.2，安装Zynq UltraScale+ RFSoC器件和ZCU111 board part xilinx.com:zcu111:part0:1.4；完整综合实现需有效器件许可。
- Linux编译机：PetaLinux2025.2（本机实际Ubuntu22.04）；Qt5.15.13、GCC13.4 SDK。
- 已验收板端内核：6.12.40-xilinx-g31626ef92ff1。驱动必须与目标内核精确匹配。
- Python3.10+和NumPy仅用于开发电脑的检查/参考，不要求ZCU111安装Python或PYNQ。

## 1. 解压及完整性检查

建议解压到短的纯英文路径，如Windows的C:/fpga/zcu111_v16_source或Linux的~/zcu111_v16_source。

```sh
python3 build/verify_manifest.py
```

Windows可将python3改成自己的python解释器。校验不访问板卡。
本包相对路径不依赖原作者用户名或磁盘布局；原始报告中的历史绝对路径仅作为证据保留。
README.md、CMakeLists.txt、LICENSE及Yocto的原生通配bbappend文件名按工具约定保留；无中文、空格文件路径。

## 2. 从源码恢复Vivado工程

在已经加载Vivado2025.2环境的终端执行，输出目录必须是新的目录：

```sh
vivado -mode batch -source build/fpga/create_project.tcl -tclargs C:/fpga/v16_hw_new
```

Linux将输出参数换成绝对Linux路径。然后在Vivado打开输出目录的zcu111_v16.xpr，即可查看完整ADC+DAC+PL DDR4+DMA+FFT Block Design。
此命令创建工程、恢复BD和wrapper并校验参数/地址，不运行综合实现、不连接板卡。
项目只包含自写.v/.vh；AMD自带IP的内部实现由对应版本工具生成。

需要重新生成BIT/XSA时显式执行：

```sh
vivado -mode batch -source build/fpga/build_bitstream.tcl -tclargs C:/fpga/v16_hw_new/zcu111_v16.xpr C:/fpga/v16_exports
```

该步骤耗时较长，导出fpga.bit/system.xsa/LTX及新报告。路由后仍执行原来的严格setup/hold/pulse/skew和布线完整性检查，不放宽DDR时序。
本次源码整理只验证了全新工程恢复，不重新跑一轮完整综合实现；build/reports/fpga中的WNS+0.140ns等是冻结硬件的真实历史结果。
不同主机/重跑布线不保证BIT字节哈希或时序数值完全相同，必须检查自己的新报告后再使用新BIT。

## 3. RTL与软件验证

Windows PowerShell：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\sim\run_rtl_tests.ps1 -VivadoBin 'D:/Vivado/2025.2/Vivado/bin' -OutputDir 'C:/fpga/v16_rtl_test_new'
```

该执行策略只对这次PowerShell进程生效，不修改系统永久策略。六项验证覆盖IQ拆分、TX静音/欠载、质量/背压、前端控制、双bank快照、结果控制；原始日志在新输出目录。
开发电脑Python参考：

```sh
python3 -m pip install -r sim/python/requirements.txt
python3 -m unittest discover -s sim/python -p 'test_*.py'
```

完整AMD FFT模型的历史数值验证见sim/results/fft；它不是仅靠六个控制测试替代的FFT/RF精度证明。
C++/SDK测试由下一节build_all.sh运行11项CTest。

## 4. 恢复PetaLinux配置和生成SDK

在Linux编译机source安装目录下的PetaLinux2025.2 settings.sh，然后执行：

```sh
bash build/linux/prepare_petalinux.sh /absolute/new_zcu111_project
cd /absolute/new_zcu111_project
```

这个入口采用冻结SDT和从实际工程取回的配置，保留DMA启动时disabled的设备树。若使用自己新构建的硬件，先用2025.2的SDTGen从新XSA生成SDT，再用petalinux-config --get-hw-description导入该新SDT，不混用改变过接口的硬件描述。ZCU111重新生成时带上板级设备树选择，例如：

```sh
sdtgen -xsa /absolute/new_exports/system.xsa -dir /absolute/new_sdt -board_dts zcu111-reva
```

核对生成的system-top.dts包含zcu111-reva.dtsi及所需板级配置，再导入。板级选项说明见[AMD SDTGen 2025.2官方文档](https://github.com/Xilinx/system-device-tree-xlnx/tree/xlnx_rel_v2025.2)。
修改meta-user内的静态IPv4配置以适配自己的网络。共享密码及哈希已移除：请自行设置系统用户/密码；可在编译机用openssl passwd -6交互生成自己的哈希，将INSTRUMENT_ROOT_PASSWORD_HASH写入自己项目的build/conf/local.conf，不提交该凭据。
确认src/linux/petalinux_project/reference_kernel.config中的OF_CONFIGFS、UIO、Xilinx DMA、CMA、USB、DRM/DP等配置在新内核中保留。完整.config用于比较，不是跨内核版本通用配置。

```sh
petalinux-build
petalinux-build --sdk
petalinux-package sysroot
```

按生成安装器提示安装SDK；Qt运行库、Wayland和SDK开发包已在捕获的meta-user层中选择。
打包BOOT.BIN需使用同一轮的FSBL、PMU固件、ATF、U-Boot和匹配BIT，例如在项目目录执行：

```sh
petalinux-package boot --u-boot --fpga /absolute/new_exports/fpga.bit --force
```

SD第一FAT分区需要BOOT.BIN、image.ub、boot.scr。优先采用同轮PetaLinux输出的boot.scr。若采用本包已知的内嵌ramdisk FIT启动方式，可从build/linux/boot.cmd生成：

```sh
mkimage -A arm64 -O linux -T script -C none -n zcu111_sd_fit_boot -d /absolute/source/build/linux/boot.cmd boot.scr
```

本次没有从头重新下载并重建整个PetaLinux镜像；已保存实际配置并验证配置导入，SDK/内核构建目录上的应用、模块、辅助程序重编译验证另有日志。勿将此边界写成新镜像冷启动已验收。

## 5. 从V16源码编译Qt、驱动和RFDC辅助程序

使用上一步的同版本SDK和已经完成petalinux-build的项目：

```sh
export SDK_ENV=/absolute/sdk/environment-setup-cortexa72-cortexa53-amd-linux
export PLNX_PROJECT=/absolute/new_zcu111_project
find "$PLNX_PROJECT/build/tmp/work" -name Module.symvers
export KERNEL_BUILD=/absolute/path/containing/the/linux/Module.symvers
bash build/linux/build_all.sh /absolute/new_output
```

KERNEL_BUILD必须是Linux内核本身的准备完成目录，含.config和Module.symvers，不是某个驱动模块目录。
入口会编译原生C++ Qt程序和命令行工具、运行11项CTest、编译两个内核模块、四个RFDC/控制辅助程序和设备树overlay，组装运行目录；不连接板卡。
AMD librfdc/libmetal/sysfsutils通过该PetaLinux工程的对应recipe获取和构建。若多个构建版本导致目录不唯一，显式设置RF_USR、LIBMETAL_USR、SYSFS_USR为各库已staged的usr目录。

## 6. 为新构建生成SD运行包

```sh
python3 build/make_sd_runtime.py --runtime /absolute/new_output/zcu111_instrument_qt5_20261101_16 --boot-bin /absolute/project/images/linux/BOOT.BIN --output /absolute/new_sd_files
```

生成器将新运行包和新BOOT.BIN各自的哈希写入新的start_v16.sh，不沿用冻结包的旧哈希。
把新运行tar.gz/start_v16.sh和配套BOOT.BIN/image.ub/boot.scr复制到SD第一分区。
board/reference/start_v16.sh是原冻结包的可审查参考，硬编码的哈希只适配原冻结包，不能直接用于重编译后的运行包。

启动后root串口执行：

```sh
sh /run/media/mmcblk0p1/start_v16.sh
```

脚本按“持有控制时钟→PL初始化复位与DDR就绪→TX/RFDC→RX→调谐服务→Weston用户Qt”顺序初始化。已有设备/相同GUI会被识别，避免重复复位。不自动采集、上传或RF播放。
首次只连接电源、JTAG/串口、DP显示器、USB鼠标和网线；RF接线与电平按实际硬件测量方案确认后进行。
不在Linux运行期间使用早期裸机固定RAM/JTAG命令；源代码中的裸机参考仅供算法与接口对照。

## 7. 验收与版本边界

V16人工确认：鼠标操作、Power mode显示CW model、Run Single更新频谱正常；板端自动测试包括Marker和连续接收。
接收频谱采用150 MS/s复采样窄带连续模式，65536点、平均1，V16记录82次显示/8.416秒；接收1 GHz显示采用13段NCO顺序扫描，不是1 GHz同时采集。发射采用1.2 GS/s复I/Q矢量架构；1.3–2.3 GHz工作带、插值滤波通带与各调制参数范围见[频率规划](../docs/bandplan_and_bandwidth.md)及[平台实现](../docs/platform_overview.md)。
CW产生、10ms/48MB DDR上传回读SHA校验、显式播放/停止已验收。多调制可以生成，当前C++路径的RF星座/EVM与调制平均功率验收仍未完成。
CW dBm模型实际标定范围仍为1790MHz及记录中的参考条件；全频段频谱/Marker为dBFS/bin。1800MHz小杂散根因未解决，不作隐藏。
V16本轮未重新断电做冷启动；V15历史冷启动结果另存，不能改记成V16测试。
完整工程状态与比赛材料还需补充的内容见report/status_and_scope.md。

## 许可

第一方应用代码采用已有MIT许可；内核驱动按各文件GPL-2.0-only声明，字体与AMD/其他库分别遵循随附许可证。
AMD商业工具、IP库和Linux/Qt完整源码不在此重复打包，从已列明的官方工具/recipe获取。第三方来源与许可见THIRD_PARTY_NOTICES.md。
本包不包含许可证文件、登录密码、SSH私钥或原始私密聊天记录。


## 技能包升级（2026-11-01）

本提交包是在原V16精简源码包上增加完整的skill技能包，原生产RTL、C++、驱动、构建配置与数据保持原字节。
请另读skill/README.md：六项可复用工作流、八个真实案例、六组提示词、四种模板、五类校验与30项行为测试。
技能包能独立离线演练，不要求本队板卡。旧源码ZIP保留，顶层SHA256SUMS已按本整合包重新生成。
没有重新部署板卡或改变V16功能；也没有因此补齐未完成的RF/冷启动验收。

载频与模型范围说明：1790 MHz 限定的是本节的 CW 功率标定条件。V16 在 Digital peak 模式下可设置其他载频，RF 设置范围为1300–2300 MHz；可编程I/Q矢量发射、波形种类与变频扩展见[能力说明](../report/vector_signal_source.md)。
