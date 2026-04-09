#include "aarchvm/external_device_proxy.hpp"

#include "aarchvm/plugin_child_runtime.hpp"
#include "aarchvm/plugin_protocol.hpp"

#include "aarchvm-plugin-sdk/plugin_api.h"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace aarchvm {

namespace {

constexpr int kHandshakeTimeoutMs = 2000;
constexpr int kRequestTimeoutMs = 2000;
constexpr int kShutdownTimeoutMs = 250;

std::string status_message(const PluginIpcMessage& message) {
  if (static_cast<PluginMessageType>(message.header.type) == PluginMessageType::Error) {
    return plugin_read_text_payload(message);
  }
  return std::string("plugin returned status ") + std::to_string(message.header.status);
}

bool wait_for_child_exit(int child_pid, int timeout_ms) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    int status = 0;
    const pid_t rc = ::waitpid(child_pid, &status, WNOHANG);
    if (rc == child_pid) {
      return true;
    }
    if (rc < 0 && errno == ECHILD) {
      return true;
    }
    ::usleep(1000u);
  }
  return false;
}

} // namespace

ExternalDeviceProxy::ExternalDeviceProxy(ExternalPluginConfig config,
                                         int socket_fd,
                                         int child_pid,
                                         std::string plugin_name,
                                         std::string plugin_version,
                                         std::uint64_t declared_mmio_size,
                                         FaultHandler fault_handler)
    : config_(std::move(config)),
      socket_fd_(socket_fd),
      child_pid_(child_pid),
      plugin_name_(std::move(plugin_name)),
      plugin_version_(std::move(plugin_version)),
      declared_mmio_size_(declared_mmio_size),
      fault_handler_(std::move(fault_handler)) {}

std::shared_ptr<ExternalDeviceProxy> ExternalDeviceProxy::spawn(const ExternalPluginConfig& config,
                                                                FaultHandler fault_handler,
                                                                std::string& error) {
  int sockets[2] = {-1, -1};
  if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
    error = std::string("socketpair() failed: ") + std::strerror(errno);
    return nullptr;
  }

  const pid_t child_pid = ::fork();
  if (child_pid < 0) {
    error = std::string("fork() failed: ") + std::strerror(errno);
    ::close(sockets[0]);
    ::close(sockets[1]);
    return nullptr;
  }

  if (child_pid == 0) {
    ::close(sockets[0]);
    const int rc = run_plugin_child(sockets[1], config);
    ::close(sockets[1]);
    _exit(rc);
  }

  ::close(sockets[1]);
  PluginIpcMessage hello{};
  if (!plugin_recv_message(sockets[0], hello, kHandshakeTimeoutMs, error)) {
    ::close(sockets[0]);
    ::kill(child_pid, SIGKILL);
    ::waitpid(child_pid, nullptr, 0);
    return nullptr;
  }

  const PluginMessageType type = static_cast<PluginMessageType>(hello.header.type);
  if (type == PluginMessageType::Error) {
    error = plugin_read_text_payload(hello);
    ::close(sockets[0]);
    ::waitpid(child_pid, nullptr, 0);
    return nullptr;
  }
  if (type != PluginMessageType::Hello) {
    error = std::string("expected HELLO from plugin child, got ") + plugin_message_type_name(type);
    ::close(sockets[0]);
    ::kill(child_pid, SIGKILL);
    ::waitpid(child_pid, nullptr, 0);
    return nullptr;
  }
  if (hello.payload.hello.abi_major != AARCHVM_PLUGIN_ABI_MAJOR) {
    error = "plugin ABI major mismatch";
    ::close(sockets[0]);
    ::kill(child_pid, SIGKILL);
    ::waitpid(child_pid, nullptr, 0);
    return nullptr;
  }
  if (hello.payload.hello.mmio_size != config.mmio_size) {
    error = "configured MMIO size does not match plugin-declared MMIO size";
    ::close(sockets[0]);
    ::kill(child_pid, SIGKILL);
    ::waitpid(child_pid, nullptr, 0);
    return nullptr;
  }

  return std::shared_ptr<ExternalDeviceProxy>(new ExternalDeviceProxy(
      config,
      sockets[0],
      static_cast<int>(child_pid),
      hello.payload.hello.plugin_name,
      hello.payload.hello.plugin_version,
      hello.payload.hello.mmio_size,
      std::move(fault_handler)));
}

ExternalDeviceProxy::~ExternalDeviceProxy() {
  std::string error;
  send_shutdown(&error);
  reap_child(!shutdown_sent_);
}

std::uint64_t ExternalDeviceProxy::read(std::uint64_t offset, std::size_t size) {
  if (faulted_) {
    return 0;
  }
  std::string error;
  std::uint64_t value = 0;
  if (!transact(0u,
                next_request_id_++,
                static_cast<std::uint16_t>(PluginMessageType::MmioReadReq),
                static_cast<std::uint16_t>(PluginMessageType::MmioReadResp),
                offset,
                static_cast<std::uint32_t>(size),
                0u,
                &value,
                error)) {
    handle_fault(error);
    return 0;
  }
  return value;
}

