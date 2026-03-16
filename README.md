![logo](doc/logo.png)

# aarchvm

Compact AArch64 full-system emulator written in C++ with CMake.
It currently boots U-Boot, hands off to Linux, reaches an interactive BusyBox shell, supports snapshots, and provides both serial and simple framebuffer bring-up paths.

## Features
- Interpreter-based AArch64 full-system emulation, with validated single-core Linux bring-up and a validated 2-core Linux SMP shell / functional regression path
- Linux boot path validated through U-Boot -> Linux `Image` -> BusyBox shell
- Minimal MMU/TLB, Generic Timer, synchronous exception, and GICv3 paths sufficient for the current Linux bring-up
- PL011 UART console
- QEMU-style interactive serial escape `Ctrl+A, x` to stop the emulator, still honoring end-of-run snapshot save
- SDL-backed simple framebuffer path usable by U-Boot and Linux `simpledrm` / `fbcon`
- PL050 PS/2 keyboard path usable by Linux `ambakmi` + `atkbd`
- Full-machine snapshot save / restore
- In-tree bare-metal, Linux functional, and Linux algorithm/perf regression suites
- Optional faster execution paths for bus/decode hot paths
- Explicit halt / unexpected-stop diagnostics so guest halt states are reported instead of looking like a silent hang
- Validated 2-core SMP round-robin scheduling with PSCI secondary boot, per-CPU GIC redistributors / timers, cross-core `SEV/WFE`, and exclusive-monitor invalidation tests

## Documentation
- English: [doc/README.en.md](doc/README.en.md)
- 中文: [doc/README.zh.md](doc/README.zh.md)

## Status
The project is still intentionally small and incomplete architecturally. The guarantee is the behavior covered by the in-tree bare-metal regression suite plus the current Linux / BusyBox bring-up and user-space test flows, not a full implementation of every mandatory Armv8-A feature.

## Demo
![demo](doc/demo.gif)
![demo](doc/demo2.gif)

## Star History

<a href="https://www.star-history.com/?repos=DreamChaser-luzeyu%2Faarchvm&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/image?repos=DreamChaser-luzeyu/aarchvm&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/image?repos=DreamChaser-luzeyu/aarchvm&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/image?repos=DreamChaser-luzeyu/aarchvm&type=date&legend=top-left" />
 </picture>
</a>

## License

This project is licensed under the MIT License.
