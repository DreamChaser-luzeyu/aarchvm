#pragma once

#include "aarchvm/device.hpp"
#include "aarchvm/plugin_config.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace aarchvm {

class ExternalDeviceProxy final : public Device {
public:
  using FaultHandler = std::function<void(const std::string&)>;

  static std::shared_ptr<ExternalDeviceProxy> spawn(const ExternalPluginConfig& config,
                                                    FaultHandler fault_handler,
                                                    std::string& error);

  ~ExternalDeviceProxy() override;

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  bool reset(std::uint64_t guest_now, std::string* error = nullptr);

  [[nodiscard]] bool faulted() const { return faulted_; }
  [[nodiscard]] const ExternalPluginConfig& config() const { return config_; }
  [[nodiscard]] const std::string& plugin_name() const { return plugin_name_; }
  [[nodiscard]] const std::string& plugin_version() const { return plugin_version_; }

private:
  ExternalDeviceProxy(ExternalPluginConfig config,
                      int socket_fd,
                      int child_pid,
                      std::string plugin_name,
                      std::string plugin_version,
                      std::uint64_t declared_mmio_size,
                      FaultHandler fault_handler);

  bool transact(std::uint64_t guest_now,
                std::uint32_t request_id,
                std::uint16_t request_type,
                std::uint16_t expected_response_type,
                std::uint64_t offset,
                std::uint32_t access_size,
                std::uint64_t value,
                std::uint64_t* read_value,
                std::string& error);
  bool send_shutdown(std::string* error);
  void handle_fault(const std::string& message);
  void append_child_status(std::string& message);
  void reap_child(bool force_kill);

  ExternalPluginConfig config_;
  int socket_fd_ = -1;
  int child_pid_ = -1;
  std::string plugin_name_;
  std::string plugin_version_;
  std::uint64_t declared_mmio_size_ = 0;
  std::uint32_t next_request_id_ = 1;
  FaultHandler fault_handler_;
  bool faulted_ = false;
  bool shutdown_sent_ = false;
  bool fault_notified_ = false;
};

} // namespace aarchvm
