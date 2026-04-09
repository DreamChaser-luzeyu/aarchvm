#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AARCHVM_PLUGIN_ABI_MAJOR 1u
#define AARCHVM_PLUGIN_ABI_MINOR 0u

#if defined(_WIN32)
#define AARCHVM_PLUGIN_EXPORT __declspec(dllexport)
#else
#define AARCHVM_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

enum aarchvm_plugin_status_v1 {
  AARCHVM_PLUGIN_STATUS_OK = 0,
  AARCHVM_PLUGIN_STATUS_ERROR = 1,
  AARCHVM_PLUGIN_STATUS_UNSUPPORTED = 2,
  AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT = 3,
};

enum aarchvm_plugin_mmio_region_flags_v1 {
  AARCHVM_PLUGIN_MMIO_REGION_FLAG_NONE = 0u,
};

struct aarchvm_plugin_host_v1;
struct aarchvm_plugin_config_v1;
struct aarchvm_plugin_mmio_region_v1;
struct aarchvm_plugin_ops_v1;
struct aarchvm_plugin_api_v1;

typedef int32_t (*aarchvm_plugin_dma_read_fn)(void* user,
                                              uint64_t guest_now,
                                              uint64_t guest_pa,
                                              void* dst,
                                              size_t size);
typedef int32_t (*aarchvm_plugin_dma_write_fn)(void* user,
                                               uint64_t guest_now,
                                               uint64_t guest_pa,
                                               const void* src,
                                               size_t size);
typedef int32_t (*aarchvm_plugin_irq_set_fn)(void* user,
                                             uint64_t guest_now,
                                             uint32_t line,
                                             uint32_t level);
typedef int32_t (*aarchvm_plugin_irq_pulse_fn)(void* user,
                                               uint64_t guest_now,
                                               uint32_t line);
typedef int32_t (*aarchvm_plugin_set_deadline_fn)(void* user, uint64_t guest_deadline);
typedef uint64_t (*aarchvm_plugin_get_guest_now_fn)(void* user);
typedef void (*aarchvm_plugin_log_fn)(void* user, uint32_t level, const char* message);
typedef void (*aarchvm_plugin_abort_device_fn)(void* user, const char* message);

struct aarchvm_plugin_host_v1 {
  uint32_t size;
  void* user;
  aarchvm_plugin_dma_read_fn dma_read;
  aarchvm_plugin_dma_write_fn dma_write;
  aarchvm_plugin_irq_set_fn irq_set;
  aarchvm_plugin_irq_pulse_fn irq_pulse;
  aarchvm_plugin_set_deadline_fn set_deadline;
  aarchvm_plugin_get_guest_now_fn get_guest_now;
  aarchvm_plugin_log_fn log;
  aarchvm_plugin_abort_device_fn abort_device;
};

struct aarchvm_plugin_config_v1 {
  uint32_t size;
  const char* instance_name;
  const char* opaque_arg;
  uint64_t mmio_base;
  uint64_t mmio_size;
};

struct aarchvm_plugin_mmio_region_v1 {
  uint32_t size;
  const char* name;
  uint64_t size_bytes;
  uint32_t flags;
  uint32_t reserved;
};

typedef int32_t (*aarchvm_plugin_create_fn)(const struct aarchvm_plugin_host_v1* host,
                                            const struct aarchvm_plugin_config_v1* config,
                                            void** device_out);
typedef void (*aarchvm_plugin_destroy_fn)(void* device);
typedef int32_t (*aarchvm_plugin_reset_fn)(void* device);
typedef int32_t (*aarchvm_plugin_mmio_read_fn)(void* device,
                                               uint64_t guest_now,
                                               uint64_t offset,
                                               uint32_t access_size,
                                               uint64_t* value_out);
typedef int32_t (*aarchvm_plugin_mmio_write_fn)(void* device,
                                                uint64_t guest_now,
                                                uint64_t offset,
                                                uint32_t access_size,
                                                uint64_t value);
typedef int32_t (*aarchvm_plugin_advance_to_fn)(void* device, uint64_t guest_now);
typedef void (*aarchvm_plugin_on_shutdown_fn)(void* device);

struct aarchvm_plugin_ops_v1 {
  uint32_t size;
  aarchvm_plugin_create_fn create;
  aarchvm_plugin_destroy_fn destroy;
  aarchvm_plugin_reset_fn reset;
  aarchvm_plugin_mmio_read_fn mmio_read;
  aarchvm_plugin_mmio_write_fn mmio_write;
  aarchvm_plugin_advance_to_fn advance_to;
  aarchvm_plugin_on_shutdown_fn on_shutdown;
};

struct aarchvm_plugin_api_v1 {
  uint32_t size;
  uint32_t abi_major;
  uint32_t abi_minor;
  const char* plugin_name;
  const char* plugin_version;
  uint32_t mmio_region_count;
  uint32_t reserved;
  const struct aarchvm_plugin_mmio_region_v1* mmio_regions;
  const struct aarchvm_plugin_ops_v1* ops;
};

AARCHVM_PLUGIN_EXPORT const struct aarchvm_plugin_api_v1* aarchvm_plugin_get_api_v1(void);

#ifdef __cplusplus
}
#endif
