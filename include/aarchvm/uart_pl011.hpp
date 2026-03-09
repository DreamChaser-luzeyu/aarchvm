#pragma once

#include "aarchvm/device.hpp"

#include <cstdint>
#include <deque>

namespace aarchvm {

class UartPl011 final : public Device {
public:
  std::uint64_t read(std::uint64_t offset, std::size_t size) override;
  void write(std::uint64_t offset, std::uint64_t value, std::size_t size) override;
  void inject_rx(std::uint8_t byte);

private:
  std::deque<std::uint8_t> rx_fifo_;
};

} // namespace aarchvm
