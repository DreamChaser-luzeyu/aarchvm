#pragma once

#include "aarchvm/device.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <iosfwd>
#include <vector>

namespace aarchvm {

class Bus;

class VirtioNetMmio final : public Device {
public:
  explicit VirtioNetMmio(Bus& bus);

  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;

  void attach_loopback(std::array<std::uint8_t, 6> mac = kDefaultMac);
  void detach();
  [[nodiscard]] bool enqueue_rx_frame(std::vector<std::uint8_t> frame);
  [[nodiscard]] bool irq_pending() const { return interrupt_status_ != 0u; }
  void set_state_change_observer(std::function<void()> observer) { state_change_observer_ = std::move(observer); }

  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in);

  static constexpr std::array<std::uint8_t, 6> kDefaultMac = {0x02u, 0x41u, 0x56u, 0x4du, 0x00u, 0x01u};

private:
  struct QueueState {
    std::uint32_t num = 0;
    bool ready = false;
    std::uint64_t desc_addr = 0;
    std::uint64_t avail_addr = 0;
    std::uint64_t used_addr = 0;
    std::uint16_t last_avail_idx = 0;
    std::uint16_t used_idx = 0;
  };

  struct TransferSegment {
    std::uint64_t addr = 0;
    std::uint32_t len = 0;
    bool writable = false;
  };

  [[nodiscard]] bool device_present() const { return present_; }
  void reset_device_state();
  void reset_queue_state();
  void notify_state_change() const;
  void raise_interrupt(std::uint32_t bits);
  void clear_interrupt(std::uint32_t bits);
  [[nodiscard]] std::uint64_t read_config(std::uint64_t offset, std::size_t size) const;
  [[nodiscard]] std::uint64_t read_queue_register(std::uint64_t offset, std::size_t size) const;
  void write_queue_register(std::uint64_t offset, std::uint64_t value, std::size_t size);
  [[nodiscard]] std::uint64_t device_features() const;
  [[nodiscard]] std::uint32_t device_features_word(std::uint32_t selector) const;
  void process_queue(std::uint32_t queue_index);
  [[nodiscard]] bool process_tx();
  [[nodiscard]] bool process_rx();
  [[nodiscard]] bool read_descriptor(const QueueState& queue,
                                     std::uint16_t index,
                                     std::uint64_t& addr,
                                     std::uint32_t& len,
                                     std::uint16_t& flags,
                                     std::uint16_t& next) const;
  [[nodiscard]] bool collect_chain(const QueueState& queue,
                                   std::uint16_t head_index,
                                   std::vector<TransferSegment>& segments) const;
  [[nodiscard]] bool write_used(const QueueState& queue,
                                std::uint16_t head_index,
                                std::uint32_t used_len,
                                std::uint16_t used_idx);
  [[nodiscard]] bool read_guest(std::uint64_t addr, void* dst, std::size_t size) const;
  [[nodiscard]] bool write_guest(std::uint64_t addr, const void* src, std::size_t size);
  [[nodiscard]] bool read_guest_u16(std::uint64_t addr, std::uint16_t& value) const;
  [[nodiscard]] bool write_guest_u16(std::uint64_t addr, std::uint16_t value);
  [[nodiscard]] bool write_guest_u32(std::uint64_t addr, std::uint32_t value);

  static constexpr std::uint32_t kQueueSize = 128u;
  static constexpr std::uint32_t kQueueRx = 0u;
  static constexpr std::uint32_t kQueueTx = 1u;
  static constexpr std::uint32_t kQueueCount = 2u;
  static constexpr std::uint32_t kConfigSize = 0x100u;
  static constexpr std::size_t kVirtioNetHdrSize = 10u;

  Bus& bus_;
  bool present_ = false;
  bool loopback_enabled_ = false;
  std::array<std::uint8_t, 6> mac_ = kDefaultMac;
  std::deque<std::vector<std::uint8_t>> pending_rx_frames_;
  std::uint32_t device_features_sel_ = 0;
  std::uint32_t driver_features_sel_ = 0;
  std::uint64_t driver_features_ = 0;
  std::uint32_t guest_page_size_ = 4096u;
  std::uint32_t queue_sel_ = 0;
  std::array<QueueState, kQueueCount> queues_{};
  std::uint32_t interrupt_status_ = 0;
  std::uint32_t status_ = 0;
  std::uint32_t config_generation_ = 0;
  std::function<void()> state_change_observer_;
};

} // namespace aarchvm
