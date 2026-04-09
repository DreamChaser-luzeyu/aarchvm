# aarchvm Plugin SDK

This directory is a standalone CMake project for isolated external-device plugins.

Build it independently from the main emulator:

```bash
cmake -S sdk -B build/sdk
cmake --build build/sdk -j
```

Current tree contents:
- `include/aarchvm-plugin-sdk/plugin_api.h`: stable C ABI header for plugins
- `cmake/AarchvmPlugin.cmake`: helper for `add_library(<name> MODULE ...)`
- `examples/register_bank/`: minimal MMIO-only example plugin
- `tests/abi_smoke/`: intentionally bad plugins used by host-side ABI/load tests

Current MVP scope:
- one plugin instance per child process
- runtime loading through `fork() + dlopen()`
- one declared MMIO window per plugin
- synchronous MMIO read/write only
- no IRQ routing yet
- no DMA API implementation yet
- no plugin deadline or guest-time scheduling yet
- no snapshot interoperability; the host rejects `-snapshot-load/-snapshot-save` when any plugin is active

The plugin ABI is pure C:
- exported entry: `aarchvm_plugin_get_api_v1()`
- plugin provides a read-only API descriptor plus an ops table
- host callbacks are passed through `aarchvm_plugin_host_v1`
- compatibility uses `ABI major/minor`; the current host rejects mismatched major versions

Example build artifact:

```text
build/sdk/examples/register_bank/aarchvm_register_bank.so
```

Example host invocation:

```bash
./build/aarchvm \
  -plugin build/sdk/examples/register_bank/aarchvm_register_bank.so,mmio=0x09050000,size=0x1000,name=demo \
  -bin tests/arm64/out/plugin_mmio_register_bank.bin \
  -load 0x0 \
  -entry 0x0 \
  -steps 400000
```

Current lifecycle/protocol:
1. The host parses `-plugin`.
2. The host creates a `SOCK_SEQPACKET` `socketpair()`.
3. The host `fork()`s a child process.
4. The child `dlopen()`s the plugin and resolves `aarchvm_plugin_get_api_v1()`.
5. The child sends a `HELLO` manifest to the parent.
6. The parent validates ABI version and MMIO window size, then maps an `ExternalDeviceProxy` on the bus.
7. MMIO accesses are forwarded as synchronous request/response IPC messages.
8. Shutdown is explicit; crashes or protocol errors fault the proxy and are logged by the host.
