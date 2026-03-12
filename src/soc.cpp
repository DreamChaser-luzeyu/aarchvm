#include <cstdlib>
#include <fstream>

#include "aarchvm/snapshot_io.hpp"
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

bool env_enabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
}

std::optional<std::uint64_t> env_timer_scale() {
  const char* scale_env = std::getenv("AARCHVM_TIMER_SCALE");
  if (scale_env == nullptr || *scale_env == '\0') {
    return std::nullopt;
  }
  const unsigned long long scale = std::strtoull(scale_env, nullptr, 0);
  if (scale == 0) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(scale);
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

  if (env_enabled("AARCHVM_BUS_FASTPATH") || env_enabled("AARCHVM_RAM_FASTPATH")) {
    fast_path_ = std::make_shared<BusFastPath>(*boot_ram_, *sdram_);
    bus_.set_fast_path(fast_path_);
  }

  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }

  timer_->set_cycles_per_step(timer_tick_scale_);
  cpu_.reset(kBootRamBase);
  timer_->rebase_to_steps(cpu_.steps());
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
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->rebase_to_steps(cpu_.steps());
}

void SoC::set_sp(std::uint64_t sp) {
  cpu_.set_sp(sp);
}

void SoC::set_x(std::uint32_t idx, std::uint64_t value) {
  cpu_.set_x(idx, value);
}

void SoC::inject_uart_rx(std::uint8_t byte) {
  uart_->inject_rx(byte);
  if (uart_->irq_pending()) {
    gic_->set_pending(kUartIntId);
  }
}

