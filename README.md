![alt text](doc/logo.png)

Vibe-coded aarch64 system emulator, written in C++.
Designed to stay compact and readable, making it suitable for learning, debugging, and experimentation.
Now capable of booting Linux into an interactive BusyBox shell.
This project is still in a very early stage of development. It currently only guarantees the correctness of the instructions used by Linux BusyBox. At this stage, it cannot yet ensure a complete and correct implementation of all mandatory ARM64 extensions, but it is iterating rapidly.

## Features
- [x] Full-system AArch64 emulation
- [x] Boots Linux to an interactive BusyBox shell
- [x] Snapshot save and restore
- [x] Minimal aarch64 implementation sufficient for Linux and Busybox
- [ ] Full aarch64 mandatory extensions implementation
- [ ] Basic GDB Command Support
- [ ] GDB Stub

## Docs
- English documentation: [README.en.md](doc/README.en.md)
- 中文文档： [README.zh.md](doc/README.zh.md)

## Milestone
- commit `61d52a05a3cc98f8c415a65f6b85d2abcad2cea3`
  - This commit combines simplicity with functional completeness. The codebase is small, making it relatively easy to read and learn from, while still being capable of booting Linux into a BusyBox shell.

## Demo
![alt text](doc/demo.gif)

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
