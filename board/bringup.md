# V17构建后上板、运行与验收

> 当前生产源码为V17/0.11.0，沿用narrow09硬件；上板验证通过按队伍确认记录。V16原始日志及固定启动脚本保留为历史基线，新构建使用生成的start_v17.sh。见[V17说明](../docs/v17_release.md)。

## 1. 准备匹配文件

使用同一轮FPGA硬件、SDT/设备树、Linux内核、模块和Qt运行包。自行重建按build/reproduce.md生成；board/reference/start_v16.sh只适配原冻结文件哈希，不直接套用给新二进制。

SD第一FAT分区应有BOOT.BIN、image.ub、boot.scr、运行tar.gz及生成的start_v17.sh。SD中程序路径和文件名由生成器写入，不依赖编译机路径。

## 2. 接线与启动

首先连接电源、串口/JTAG、DP显示器、USB鼠标；首次接收检查J1/J7可留空，信号源RF OFF。上电从SD启动，登录root，执行：

```sh
sh /run/media/mmcblk0p1/start_v17.sh
```

初始化顺序：控制时钟保持 → PL复位和DDR就绪 → TX/RFDC → RX → NCO服务 → Weston用户Qt。默认不自动播放。Qt在串口前台运行时，输入的poweroff不会由shell执行；先RX STOP / RF STOP，再退出应用，出现root提示符后执行sync、poweroff。

## 3. 初始功能验收

1. 显示SA3000仪器界面，鼠标移动/点击，Spectrum与Generator可切换。
2. Spectrum选Linux RX device，FFT65536、平均1，Run Single取得有效频谱。
3. Run Cont.持续刷新，主页面M1–M4可读；点RX STOP退出连续接收。
4. M2卡片/曲线点击可改变位置，Delta参考M1能显示差值，Peak Search定位当前有效曲线最大值。
5. Generator选择CW，1790MHz；使用当前模型适用设置生成10ms文件、UPLOAD TO DDR4，等待round-trip/SHA256通过及NOT playing。

## 4. RF 验收分开接线

- **DAC输出测量：** J7→频谱仪输入，J1留空；仅在回读通过后RF PLAY，记录频率/功率，完成RF STOP。板端未接输入时的Marker不用于评价DAC频率。
- **接收验证：** 信号源→J1，J7留空；先RF OFF设频率/电平/Level Offset，确认合适输入范围后RF ON，板端采集并保存，再RF OFF。保留同根线、参考面与仪器参数。
- **自回环：** 采用已经确认合适电平的J7→J1路径；不把两个RF输出口相连。回环看链路与相对结果，不单独给出绝对功率精度。

CW功率复核按measurements.md参考条件；全频段扫描使用Span1000MHz，中心1800MHz，FFT8192、平均1，Run Single。窄带端点1300MHz可调RX center1310MHz、Span100MHz。所有结果记录当时参数，而非仅截图一个峰值。

## 5. 验收记录模板

记录软件/硬件/内核/运行包SHA、接线、仪器型号与设置、时钟/DDR/驱动状态、采集参数、原始包与SHA、期望值/实测/差值、开始/结束RF状态。复制runtime包的新运行结果到自己的结果目录，不覆盖历史参考。

V16已有在线实板及人工确认；V15冷启动历史可参考，不改记为V16新冷启动。本次材料整理未接板或重新发射。

载频与模型范围说明：1790 MHz限定的是本节CW功率参考条件。当前V17在Digital peak和CW model模式均接受1300–2300 MHz，非1790 MHz的CW model显示ESTIMATED；可编程I/Q矢量发射、波形种类与变频扩展见[能力说明](../report/vector_signal_source.md)。
