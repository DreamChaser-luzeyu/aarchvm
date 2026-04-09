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
- `tests/runtime_reset/`: runtime regression plugin used to verify startup/reset wiring

Recommended in-tree workflow:
1. Add a new directory such as `sdk/examples/my_device/`.
2. Create `sdk/examples/my_device/CMakeLists.txt`.
3. Add `aarchvm_add_plugin(aarchvm_my_device my_device.c)`.
4. Add `add_subdirectory(examples/my_device)` to `sdk/CMakeLists.txt`.
5. Build with `cmake -S sdk -B build/sdk && cmake --build build/sdk -j`.

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

Current host-side callback reality:
- `log` is usable now and is the recommended way to emit plugin-side diagnostics
- `dma_read`, `dma_write`, `irq_set`, `irq_pulse`, and `set_deadline` are present in the ABI but currently return `AARCHVM_PLUGIN_STATUS_UNSUPPORTED` in the in-tree host
- the ABI carries `guest_now`, but the current MMIO-only host path still drives `mmio_read`/`mmio_write` with `guest_now == 0`, and `get_guest_now()` currently also returns `0`; treat all time-related fields as placeholders until the IRQ/deadline/guest-time stage lands

Minimal `CMakeLists.txt` for an in-tree plugin:

```cmake
aarchvm_add_plugin(aarchvm_my_device my_device.c)
```

Minimal plugin contract:
- include `aarchvm-plugin-sdk/plugin_api.h`
- provide a `create` function that stores the host callback table and plugin config if needed
- provide `destroy`, `reset`, `mmio_read`, and `mmio_write`
- describe exactly one MMIO window in `aarchvm_plugin_mmio_region_v1`
- export `aarchvm_plugin_get_api_v1()`

Minimal source skeleton:

```c
#include "aarchvm-plugin-sdk/plugin_api.h"
#include <stdlib.h>

static int32_t my_create(const struct aarchvm_plugin_host_v1* host,
                         const struct aarchvm_plugin_config_v1* config,
                         void** device_out) {
  (void)host;
  (void)config;
  if (device_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  *device_out = calloc(1, 1);
  if (*device_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_ERROR;
  }
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void my_destroy(void* device) { free(device); }
static int32_t my_reset(void* device) { (void)device; return AARCHVM_PLUGIN_STATUS_OK; }
static int32_t my_mmio_read(void* device, uint64_t guest_now,
                            uint64_t offset, uint32_t access_size, uint64_t* value_out) {
  (void)device; (void)guest_now; (void)offset; (void)access_size;
  *value_out = 0;
  return AARCHVM_PLUGIN_STATUS_OK;
}
static int32_t my_mmio_write(void* device, uint64_t guest_now,
                             uint64_t offset, uint32_t access_size, uint64_t value) {
  (void)device; (void)guest_now; (void)offset; (void)access_size; (void)value;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static const struct aarchvm_plugin_mmio_region_v1 kRegions[] = {{
    .size = sizeof(struct aarchvm_plugin_mmio_region_v1),
    .name = "my_device",
    .size_bytes = 0x1000,
    .flags = AARCHVM_PLUGIN_MMIO_REGION_FLAG_NONE,
    .reserved = 0,
}};

static const struct aarchvm_plugin_ops_v1 kOps = {
    .size = sizeof(struct aarchvm_plugin_ops_v1),
    .create = my_create,
    .destroy = my_destroy,
    .reset = my_reset,
    .mmio_read = my_mmio_read,
    .mmio_write = my_mmio_write,
    .advance_to = 0,
    .on_shutdown = 0,
};

static const struct aarchvm_plugin_api_v1 kApi = {
    .size = sizeof(struct aarchvm_plugin_api_v1),
    .abi_major = AARCHVM_PLUGIN_ABI_MAJOR,
    .abi_minor = AARCHVM_PLUGIN_ABI_MINOR,
    .plugin_name = "my_device",
    .plugin_version = "0.1.0",
    .mmio_region_count = 1,
    .reserved = 0,
    .mmio_regions = kRegions,
    .ops = &kOps,
};

AARCHVM_PLUGIN_EXPORT const struct aarchvm_plugin_api_v1* aarchvm_plugin_get_api_v1(void) {
  return &kApi;
}
```

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

`-plugin` spec notes:
- `mmio=` and `size=` are required
- `size=` must match the plugin-declared MMIO size
- `name=` is optional; default is derived from the `.so` filename
- `arg=` is optional opaque text passed to `aarchvm_plugin_config_v1.opaque_arg`
- if `arg=` contains commas, place it last in the spec
- the MMIO window must not overlap existing built-in device mappings
- multiple `-plugin` options are allowed; each creates a separate child process

Current lifecycle/protocol:
1. The host parses `-plugin`.
2. The host creates a `SOCK_SEQPACKET` `socketpair()`.
3. The host `fork()`s a child process.
4. The child `dlopen()`s the plugin and resolves `aarchvm_plugin_get_api_v1()`.
5. The child sends a `HELLO` manifest to the parent.
6. The parent validates ABI version and MMIO window size, then maps an `ExternalDeviceProxy` on the bus.
7. MMIO accesses are forwarded as synchronous request/response IPC messages.
8. Shutdown is explicit; crashes or protocol errors fault the proxy and are logged by the host.

Current host rejection cases worth knowing while developing plugins:
- missing `aarchvm_plugin_get_api_v1()`
- ABI major mismatch
- invalid or zero-sized MMIO manifest
- more than one MMIO window in the current MVP
- configured `size=` not matching the plugin manifest
- `irq=` requested on the command line
- any active plugin combined with `-snapshot-load` or `-snapshot-save`
