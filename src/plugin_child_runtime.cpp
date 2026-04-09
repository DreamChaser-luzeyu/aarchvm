#include "aarchvm/plugin_child_runtime.hpp"

#include "aarchvm/plugin_protocol.hpp"

#include "aarchvm-plugin-sdk/plugin_api.h"

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <unistd.h>

namespace aarchvm {

namespace {

struct ChildHostContext {
  std::string instance_name;
};

void copy_string(char* dst, std::size_t dst_size, const char* src) {
  if (dst_size == 0u) {
    return;
  }
  std::memset(dst, 0, dst_size);
  if (src == nullptr) {
    return;
  }
  const std::size_t copied = std::min<std::size_t>(std::strlen(src), dst_size - 1u);
  std::memcpy(dst, src, copied);
}

PluginMessageStatus plugin_status_from_api(int status) {
  switch (status) {
    case AARCHVM_PLUGIN_STATUS_OK:
      return PluginMessageStatus::Ok;
    case AARCHVM_PLUGIN_STATUS_UNSUPPORTED:
      return PluginMessageStatus::Unsupported;
    case AARCHVM_PLUGIN_STATUS_INVALID_ARGUMENT:
      return PluginMessageStatus::Invalid;
    default:
      return PluginMessageStatus::Error;
  }
}

bool send_error_message(int fd, const std::string& text) {
  PluginIpcMessage message{};
  std::string error;
  plugin_init_message(message, PluginMessageType::Error, 0u, 0u, PluginMessageStatus::Error);
  plugin_set_text_payload(message, text);
  return plugin_send_message(fd, message, error);
}

int32_t host_dma_read(void* user,
                      std::uint64_t guest_now,
                      std::uint64_t guest_pa,
                      void* dst,
                      std::size_t size) {
  (void)user;
  (void)guest_now;
  (void)guest_pa;
  (void)dst;
  (void)size;
  return AARCHVM_PLUGIN_STATUS_UNSUPPORTED;
}

int32_t host_dma_write(void* user,
                       std::uint64_t guest_now,
                       std::uint64_t guest_pa,
                       const void* src,
                       std::size_t size) {
  (void)user;
  (void)guest_now;
  (void)guest_pa;
  (void)src;
  (void)size;
  return AARCHVM_PLUGIN_STATUS_UNSUPPORTED;
}

int32_t host_irq_set(void* user, std::uint64_t guest_now, std::uint32_t line, std::uint32_t level) {
  (void)user;
  (void)guest_now;
  (void)line;
  (void)level;
  return AARCHVM_PLUGIN_STATUS_UNSUPPORTED;
}

int32_t host_irq_pulse(void* user, std::uint64_t guest_now, std::uint32_t line) {
  (void)user;
  (void)guest_now;
  (void)line;
  return AARCHVM_PLUGIN_STATUS_UNSUPPORTED;
}

int32_t host_set_deadline(void* user, std::uint64_t guest_deadline) {
  (void)user;
  (void)guest_deadline;
  return AARCHVM_PLUGIN_STATUS_UNSUPPORTED;
}

std::uint64_t host_get_guest_now(void* user) {
  (void)user;
  return 0;
}

void host_log(void* user, std::uint32_t level, const char* message) {
  const ChildHostContext* ctx = static_cast<const ChildHostContext*>(user);
  std::cerr << "PLUGIN-CHILD";
  if (ctx != nullptr && !ctx->instance_name.empty()) {
    std::cerr << " instance=" << ctx->instance_name;
  }
  std::cerr << " level=" << level << ' ';
  if (message != nullptr) {
    std::cerr << message;
  }
  std::cerr << '\n';
}

void host_abort_device(void* user, const char* message) {
  host_log(user, 0u, message);
}

bool validate_api(const aarchvm_plugin_api_v1* api, std::string& error) {
  if (api == nullptr) {
    error = "plugin returned a null API descriptor";
    return false;
  }
  if (api->size < sizeof(aarchvm_plugin_api_v1)) {
    error = "plugin API descriptor is too small";
    return false;
  }
  if (api->ops == nullptr || api->ops->size < sizeof(aarchvm_plugin_ops_v1)) {
    error = "plugin ops table is missing or truncated";
    return false;
  }
  if (api->ops->create == nullptr || api->ops->destroy == nullptr || api->ops->reset == nullptr ||
      api->ops->mmio_read == nullptr || api->ops->mmio_write == nullptr) {
    error = "plugin ops table is missing required entry points";
    return false;
  }
  if (api->mmio_region_count != 1u || api->mmio_regions == nullptr) {
    error = "MVP only supports exactly one declared MMIO region";
    return false;
  }
  if (api->mmio_regions[0].size < sizeof(aarchvm_plugin_mmio_region_v1) ||
      api->mmio_regions[0].size_bytes == 0u) {
    error = "plugin declared an invalid MMIO region";
    return false;
  }
  return true;
}

} // namespace

int run_plugin_child(int fd, const ExternalPluginConfig& config) {
  void* handle = nullptr;
  void* device = nullptr;
  const aarchvm_plugin_api_v1* api = nullptr;
  std::string error;

  handle = ::dlopen(config.shared_object_path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const std::string message = std::string("dlopen failed: ") + ::dlerror();
    std::cerr << "PLUGIN-CHILD-ERROR " << message << '\n';
    send_error_message(fd, message);
    return 1;
  }

  using GetApiFn = const aarchvm_plugin_api_v1* (*)();
  ::dlerror();
  const void* symbol = ::dlsym(handle, "aarchvm_plugin_get_api_v1");
  if (symbol == nullptr) {
    const std::string message =
        std::string("dlsym(aarchvm_plugin_get_api_v1) failed: ") + ::dlerror();
    std::cerr << "PLUGIN-CHILD-ERROR " << message << '\n';
    send_error_message(fd, message);
    ::dlclose(handle);
    return 1;
  }

  api = reinterpret_cast<GetApiFn>(const_cast<void*>(symbol))();
  if (!validate_api(api, error)) {
    std::cerr << "PLUGIN-CHILD-ERROR " << error << '\n';
    send_error_message(fd, error);
    ::dlclose(handle);
    return 1;
  }

  ChildHostContext host_ctx{.instance_name = config.instance_name};
  const aarchvm_plugin_host_v1 host = {
      .size = sizeof(aarchvm_plugin_host_v1),
      .user = &host_ctx,
      .dma_read = host_dma_read,
      .dma_write = host_dma_write,
      .irq_set = host_irq_set,
      .irq_pulse = host_irq_pulse,
      .set_deadline = host_set_deadline,
      .get_guest_now = host_get_guest_now,
      .log = host_log,
      .abort_device = host_abort_device,
  };
  const aarchvm_plugin_config_v1 plugin_config = {
      .size = sizeof(aarchvm_plugin_config_v1),
      .instance_name = config.instance_name.empty() ? nullptr : config.instance_name.c_str(),
      .opaque_arg = config.opaque_arg.empty() ? nullptr : config.opaque_arg.c_str(),
      .mmio_base = config.mmio_base,
      .mmio_size = config.mmio_size,
  };
  if (api->ops->create(&host, &plugin_config, &device) != AARCHVM_PLUGIN_STATUS_OK || device == nullptr) {
    std::cerr << "PLUGIN-CHILD-ERROR plugin create() failed\n";
    send_error_message(fd, "plugin create() failed");
    ::dlclose(handle);
    return 1;
  }

  PluginIpcMessage hello{};
  std::string transport_error;
  plugin_init_message(hello, PluginMessageType::Hello, 0u, 0u, PluginMessageStatus::Ok);
  hello.header.payload_size = sizeof(hello.payload.hello);
  hello.payload.hello.abi_major = api->abi_major;
  hello.payload.hello.abi_minor = api->abi_minor;
  hello.payload.hello.mmio_size = api->mmio_regions[0].size_bytes;
  hello.payload.hello.mmio_flags = api->mmio_regions[0].flags;
  copy_string(hello.payload.hello.plugin_name,
              sizeof(hello.payload.hello.plugin_name),
              api->plugin_name);
  copy_string(hello.payload.hello.plugin_version,
              sizeof(hello.payload.hello.plugin_version),
              api->plugin_version);
  if (!plugin_send_message(fd, hello, transport_error)) {
    std::cerr << "PLUGIN-CHILD-ERROR failed to send HELLO: " << transport_error << '\n';
    api->ops->destroy(device);
    ::dlclose(handle);
    return 1;
  }

  while (true) {
    PluginIpcMessage request{};
    if (!plugin_recv_message(fd, request, -1, transport_error)) {
      break;
    }

    const PluginMessageType request_type = static_cast<PluginMessageType>(request.header.type);
    if ((request_type == PluginMessageType::MmioReadReq ||
         request_type == PluginMessageType::MmioWriteReq) &&
        api->ops->advance_to != nullptr) {
      const int status = api->ops->advance_to(device, request.header.guest_now);
      if (status != AARCHVM_PLUGIN_STATUS_OK) {
        send_error_message(fd, "plugin advance_to() failed");
        break;
      }
    }

    PluginIpcMessage response{};
    switch (request_type) {
      case PluginMessageType::ResetReq: {
        const int status = api->ops->reset(device);
        plugin_init_message(response,
                            PluginMessageType::ResetResp,
                            request.header.request_id,
                            request.header.guest_now,
                            plugin_status_from_api(status));
        response.header.payload_size = 0u;
        if (!plugin_send_message(fd, response, transport_error)) {
          break;
        }
        continue;
      }
      case PluginMessageType::MmioReadReq: {
        std::uint64_t value = 0;
        const int status = api->ops->mmio_read(device,
                                               request.header.guest_now,
                                               request.payload.mmio.offset,
                                               request.payload.mmio.access_size,
                                               &value);
        plugin_init_message(response,
                            PluginMessageType::MmioReadResp,
                            request.header.request_id,
                            request.header.guest_now,
                            plugin_status_from_api(status));
        response.header.payload_size = sizeof(response.payload.mmio);
        response.payload.mmio.offset = request.payload.mmio.offset;
        response.payload.mmio.access_size = request.payload.mmio.access_size;
        response.payload.mmio.value = value;
        if (!plugin_send_message(fd, response, transport_error)) {
          break;
        }
        continue;
      }
      case PluginMessageType::MmioWriteReq: {
        const int status = api->ops->mmio_write(device,
                                                request.header.guest_now,
                                                request.payload.mmio.offset,
                                                request.payload.mmio.access_size,
                                                request.payload.mmio.value);
        plugin_init_message(response,
                            PluginMessageType::MmioWriteResp,
                            request.header.request_id,
                            request.header.guest_now,
                            plugin_status_from_api(status));
        response.header.payload_size = 0u;
        if (!plugin_send_message(fd, response, transport_error)) {
          break;
        }
        continue;
      }
      case PluginMessageType::ShutdownReq: {
        if (api->ops->on_shutdown != nullptr) {
          api->ops->on_shutdown(device);
        }
        plugin_init_message(response,
                            PluginMessageType::ShutdownResp,
                            request.header.request_id,
                            request.header.guest_now,
                            PluginMessageStatus::Ok);
        response.header.payload_size = 0u;
        plugin_send_message(fd, response, transport_error);
        api->ops->destroy(device);
        ::dlclose(handle);
        return 0;
      }
      default:
        error = std::string("unexpected request type: ") + plugin_message_type_name(request_type);
        std::cerr << "PLUGIN-CHILD-ERROR " << error << '\n';
        send_error_message(fd, error);
        api->ops->destroy(device);
        ::dlclose(handle);
        return 1;
    }
    break;
  }

  if (device != nullptr) {
    api->ops->destroy(device);
  }
  if (handle != nullptr) {
    ::dlclose(handle);
  }
  return 0;
}

} // namespace aarchvm
