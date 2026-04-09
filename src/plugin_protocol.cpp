#include "aarchvm/plugin_protocol.hpp"

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace aarchvm {

namespace {

bool wait_fd_readable(int fd, int timeout_ms, std::string& error) {
  if (timeout_ms < 0) {
    return true;
  }

  pollfd pfd{};
  pfd.fd = fd;
  pfd.events = POLLIN;
  while (true) {
    const int rc = ::poll(&pfd, 1, timeout_ms);
    if (rc > 0) {
      return true;
    }
    if (rc == 0) {
      error = "timed out waiting for plugin response";
      return false;
    }
    if (errno == EINTR) {
      continue;
    }
    error = std::string("poll() failed: ") + std::strerror(errno);
    return false;
  }
}

} // namespace

void plugin_init_message(PluginIpcMessage& message,
                         PluginMessageType type,
                         std::uint32_t request_id,
                         std::uint64_t guest_now,
                         PluginMessageStatus status) {
  message = {};
  message.header.magic = kPluginIpcMagic;
  message.header.version = kPluginIpcVersion;
  message.header.type = static_cast<std::uint16_t>(type);
  message.header.request_id = request_id;
  message.header.status = static_cast<std::int32_t>(status);
  message.header.guest_now = guest_now;
}

void plugin_set_text_payload(PluginIpcMessage& message, std::string_view text) {
  const std::size_t copied = (text.size() < (kPluginTextMax - 1u)) ? text.size() : (kPluginTextMax - 1u);
  std::memset(message.payload.text.text, 0, sizeof(message.payload.text.text));
  std::memcpy(message.payload.text.text, text.data(), copied);
  message.header.payload_size = sizeof(message.payload.text);
}

std::string plugin_read_text_payload(const PluginIpcMessage& message) {
  const char* text = message.payload.text.text;
  return std::string(text, ::strnlen(text, sizeof(message.payload.text.text)));
}

bool plugin_send_message(int fd, const PluginIpcMessage& message, std::string& error) {
  while (true) {
    const ssize_t sent = ::send(fd, &message, sizeof(message), MSG_NOSIGNAL);
    if (sent == static_cast<ssize_t>(sizeof(message))) {
      return true;
    }
    if (sent < 0 && errno == EINTR) {
      continue;
    }
    if (sent < 0) {
      error = std::string("send() failed: ") + std::strerror(errno);
    } else {
      error = "short send on plugin socket";
    }
    return false;
  }
}

bool plugin_recv_message(int fd,
                         PluginIpcMessage& message,
                         int timeout_ms,
                         std::string& error) {
  if (!wait_fd_readable(fd, timeout_ms, error)) {
    return false;
  }
  while (true) {
    const ssize_t received = ::recv(fd, &message, sizeof(message), 0);
    if (received == static_cast<ssize_t>(sizeof(message))) {
      break;
    }
    if (received < 0 && errno == EINTR) {
      continue;
    }
    if (received == 0) {
      error = "plugin socket closed";
    } else if (received < 0) {
      error = std::string("recv() failed: ") + std::strerror(errno);
    } else {
      error = "short receive on plugin socket";
    }
    return false;
  }

  if (message.header.magic != kPluginIpcMagic) {
    error = "invalid plugin IPC magic";
    return false;
  }
  if (message.header.version != kPluginIpcVersion) {
    error = "unsupported plugin IPC version";
    return false;
  }
  return true;
}

const char* plugin_message_type_name(PluginMessageType type) {
  switch (type) {
    case PluginMessageType::Hello:
      return "HELLO";
    case PluginMessageType::Error:
      return "ERROR";
    case PluginMessageType::ResetReq:
      return "RESET_REQ";
    case PluginMessageType::ResetResp:
      return "RESET_RESP";
    case PluginMessageType::MmioReadReq:
      return "MMIO_READ_REQ";
    case PluginMessageType::MmioReadResp:
      return "MMIO_READ_RESP";
    case PluginMessageType::MmioWriteReq:
      return "MMIO_WRITE_REQ";
    case PluginMessageType::MmioWriteResp:
      return "MMIO_WRITE_RESP";
    case PluginMessageType::ShutdownReq:
      return "SHUTDOWN_REQ";
    case PluginMessageType::ShutdownResp:
      return "SHUTDOWN_RESP";
  }
  return "UNKNOWN";
}

} // namespace aarchvm