bool SoC::run(std::size_t max_steps) {
  auto sync_devices = [&]() {
    timer_->sync_to_steps(cpu_.steps());
    gic_->set_level(kTimerVirtIntId, timer_->irq_pending() || timer_->irq_pending_virtual());
    gic_->set_level(kTimerPhysIntId, timer_->irq_pending_physical());
    gic_->set_level(kUartIntId, uart_->irq_pending());
  };

  static constexpr std::size_t kMaxInternalChunk = 65536;
  std::size_t remaining = max_steps;
  while (remaining > 0) {
    sync_devices();

    std::size_t chunk = std::min<std::size_t>(remaining, kMaxInternalChunk);
    if (uart_->irq_pending()) {
      chunk = 1;
    } else {
      const std::uint64_t timer_limit = timer_->steps_until_irq(cpu_.steps(), chunk);
      if (timer_limit == 0u) {
        chunk = 1;
      } else {
        chunk = static_cast<std::size_t>(std::min<std::uint64_t>(chunk, timer_limit));
      }
    }

    for (std::size_t i = 0; i < chunk; ++i) {
      if (!cpu_.step()) {
        return cpu_.halted();
      }
    }
    remaining -= chunk;
  }

  sync_devices();
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

std::uint64_t SoC::x(std::uint32_t idx) const {
  return cpu_.x(idx);
}

std::uint64_t SoC::sp() const {
  return cpu_.sp();
}

std::uint64_t SoC::uart_tx_count() const {
  return uart_->tx_count();
}

std::uint64_t SoC::uart_mmio_reads() const {
  return uart_->mmio_reads();
}

std::uint64_t SoC::uart_mmio_writes() const {
  return uart_->mmio_writes();
}

std::uint64_t SoC::uart_config_writes() const {
  return uart_->config_writes();
}

std::uint64_t SoC::uart_id_reads() const {
  return uart_->id_reads();
}

std::size_t SoC::uart_rx_fifo_size() const {
  return uart_->rx_fifo_size();
}

std::uint64_t SoC::uart_rx_injected_count() const {
  return uart_->rx_injected_count();
}

std::uint32_t SoC::uart_cr() const {
  return uart_->cr();
}

std::uint32_t SoC::uart_imsc() const {
  return uart_->imsc();
}

std::uint32_t SoC::uart_ris() const {
  return uart_->ris();
}

std::uint64_t SoC::pstate_bits() const {
  return cpu_.pstate_bits();
}

std::uint64_t SoC::icc_igrpen1_el1() const {
  return cpu_.icc_igrpen1_el1();
}

std::uint32_t SoC::exception_depth() const {
  return cpu_.exception_depth();
}

bool SoC::cpu_waiting_for_interrupt() const {
  return cpu_.waiting_for_interrupt();
}

bool SoC::cpu_waiting_for_event() const {
  return cpu_.waiting_for_event();
}

std::uint64_t SoC::vbar_el1() const {
  return cpu_.vbar_el1();
}

bool SoC::irq_masked() const {
  return cpu_.irq_masked();
}

bool SoC::gic_pending(std::uint32_t intid) const {
  return gic_->pending(intid);
}

bool SoC::gic_enabled(std::uint32_t intid) const {
  return gic_->enabled(intid);
}

std::uint32_t SoC::gicd_ctlr() const {
  return gic_->gicd_ctlr();
}

std::uint64_t SoC::timer_counter() const {
  return timer_->counter_at_steps(cpu_.steps());
}

std::uint64_t SoC::timer_cntv_ctl() const {
  return timer_->read_cntv_ctl_el0(cpu_.steps());
}

std::uint64_t SoC::timer_cntv_cval() const {
  return timer_->read_cntv_cval_el0();
}

std::uint64_t SoC::timer_cntv_tval() const {
  return timer_->read_cntv_tval_el0(cpu_.steps());
}

std::uint64_t SoC::timer_cntp_ctl() const {
  return timer_->read_cntp_ctl_el0(cpu_.steps());
}

std::uint64_t SoC::timer_cntp_cval() const {
  return timer_->read_cntp_cval_el0();
}

std::uint64_t SoC::timer_cntp_tval() const {
  return timer_->read_cntp_tval_el0(cpu_.steps());
}

bool SoC::save_snapshot(const std::string& path) const {
  const_cast<GenericTimer&>(*timer_).sync_to_steps(cpu_.steps());

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  static constexpr char kMagic[8] = {'A', 'A', 'R', 'C', 'H', 'S', 'N', 'P'};
  static constexpr std::uint32_t kVersion = 4;
  out.write(kMagic, sizeof(kMagic));
  if (!out ||
      !snapshot_io::write(out, kVersion) ||
      !snapshot_io::write(out, kBootRamBase) ||
      !snapshot_io::write(out, kBootRamSize) ||
      !snapshot_io::write(out, kSdramBase) ||
      !snapshot_io::write(out, kSdramSize) ||
      !snapshot_io::write(out, timer_tick_scale_) ||
      !boot_ram_->save_state(out) ||
      !sdram_->save_state(out) ||
      !uart_->save_state(out) ||
      !gic_->save_state(out) ||
      !timer_->save_state(out) ||
      !cpu_.save_state(out)) {
    return false;
  }
  return static_cast<bool>(out);
}

bool SoC::load_snapshot(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  char magic[8] = {};
  std::uint32_t version = 0;
  std::uint64_t boot_ram_base = 0;
  std::uint64_t boot_ram_size = 0;
  std::uint64_t sdram_base = 0;
  std::uint64_t sdram_size = 0;
  if (!in.read(magic, sizeof(magic)) ||
      !snapshot_io::read(in, version) ||
      !snapshot_io::read(in, boot_ram_base) ||
      !snapshot_io::read(in, boot_ram_size) ||
      !snapshot_io::read(in, sdram_base) ||
      !snapshot_io::read(in, sdram_size) ||
      magic[0] != 'A' || magic[1] != 'A' || magic[2] != 'R' || magic[3] != 'C' ||
      magic[4] != 'H' || magic[5] != 'S' || magic[6] != 'N' || magic[7] != 'P' ||
      (version != 1 && version != 2 && version != 3 && version != 4) || boot_ram_base != kBootRamBase || boot_ram_size != kBootRamSize ||
      sdram_base != kSdramBase || sdram_size != kSdramSize ||
      !snapshot_io::read(in, timer_tick_scale_) ||
      !boot_ram_->load_state(in) ||
      !sdram_->load_state(in) ||
      !uart_->load_state(in) ||
      !gic_->load_state(in, version) ||
      !timer_->load_state(in, version) ||
      !cpu_.load_state(in, version)) {
    return false;
  }
  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->rebase_to_steps(cpu_.steps());
  return true;
}

} // namespace aarchvm
