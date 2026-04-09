#include "aarchvm-plugin-sdk/plugin_api.h"

#include <stdint.h>

static int32_t bad_abi_create(const struct aarchvm_plugin_host_v1* host,
                              const struct aarchvm_plugin_config_v1* config,
                              void** device_out) {
  (void)host;
  (void)config;
  if (device_out != NULL) {
    *device_out = (void*)0x1;
  }
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void bad_abi_destroy(void* device) {
  (void)device;
}

static int32_t bad_abi_reset(void* device) {
  (void)device;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t bad_abi_read(void* device,
                            uint64_t guest_now,
                            uint64_t offset,
                            uint32_t access_size,
                            uint64_t* value_out) {
  (void)device;
  (void)guest_now;
  (void)offset;
  (void)access_size;
  if (value_out != NULL) {
    *value_out = 0;
  }
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t bad_abi_write(void* device,
                             uint64_t guest_now,
                             uint64_t offset,
                             uint32_t access_size,
                             uint64_t value) {
  (void)device;
  (void)guest_now;
  (void)offset;
  (void)access_size;
  (void)value;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t bad_abi_advance(void* device, uint64_t guest_now) {
  (void)device;
  (void)guest_now;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void bad_abi_shutdown(void* device) {
  (void)device;
}

static const struct aarchvm_plugin_mmio_region_v1 kRegions[] = {
    {
        .size = sizeof(struct aarchvm_plugin_mmio_region_v1),
        .name = "bad_abi",
        .size_bytes = 0x1000u,
        .flags = 0,
        .reserved = 0,
    },
};

static const struct aarchvm_plugin_ops_v1 kOps = {
    .size = sizeof(struct aarchvm_plugin_ops_v1),
    .create = bad_abi_create,
    .destroy = bad_abi_destroy,
    .reset = bad_abi_reset,
    .mmio_read = bad_abi_read,
    .mmio_write = bad_abi_write,
    .advance_to = bad_abi_advance,
    .on_shutdown = bad_abi_shutdown,
};

static const struct aarchvm_plugin_api_v1 kApi = {
    .size = sizeof(struct aarchvm_plugin_api_v1),
    .abi_major = 99u,
    .abi_minor = 0u,
    .plugin_name = "bad_abi",
    .plugin_version = "0.0.1",
    .mmio_region_count = 1u,
    .reserved = 0u,
    .mmio_regions = kRegions,
    .ops = &kOps,
};

AARCHVM_PLUGIN_EXPORT const struct aarchvm_plugin_api_v1* aarchvm_plugin_get_api_v1(void) {
  return &kApi;
}
