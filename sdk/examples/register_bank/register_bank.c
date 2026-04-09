#include "aarchvm-plugin-sdk/plugin_api.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
  REG_ID = 0x00,
  REG_VERSION = 0x04,
  REG_SCRATCH0 = 0x08,
  REG_SCRATCH1 = 0x10,
  REG_SPACE_SIZE = 0x1000,
};

struct register_bank_device {
  uint64_t scratch0;
  uint64_t scratch1;
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

static int32_t register_bank_create(const struct aarchvm_plugin_host_v1* host,
                                    const struct aarchvm_plugin_config_v1* config,
                                    void** device_out) {
  (void)host;
  if (config == NULL || device_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  struct register_bank_device* device =
      (struct register_bank_device*)calloc(1u, sizeof(*device));
  if (device == NULL) {
    return AARCHVM_PLUGIN_STATUS_ERROR;
  }
  *device_out = device;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void register_bank_destroy(void* opaque) {
  free(opaque);
}

static int32_t register_bank_reset(void* opaque) {
  struct register_bank_device* device = (struct register_bank_device*)opaque;
  if (device == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  device->scratch0 = 0;
  device->scratch1 = 0;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t register_bank_mmio_read(void* opaque,
                                       uint64_t guest_now,
                                       uint64_t offset,
                                       uint32_t access_size,
                                       uint64_t* value_out) {
  (void)guest_now;
  struct register_bank_device* device = (struct register_bank_device*)opaque;
  uint8_t regs[0x18];
  if (device == NULL || value_out == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (access_size != 1u && access_size != 2u && access_size != 4u && access_size != 8u) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (offset + access_size > REG_SPACE_SIZE) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }

  memset(regs, 0, sizeof(regs));
  store_le_value(&regs[REG_ID], 4u, 0x504C5547u);      /* "PLUG" */
  store_le_value(&regs[REG_VERSION], 4u, 0x00010000u); /* 1.0 */
  store_le_value(&regs[REG_SCRATCH0], 8u, device->scratch0);
  store_le_value(&regs[REG_SCRATCH1], 8u, device->scratch1);

  if (offset + access_size <= sizeof(regs)) {
    *value_out = load_le_value(&regs[offset], access_size);
  } else {
    *value_out = 0;
  }
  return AARCHVM_PLUGIN_STATUS_OK;
}

static int32_t register_bank_mmio_write(void* opaque,
                                        uint64_t guest_now,
                                        uint64_t offset,
                                        uint32_t access_size,
                                        uint64_t value) {
  (void)guest_now;
  struct register_bank_device* device = (struct register_bank_device*)opaque;
  uint8_t scratch[8];
  uint8_t current[8];
  if (device == NULL) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (access_size != 1u && access_size != 2u && access_size != 4u && access_size != 8u) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }
  if (offset + access_size > REG_SPACE_SIZE) {
    return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
  }

  if (offset >= REG_SCRATCH0 && offset + access_size <= REG_SCRATCH0 + 8u) {
    store_le_value(current, 8u, device->scratch0);
    store_le_value(scratch, access_size, value);
    memcpy(&current[offset - REG_SCRATCH0], scratch, access_size);
    device->scratch0 = load_le_value(current, 8u);
    return AARCHVM_PLUGIN_STATUS_OK;
  }
  if (offset >= REG_SCRATCH1 && offset + access_size <= REG_SCRATCH1 + 8u) {
    store_le_value(current, 8u, device->scratch1);
    store_le_value(scratch, access_size, value);
    memcpy(&current[offset - REG_SCRATCH1], scratch, access_size);
    device->scratch1 = load_le_value(current, 8u);
    return AARCHVM_PLUGIN_STATUS_OK;
  }

  return AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT;
}

static int32_t register_bank_advance_to(void* opaque, uint64_t guest_now) {
  (void)opaque;
  (void)guest_now;
  return AARCHVM_PLUGIN_STATUS_OK;
}

static void register_bank_on_shutdown(void* opaque) {
  (void)opaque;
}

static const struct aarchvm_plugin_mmio_region_v1 kRegions[] = {
    {
        .size = sizeof(struct aarchvm_plugin_mmio_region_v1),
        .name = "register_bank",
        .size_bytes = REG_SPACE_SIZE,
        .flags = AARCHVM_PLUGIN_MMIO_REGION_FLAG_NONE,
        .reserved = 0,
    },
};

static const struct aarchvm_plugin_ops_v1 kOps = {
    .size = sizeof(struct aarchvm_plugin_ops_v1),
    .create = register_bank_create,
    .destroy = register_bank_destroy,
    .reset = register_bank_reset,
    .mmio_read = register_bank_mmio_read,
    .mmio_write = register_bank_mmio_write,
    .advance_to = register_bank_advance_to,
    .on_shutdown = register_bank_on_shutdown,
};

static const struct aarchvm_plugin_api_v1 kApi = {
    .size = sizeof(struct aarchvm_plugin_api_v1),
    .abi_major = AARCHVM_PLUGIN_ABI_MAJOR,
    .abi_minor = AARCHVM_PLUGIN_ABI_MINOR,
    .plugin_name = "register_bank",
    .plugin_version = "1.0.0",
    .mmio_region_count = (uint32_t)(sizeof(kRegions) / sizeof(kRegions[0])),
    .reserved = 0,
    .mmio_regions = kRegions,
    .ops = &kOps,
};

AARCHVM_PLUGIN_EXPORT const struct aarchvm_plugin_api_v1* aarchvm_plugin_get_api_v1(void) {
  return &kApi;
}
