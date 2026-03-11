#include "aarchvm/uart_pl011.hpp"

#include "aarchvm/snapshot_io.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace aarchvm {

namespace {
constexpr std::uint32_t kRxRis = 1u << 4;
constexpr std::uint32_t kRtRis = 1u << 6;
}

std::uint64_t UartPl011::read(std::uint64_t offset, std::size_t size) {
  if (size != 1 && size != 2 && size != 4 && size != 8) {
    return 0;
  }

  ++mmio_reads_;
  const bool trace_mmio = (std::getenv("AARCHVM_TRACE_UART_MMIO") != nullptr);
  if (trace_mmio && traced_read_offsets_.insert(offset).second) {
    std::cerr << "UART-READ offset=0x" << std::hex << offset << std::dec << "\n";
  }

  if (offset == 0x00) {
    if (rx_fifo_.empty()) {
      return 0;
    }
    const std::uint8_t ch = rx_fifo_.front();
    rx_fifo_.pop_front();
    update_interrupt_state();
    return ch;
  }
  if (offset == 0x04) {
    return 0;
  }
  if (offset == 0x18) {
    const bool rx_empty = rx_fifo_.empty();
    return rx_empty ? 0x90u : 0x80u;
  }
  if (offset == 0x24) {
    ++config_writes_;
    return ibrd_;
  }
  if (offset == 0x28) {
    ++config_writes_;
    return fbrd_;
  }
  if (offset == 0x2c) {
    ++config_writes_;
    return lcrh_;
  }
  if (offset == 0x30) {
    ++config_writes_;
    return cr_;
  }
  if (offset == 0x34) {
    ++config_writes_;
    return ifls_;
  }
  if (offset == 0x38) {
    ++config_writes_;
    return imsc_;
  }
  if (offset == 0x3c) {
    return ris_;
  }
  if (offset == 0x40) {
    return ris_ & imsc_;
  }
  if (offset == 0x48) {
    ++config_writes_;
    return dmacr_;
  }
  if (offset == 0xFE0) {
    ++id_reads_;
    return 0x11u;
  }
  if (offset == 0xFE4) {
    ++id_reads_;
    return 0x10u;
  }
  if (offset == 0xFE8) {
    ++id_reads_;
    return 0x04u;
  }
  if (offset == 0xFEC) {
    ++id_reads_;
    return 0x00u;
  }
  if (offset == 0xFF0) {
    ++id_reads_;
    return 0x0Du;
  }
  if (offset == 0xFF4) {
    ++id_reads_;
    return 0xF0u;
  }
  if (offset == 0xFF8) {
    ++id_reads_;
    return 0x05u;
  }
  if (offset == 0xFFC) {
    ++id_reads_;
    return 0xB1u;
  }
  return 0;
}

void UartPl011::write(std::uint64_t offset, std::uint64_t value, std::size_t size) {
  if (size != 1 && size != 2 && size != 4 && size != 8) {
    return;
  }
  ++mmio_writes_;
  const bool trace_mmio = (std::getenv("AARCHVM_TRACE_UART_MMIO") != nullptr);
  if (trace_mmio && traced_write_offsets_.insert(offset).second) {
    std::cerr << "UART-WRITE offset=0x" << std::hex << offset << " value=0x" << value << std::dec << "\n";
  }
  if (offset == 0x00) {
    ++tx_count_;
    const char ch = static_cast<char>(value & 0xFFu);
    std::putchar(ch);
    std::fflush(stdout);
    return;
  }
  if (offset == 0x04) {
    return;
  }
  if (offset == 0x24) {
    ++config_writes_;
    ibrd_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (offset == 0x28) {
    ++config_writes_;
    fbrd_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (offset == 0x2c) {
    ++config_writes_;
    lcrh_ = static_cast<std::uint32_t>(value);
    return;
  }
  if (offset == 0x30) {
    ++config_writes_;
    cr_ = static_cast<std::uint32_t>(value);
    update_interrupt_state();
    return;
  }
  if (offset == 0x34) {
    ++config_writes_;
    ifls_ = static_cast<std::uint32_t>(value);
    update_interrupt_state();
    return;
  }
  if (offset == 0x38) {
    ++config_writes_;
    imsc_ = static_cast<std::uint32_t>(value);
    update_interrupt_state();
    return;
  }
  if (offset == 0x44) {
    ++config_writes_;
    ris_ &= ~static_cast<std::uint32_t>(value);
    update_interrupt_state();
    return;
  }
  if (offset == 0x48) {
    ++config_writes_;
    dmacr_ = static_cast<std::uint32_t>(value);
    return;
  }
}

void UartPl011::inject_rx(std::uint8_t byte) {
  ++rx_injected_count_;
  if (rx_fifo_.size() < 1024) {
    rx_fifo_.push_back(byte);
  }
  update_interrupt_state();
}

bool UartPl011::irq_pending() const {
  return (ris_ & imsc_) != 0u;
}

bool UartPl011::save_state(std::ostream& out) const {
    std::vector<std::uint8_t> fifo(rx_fifo_.begin(), rx_fifo_.end());
  return snapshot_io::write_vector(out, fifo) &&
         snapshot_io::write(out, ibrd_) &&
         snapshot_io::write(out, fbrd_) &&
         snapshot_io::write(out, lcrh_) &&
         snapshot_io::write(out, cr_) &&
         snapshot_io::write(out, ifls_) &&
         snapshot_io::write(out, imsc_) &&
         snapshot_io::write(out, ris_) &&
         snapshot_io::write(out, dmacr_) &&
         snapshot_io::write(out, tx_count_) &&
         snapshot_io::write(out, mmio_reads_) &&
         snapshot_io::write(out, mmio_writes_) &&
         snapshot_io::write(out, config_writes_) &&
         snapshot_io::write(out, id_reads_) &&
         snapshot_io::write(out, rx_injected_count_);
}

bool UartPl011::load_state(std::istream& in) {
    std::vector<std::uint8_t> fifo;
  if (!snapshot_io::read_vector(in, fifo) || fifo.size() > 1024u ||
      !snapshot_io::read(in, ibrd_) || !snapshot_io::read(in, fbrd_) || !snapshot_io::read(in, lcrh_) || !snapshot_io::read(in, cr_) ||
      !snapshot_io::read(in, ifls_) || !snapshot_io::read(in, imsc_) || !snapshot_io::read(in, ris_) || !snapshot_io::read(in, dmacr_) ||
      !snapshot_io::read(in, tx_count_) || !snapshot_io::read(in, mmio_reads_) || !snapshot_io::read(in, mmio_writes_) ||
      !snapshot_io::read(in, config_writes_) || !snapshot_io::read(in, id_reads_) || !snapshot_io::read(in, rx_injected_count_)) {
    return false;
  }
  rx_fifo_.clear();
  for (std::uint8_t byte : fifo) {
    rx_fifo_.push_back(byte);
  }
  traced_read_offsets_.clear();
  traced_write_offsets_.clear();
  update_interrupt_state();
  return true;
}

void UartPl011::update_interrupt_state() {
  ris_ &= ~(kRxRis | kRtRis);
  const bool uart_enabled = (cr_ & 0x1u) != 0u;
  const bool rx_enabled = (cr_ & 0x200u) != 0u;
  if (uart_enabled && rx_enabled && !rx_fifo_.empty()) {
    ris_ |= (kRxRis | kRtRis);
  }
}

} // namespace aarchvm