void ExternalDeviceProxy::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (faulted_) {
    return;
  }
  std::string error;
  if (!transact(0u,
                next_request_id_++,
                static_cast<std::uint16_t>(PluginMessageType::MmioWriteReq),
                static_cast<std::uint16_t>(PluginMessageType::MmioWriteResp),
                offset,
                static_cast<std::uint32_t>(size),
                value,
                nullptr,
                error)) {
    handle_fault(error);
  }
}

bool ExternalDeviceProxy::reset(std::uint64_t guest_now, std::string* error) {
  if (faulted_) {
    if (error != nullptr) {
      *error = "plugin is already faulted";
    }
    return false;
  }
  std::string local_error;
  const bool ok =
      transact(guest_now,
               next_request_id_++,
               static_cast<std::uint16_t>(PluginMessageType::ResetReq),
               static_cast<std::uint16_t>(PluginMessageType::ResetResp),
               0u,
               0u,
               0u,
               nullptr,
               local_error);
  if (!ok) {
    handle_fault(local_error);
  }
  if (error != nullptr) {
    *error = local_error;
  }
  return ok;
}

bool ExternalDeviceProxy::transact(std::uint64_t guest_now,
                                   std::uint32_t request_id,
                                   std::uint16_t request_type,
                                   std::uint16_t expected_response_type,
                                   std::uint64_t offset,
                                   std::uint32_t access_size,
                                   std::uint64_t value,
                                   std::uint64_t* read_value,
                                   std::string& error) {
  if (socket_fd_ < 0) {
    error = "plugin socket is closed";
    return false;
  }

  PluginIpcMessage request{};
  plugin_init_message(request,
                      static_cast<PluginMessageType>(request_type),
                      request_id,
                      guest_now,
                      PluginMessageStatus::Ok);
  request.header.payload_size = sizeof(request.payload.mmio);
  request.payload.mmio.offset = offset;
  request.payload.mmio.access_size = access_size;
  request.payload.mmio.value = value;
  if (!plugin_send_message(socket_fd_, request, error)) {
    append_child_status(error);
    return false;
  }

  PluginIpcMessage response{};
  if (!plugin_recv_message(socket_fd_, response, kRequestTimeoutMs, error)) {
    append_child_status(error);
    return false;
  }

  const PluginMessageType response_type = static_cast<PluginMessageType>(response.header.type);
  if (response_type == PluginMessageType::Error) {
    error = plugin_read_text_payload(response);
    return false;
  }
  if (response_type != static_cast<PluginMessageType>(expected_response_type)) {
    error = std::string("unexpected plugin response type: ") + plugin_message_type_name(response_type);
    return false;
  }
  if (response.header.request_id != request_id) {
    error = "plugin response request-id mismatch";
    return false;
  }
  if (response.header.status != static_cast<std::int32_t>(PluginMessageStatus::Ok)) {
    error = status_message(response);
    return false;
  }

  if (read_value != nullptr) {
    *read_value = response.payload.mmio.value;
  }
  return true;
}

bool ExternalDeviceProxy::send_shutdown(std::string* error) {
  if (shutdown_sent_ || socket_fd_ < 0 || faulted_) {
    shutdown_sent_ = true;
    return true;
  }

  PluginIpcMessage request{};
  std::string local_error;
  plugin_init_message(request,
                      PluginMessageType::ShutdownReq,
                      next_request_id_++,
                      0u,
                      PluginMessageStatus::Ok);
  request.header.payload_size = 0u;
  if (!plugin_send_message(socket_fd_, request, local_error)) {
    if (error != nullptr) {
      *error = local_error;
    }
    return false;
  }

  PluginIpcMessage response{};
  if (!plugin_recv_message(socket_fd_, response, kShutdownTimeoutMs, local_error) ||
      static_cast<PluginMessageType>(response.header.type) != PluginMessageType::ShutdownResp) {
    if (error != nullptr) {
      *error = local_error.empty() ? "plugin shutdown response missing" : local_error;
    }
    return false;
  }

  shutdown_sent_ = true;
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
  return true;
}

void ExternalDeviceProxy::handle_fault(const std::string& message) {
  std::string enriched = message;
  append_child_status(enriched);
  faulted_ = true;
  if (!fault_notified_ && fault_handler_) {
    fault_handler_(enriched);
    fault_notified_ = true;
  }
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

void ExternalDeviceProxy::append_child_status(std::string& message) {
  if (child_pid_ <= 0) {
    return;
  }
  int status = 0;
  const pid_t rc = ::waitpid(child_pid_, &status, WNOHANG);
  if (rc == 0 || (rc < 0 && errno == ECHILD)) {
    return;
  }
  if (rc != child_pid_) {
    return;
  }
  if (WIFEXITED(status)) {
    message += "; child exited with status " + std::to_string(WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    message += "; child terminated by signal " + std::to_string(WTERMSIG(status));
  }
  child_pid_ = -1;
}

void ExternalDeviceProxy::reap_child(bool force_kill) {
  if (child_pid_ <= 0) {
    return;
  }
  if (force_kill || !wait_for_child_exit(child_pid_, kShutdownTimeoutMs)) {
    ::kill(child_pid_, SIGKILL);
  }
  ::waitpid(child_pid_, nullptr, 0);
  child_pid_ = -1;
}

} // namespace aarchvm
