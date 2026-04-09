#include "aarchvm-plugin-sdk/plugin_api.h"

#include <stdint.h>
#include <stdlib.h>

enum {
  REG_SCRATCH0 = 0x00,
  REG_SPACE_SIZE = 0x1000,
};

struct reset_observable_device {
  uint64_t scratch0;
};

static uint64_t load_le_value(const uint8_t* src, uint32_t size) {
  uint64_t value = 0;
  for (uint32_t i = 0; i < size; ++i) {
    value |= ((uint64_t)src[i]) << (8u * i);
  }
  return value;
}

static void store_le_value(uint8_t* dst, uint32_t size, uint64_t value) {
  for (uint32_t i = 0; i < size; ++i) {
    dst[i] = (uint8_t)((value >> (8u * i)) & 0xFFu);
  }
}

static int32_t reset_observable_create(const struct aarchvm_plugin_host_v1* host,
                                       const struct aarchvm_plugin_config_v1* config,
                                       void** device_out) {
  (void)host;
  if (config == NULL || device_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }

  struct reset_observable_device* device =
      (struct reset_observable_device*)calloc(1u, sizeof(*device));
  if (device == NULL) {
    return AARCHVM_PLUGIN_STATUS_ERROR;
  }

  device->scratch0 = 0x1122334455667788ull;
  *device_out = device;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void reset_observable_destroy(void* opaque) {
  free(opaque);
}

static int32_t reset_observable_reset(void* opaque) {
  struct reset_observable_device* device = (struct reset_observable_device*)opaque;
  if (device == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }

  device->scratch0 = 0;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t reset_observable_mmio_read(void* opaque,
                                          uint64_t guest_now,
                                          uint64_t offset,
                                          uint32_t access_size,
                                          uint64_t* value_out) {
  (void)guest_now;
  struct reset_observable_device* device = (struct reset_observable_device*)opaque;
  uint8_t scratch[8];
  if (device == NULL || value_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (access_size != 1u && access_size != 2u && access_size != 4u && access_size != 8u) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (offset + access_size > REG_SPACE_SIZE) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (offset < REG_SCRATCH0 || offset + access_size > REG_SCRATCH0 + 8u) {
    *value_out = 0;
    return AARCHVM_PLUGIN_STATUS_OK;
  }

  store_le_value(scratch, 8u, device->scratch0);
  *value_out = load_le_value(&scratch[offset - REG_SCRATCH0], access_size);
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t reset_observable_mmio_write(void* opaque,
                                           uint64_t guest_now,
                                           uint64_t offset,
                                           uint32_t access_size,
                                           uint64_t value) {
  (void)opaque;
  (void)guest_now;
  (void)offset;
  (void)access_size;
  (void)value;
  return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
}

static const struct aarchvm_plugin_mmio_region_v1 kRegions[] = {
    {
        .size = sizeof(struct aarchvm_plugin_mmio_region_v1),
        .name = "reset_observable",
        .size_bytes = REG_SPACE_SIZE,
        .flags = AARCHVM_PLUGIN_MMIO_REGION_FLAG_NONE,
        .reserved = 0,
    },
};

static const struct aarchvm_plugin_ops_v1 kOps = {
    .size = sizeof(struct aarchvm_plugin_ops_v1),
    .create = reset_observable_create,
    .destroy = reset_observable_destroy,
    .reset = reset_observable_reset,
    .mmio_read = reset_observable_mmio_read,
    .mmio_write = reset_observable_mmio_write,
    .advance_to = 0,
    .on_shutdown = 0,
};

static const struct aarchvm_plugin_api_v1 kApi = {
    .size = sizeof(struct aarchvm_plugin_api_v1),
    .abi_major = AARCHVM_PLUGIN_ABI_MAJOR,
    .abi_minor = AARCHVM_PLUGIN_ABI_MINOR,
    .plugin_name = "reset_observable",
    .plugin_version = "1.0.0",
    .mmio_region_count = (uint32_t)(sizeof(kRegions) / sizeof(kRegions[0])),
    .reserved = 0,
    .mmio_regions = kRegions,
    .ops = &kOps,
};

AARCHVM_PLUGIN_EXPORT const struct aarchvm_plugin_api_v1* aarchvm_plugin_get_api_v1(void) {
  return &kApi;
}
