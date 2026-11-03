# Board entry and validation

本目录保留V16运行脚本源码和冻结回执，不包含可直接烧写的二进制镜像。
reference/start_v16.sh只适配原冻结运行包和原BOOT.BIN哈希；源码重编译后使用build/make_sd_runtime.py生成自己的启动入口。
runtime_layout是Linux软件构建时复制到运行目录的脚本和1080p/Pixman显示配置，不是空壳GUI或模拟板卡后端。

已有验收：V16在真实ZCU111上通过连续RX、Power/Marker页面测试；用户确认鼠标、CW model标签和Run Single。记录在reference及report/validation/v16_20261101。
本包整理没有访问板卡、重新配置RF时钟、下载、复位或启动RF。当前软件默认停播；RF PLAY必须明确操作。
新构建首次上板应先核对BOOT/内核/模块/SDT匹配和初始化顺序，再验证停播、单次采集、连续刷新、上传校验、播放/停止。

接收精确功率标定、发射标定及最大输入范围需沿用各自记录的频率/线缆/参考面，换接线后不直接承诺原精度。
停止和关机：RX STOP、RF STOP、信号源RF OFF；退出前台Qt返回root提示符后sync、poweroff，待系统关机完成再断电。

## 独立Linux仪器与工作带说明

运行栈为PetaLinux/Linux + 设备驱动 + Weston/Wayland桌面会话 + 原生C++17/Qt5 Widgets，采用1080p/Pixman配置；启动脚本和显示配置均在runtime_layout内。详见[平台技术栈及源码对应](../docs/platform_overview.md)。
1.3–2.3 GHz是1 GHz宽中频工作带；RX通过150 MS/s复采样与NCO分段覆盖，TX为宽带矢量I/Q架构，通带和实测边界详见[频率规划](../docs/bandplan_and_bandwidth.md)。不要将扫描覆盖写成同时捕获，或将1790 MHz的CW标定条件写成唯一输出频点。
