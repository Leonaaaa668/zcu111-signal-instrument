# V17验证入口

本次本机测试使用C++编译器和INSTRUMENT_BUILD_GUI=OFF，本机测试记录与队伍确认的上板验证分开保存。新cw-frequency-estimate检查默认禁止隐式跨频估算、频率/功率范围、上下边缘、补偿范围及三份10 ms波形的估算元数据。

使用匹配SDK时，按build/linux/build_all.sh构建并运行全部CTest。Qt界面可在SDK QEMU环境执行：

```sh
export QT_QPA_PLATFORM=offscreen
export QT_PLUGIN_PATH="$SDKTARGETSYSROOT/usr/lib/plugins"
export XDG_RUNTIME_DIR=/tmp/zcu111-v17-qt-test
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
# 在新app构建目录运行；RES使用仓库resources的绝对路径。
RES=/absolute/repository/src/linux/apps/instrument_cpp/resources
for test in generator power markers; do
  "$OECORE_NATIVE_SYSROOT/usr/bin/qemu-aarch64" -L "$SDKTARGETSYSROOT"     ./zcu111-instrument --data "$RES" --output /tmp/zcu111-v17-ui-test     --self-test-$test --screenshot "/tmp/zcu111-v17-$test.png"
done
```

generator自测覆盖参考频点与1300/1500/1800/2000/2300 MHz的ESTIMATED状态、频率映射和编辑后上传有效性撤销。自测不连接RX/TX设备；截图为离线UI验证，不是仪器实测图。
