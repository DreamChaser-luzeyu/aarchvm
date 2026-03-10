#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <deque>
#include <unordered_set>

namespace aarchvm {

class UartPl011 final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;
  void inject_rx(std::uint8_t byte);
  [[nodiscard]] std::uint64_t tx_count() const { return tx_count_; }
  [[nodiscard]] std::uint64_t mmio_reads() const { return mmio_reads_; }
  [[nodiscard]] std::uint64_t mmio_writes() const { return mmio_writes_; }
  [[nodiscard]] std::uint64_t config_writes() const { return config_writes_; }
  [[nodiscard]] std::uint64_t id_reads() const { return id_reads_; }

private:
  std::deque<std::uint8_t> rx_fifo_;
  std::uint32_t ibrd_ = 1;
  std::uint32_t fbrd_ = 0;
  std::uint32_t lcrh_ = 0;
  std::uint32_t cr_ = 0x301;
  std::uint32_t ifls_ = 0;
  std::uint32_t imsc_ = 0;
  std::uint32_t ris_ = 0;
  std::uint32_t dmacr_ = 0;
  std::uint64_t tx_count_ = 0;
  std::uint64_t mmio_reads_ = 0;
  std::uint64_t mmio_writes_ = 0;
  std::uint64_t config_writes_ = 0;
  std::uint64_t id_reads_ = 0;
  std::unordered_set<std::uint64_t> traced_read_offsets_;
  std::unordered_set<std::uint64_t> traced_write_offsets_;
};

} // namespace aarchvm
