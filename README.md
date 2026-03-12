![alt text](doc/logo.png)

Vibe-coded aarch64 system emulator, written in C++.
Designed to stay compact and readable, making it suitable for learning, debugging, and experimentation.
Now capable of booting Linux into an interactive BusyBox shell.

## Features
- [x] Full-system AArch64 emulation
- [x] Boots Linux to an interactive BusyBox shell
- [x] Snapshot save and restore
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

## License

This project is licensed under the MIT License.
