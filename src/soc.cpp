#include "aarchvm/soc.hpp"

namespace aarchvm {

namespace {

bool in_range(std::uint64_t addr, std::uint64_t base, std::uint64_t size, std::uint64_t& offset) {
  if (addr < base || addr >= (base + size)) {
    return false;
  }
  offset = addr - base;
  return true;
}

} // namespace

SoC::SoC()
    : boot_ram_(std::make_shared<Ram>(kBootRamSize)),
      sdram_(std::make_shared<Ram>(kSdramSize)),
      uart_(std::make_shared<UartPl011>()),
      gic_(std::make_shared<GicV3>()),
      timer_(std::make_shared<GenericTimer>()),
      cpu_(bus_, *gic_, *timer_) {
  bus_.map(kBootRamBase, kBootRamSize, boot_ram_);
  bus_.map(kSdramBase, kSdramSize, sdram_);
  bus_.map(kUartBase, kUartSize, uart_);
  bus_.map(kGicBase, kGicSize, gic_);
  bus_.map(kTimerBase, kTimerSize, timer_);

  cpu_.reset(kBootRamBase);
}

bool SoC::load_image(std::uint64_t addr, const std::vector<std::uint32_t>& words) {
  std::uint64_t offset = 0;
  if (in_range(addr, kBootRamBase, kBootRamSize, offset)) {
    return boot_ram_->load(offset, words);
  }
  if (in_range(addr, kSdramBase, kSdramSize, offset)) {
    return sdram_->load(offset, words);
  }
  return false;
}

bool SoC::load_binary(std::uint64_t addr, const std::vector<std::uint8_t>& bytes) {
  std::uint64_t offset = 0;
  if (in_range(addr, kBootRamBase, kBootRamSize, offset)) {
    return boot_ram_->load_bytes(offset, bytes);
  }
  if (in_range(addr, kSdramBase, kSdramSize, offset)) {
    return sdram_->load_bytes(offset, bytes);
  }
  return false;
}

void SoC::reset(std::uint64_t entry_pc) {
  cpu_.reset(entry_pc);
}

void SoC::set_sp(std::uint64_t sp) {
  cpu_.set_sp(sp);
}

void SoC::set_x(std::uint32_t idx, std::uint64_t value) {
  cpu_.set_x(idx, value);
}

void SoC::inject_uart_rx(std::uint8_t byte) {
  uart_->inject_rx(byte);
}

bool SoC::run(std::size_t max_steps) {
  for (std::size_t i = 0; i < max_steps; ++i) {
    timer_->tick(1);
    if (timer_->irq_pending()) {
      gic_->set_pending(kTimerIntId);
      timer_->clear_irq();
    }
    cpu_.set_cntvct(timer_->counter());
    if (!cpu_.step()) {
      return cpu_.halted();
    }
  }
  return true;
}

std::optional<std::uint8_t> SoC::read_u8(std::uint64_t addr) const {
  const auto value = bus_.read(addr, 1);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(*value & 0xFFu);
}

std::uint64_t SoC::pc() const {
  return cpu_.pc();
}

std::uint64_t SoC::steps() const {
  return cpu_.steps();
}

} // namespace aarchvm
