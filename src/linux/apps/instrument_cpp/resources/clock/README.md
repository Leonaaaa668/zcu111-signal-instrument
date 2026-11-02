# Clock resources

`clock_tables.h` is copied byte-for-byte from this project's frozen, previously board-tested clock table (LMK 122.88 MHz reference, LMX 2.4 GHz ADC/DAC clocks). Its SHA256 is pinned in `src/clock.cpp`. Vendor redistribution terms are retained in `vendor/LICENSE` (BSD-3-Clause).

`plan_reference.tsv` was exported on the development PC from the frozen `linux/runtime_20261029_01/python/sa_clock.py::plan_clocks()` implementation. Columns are message kind, hexadecimal message bytes (empty for delay-only steps), and delay in milliseconds. It is a static reference fixture, not a target runtime dependency. The native C++ test compares every generated step against it.

The native port's hardware programming, I2C adapter topology, PLL lock and RFDC startup still require target acceptance. Generating or comparing a plan does not program the board. No Python interpreter or Python source is needed by the target application.
