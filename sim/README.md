# Verification map

## V17当前源码验证

2026-11-03本机非Qt C++构建完成，7项CTest通过，见[原始测试输出](results/v17/host_ctest.log)。Windows/GCC9.5路径使用-Werror，并仅将已有Linux专用分支在Windows产生的unused-parameter/unused-function告警保留为告警；Linux SDK构建入口仍保持原有严格-Werror。
新测试生成1300/1800/2300 MHz对应的三份48 MB/10 ms I/Q文件；[独立样点检查](results/v17/generated_iq_check.json)核对数字频率、SHA256和循环边界。可用`python3 sim/python/check_v17_generated_iq.py /absolute/app/cw-frequency-estimate-evidence/run_...`复算。
V17上板验证通过按队伍确认记录；本机7项测试覆盖非Qt路径，[后续Qt验证入口](v17_validation.md)。下方日志为原V16历史基线。

当前整理新增验证的原始日志：

- report/validation/rtl_tests_02.log：6项自写Verilog仿真。
- sim/results/current/*/simulation.log：各测试独立原始输出。
- sim/results/current/snapshot_packet_verification.json：六包逐样本比对（含65536点），14种损坏/截断拒绝检查。
- report/validation/python_tests.log：20项开发电脑Python黄金参考/波形/标定约束测试。
- report/validation/linux_rebuild.log与cpp_ctest.log：V16源码在新的SDK目录编译，11项CTest。

运行方式见根README。sim/rtl中不存在自写SystemVerilog平台，均为.v；仿真中使用AMD库自带的XPM属于工具依赖。
sim/results/fft保存原AMD FFT模型数值验证记录及受测源码哈希，不是这次重新运行的FFT结果。
V16 Qt的生产实现是C++，sim/python的波形/标定工具是开发阶段参考，不在板上要求Python环境。
src/baremetal_reference/tx_command.tcl用于历史常量契约对照，含旧机路径；不是Linux控制入口，不执行它来驱动当前板卡。

全部单元、模型、仿真测试均不能代替模拟RF指标、接线、电平和新硬件时序验收。

## 频率规划与带宽交叉核对

`python3 sim/check_bandplan.py`只读冻结RFDC配置、C++生成/扫描约束及三级FIR系数，核对第二奈奎斯特区、接口速率、10 ms文件、13段覆盖与理想系数响应。
结果见[data/bandplan/offline_check.json](../data/bandplan/offline_check.json)，解释见[频率规划](../docs/bandplan_and_bandwidth.md)。这不是RF测量或新的综合实现，也不替代模拟平坦度验收。
