#include "aarchvm/uart_pl011.hpp"

#include <cstdio>

namespace aarchvm {

std::uint64_t UartPl011::read(std::uint64_t offset, std::size_t size) {
  if (size != 4 && size != 8) {
    return 0;
  }

  // Minimal subset: FR indicates TX empty and RX empty/non-empty.
  if (offset == 0x18) {
    const bool rx_empty = rx_fifo_.empty();
    return rx_empty ? 0x90u : 0x80u; // TXFE=1, RXFE reflects fifo state.
  }
  if (offset == 0x00) {
    if (rx_fifo_.empty()) {
      return 0;
    }
    const std::uint8_t ch = rx_fifo_.front();
    rx_fifo_.pop_front();
    return ch;
  }
  if (offset == 0x04) {
    // RSR/ECR: no receive errors in this minimal model.
    return 0;
  }
  return 0;
}

void UartPl011::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if ((size != 4 && size != 8) || offset != 0x00) {
    return;
  }

  const char ch = static_cast<char>(value & 0xFFu);
  std::putchar(ch);
  std::fflush(stdout);
}

void UartPl011::inject_rx(std::uint8_t byte) {
  // Keep a tiny FIFO to avoid unbounded growth when guest doesn't drain input.
  if (rx_fifo_.size() < 1024) {
    rx_fifo_.push_back(byte);
  }
}

} // namespace aarchvm
