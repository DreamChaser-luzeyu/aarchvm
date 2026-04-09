#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace aarchvm {

inline constexpr std::uint32_t kPluginIpcMagic = 0x41504C47u; // "APLG"
inline constexpr std::uint16_t kPluginIpcVersion = 1u;
inline constexpr std::size_t kPluginNameMax = 80u;
inline constexpr std::size_t kPluginVersionMax = 32u;
inline constexpr std::size_t kPluginTextMax = 192u;

enum class PluginMessageType : std::uint16_t {
  Hello = 1,
  Error = 2,
  ResetReq = 3,
  ResetResp = 4,
  MmioReadReq = 5,
  MmioReadResp = 6,
  MmioWriteReq = 7,
  MmioWriteResp = 8,
  ShutdownReq = 9,
  ShutdownResp = 10,
};

enum class PluginMessageStatus : std::int32_t {
  Ok = 0,
  Error = 1,
  Unsupported = 2,
  Invalid = 3,
};

struct PluginIpcHeader {
  std::uint32_t magic = kPluginIpcMagic;
  std::uint16_t version = kPluginIpcVersion;
  std::uint16_t type = 0;
  std::uint32_t payload_size = 0;
  std::uint32_t request_id = 0;
  std::int32_t status = 0;
  std::uint32_t reserved = 0;
  std::uint64_t guest_now = 0;
};

struct PluginHelloPayload {
  std::uint32_t abi_major = 0;
  std::uint32_t abi_minor = 0;
  std::uint64_t mmio_size = 0;
  std::uint32_t mmio_flags = 0;
  std::uint32_t reserved = 0;
  char plugin_name[kPluginNameMax] = {};
  char plugin_version[kPluginVersionMax] = {};
};

struct PluginMmioPayload {
  std::uint64_t offset = 0;
  std::uint64_t value = 0;
  std::uint32_t access_size = 0;
  std::uint32_t reserved = 0;
};

struct PluginTextPayload {
  char text[kPluginTextMax] = {};
};

struct PluginIpcMessage {
  PluginIpcHeader header{};
  union {
    PluginHelloPayload hello;
    PluginMmioPayload mmio;
    PluginTextPayload text;
  } payload{};
};

void plugin_init_message(PluginIpcMessage& message,
                         PluginMessageType type,
                         std::uint32_t request_id,
                         std::uint64_t guest_now,
                         PluginMessageStatus status);
void plugin_set_text_payload(PluginIpcMessage& message, std::string_view text);
std::string plugin_read_text_payload(const PluginIpcMessage& message);

bool plugin_send_message(int fd, const PluginIpcMessage& message, std::string& error);
bool plugin_recv_message(int fd,
                         PluginIpcMessage& message,
                         int timeout_ms,
                         std::string& error);

const char* plugin_message_type_name(PluginMessageType type);

} // namespace aarchvm
