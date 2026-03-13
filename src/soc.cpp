#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>

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

std::uint64_t host_monotonic_ns() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

PerfCounters perf_delta(const PerfCounters& end, const PerfCounters& start) {
  PerfCounters d{};
  d.steps = end.steps - start.steps;
  d.bus_reads = end.bus_reads - start.bus_reads;
  d.bus_writes = end.bus_writes - start.bus_writes;
  d.bus_read_bytes = end.bus_read_bytes - start.bus_read_bytes;
  d.bus_write_bytes = end.bus_write_bytes - start.bus_write_bytes;
  d.bus_find_calls = end.bus_find_calls - start.bus_find_calls;
  d.bus_device_reads = end.bus_device_reads - start.bus_device_reads;
  d.bus_device_writes = end.bus_device_writes - start.bus_device_writes;
  d.ram_fast_reads = end.ram_fast_reads - start.ram_fast_reads;
  d.ram_fast_writes = end.ram_fast_writes - start.ram_fast_writes;
  d.ram_fast_read_bytes = end.ram_fast_read_bytes - start.ram_fast_read_bytes;
  d.ram_fast_write_bytes = end.ram_fast_write_bytes - start.ram_fast_write_bytes;
  d.translate_calls = end.translate_calls - start.translate_calls;
  d.tlb_lookups = end.tlb_lookups - start.tlb_lookups;
  d.tlb_hits = end.tlb_hits - start.tlb_hits;
  d.tlb_misses = end.tlb_misses - start.tlb_misses;
  d.tlb_inserts = end.tlb_inserts - start.tlb_inserts;
  d.tlb_flush_all = end.tlb_flush_all - start.tlb_flush_all;
  d.tlb_flush_va = end.tlb_flush_va - start.tlb_flush_va;
  d.page_walks = end.page_walks - start.page_walks;
  d.page_walk_desc_reads = end.page_walk_desc_reads - start.page_walk_desc_reads;
  d.gic_has_pending = end.gic_has_pending - start.gic_has_pending;
  d.gic_acknowledge = end.gic_acknowledge - start.gic_acknowledge;
  d.gic_recompute = end.gic_recompute - start.gic_recompute;
  d.gic_set_level = end.gic_set_level - start.gic_set_level;
  d.soc_sync_devices = end.soc_sync_devices - start.soc_sync_devices;
  d.soc_run_chunks = end.soc_run_chunks - start.soc_run_chunks;
  return d;
}

} // namespace

SoC::SoC()
    : boot_ram_(std::make_shared<Ram>(kBootRamSize)),
      sdram_(std::make_shared<Ram>(kSdramSize)),
      uart_(std::make_shared<UartPl011>()),
      perf_mailbox_(std::make_shared<PerfMailbox>(PerfMailbox::Callbacks{
          .begin = [this](std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1) {
            perf_begin(case_id, arg0, arg1);
          },
          .end = [this](std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1) {
            return perf_end(case_id, arg0, arg1);
          },
          .request_exit = [this]() { request_stop(); },
          .flush_tlb = [this]() { perf_flush_tlb(); },
      })),
      gic_(std::make_shared<GicV3>()),
      timer_(std::make_shared<GenericTimer>()),
      cpu_(bus_, *gic_, *timer_) {
  bus_.map(kBootRamBase, kBootRamSize, boot_ram_);
  bus_.map(kSdramBase, kSdramSize, sdram_);
  bus_.map(kUartBase, kUartSize, uart_);
  bus_.map(kPerfBase, kPerfSize, perf_mailbox_);
  bus_.map(kGicBase, kGicSize, gic_);
  bus_.map(kTimerBase, kTimerSize, timer_);

  if (env_enabled("AARCHVM_BUS_FASTPATH") || env_enabled("AARCHVM_RAM_FASTPATH")) {
    fast_path_ = std::make_shared<BusFastPath>(*boot_ram_, *sdram_, *uart_, *perf_mailbox_, *gic_, *timer_);
    bus_.set_fast_path(fast_path_);
  }

  if (fast_path_ != nullptr) {
    fast_path_ = std::make_shared<BusFastPath>(*boot_ram_, *sdram_, *uart_, *perf_mailbox_, *gic_, *timer_);
    bus_.set_fast_path(fast_path_);
  }
  if (fast_path_ != nullptr) {
    fast_path_ = std::make_shared<BusFastPath>(*boot_ram_, *sdram_, *uart_, *perf_mailbox_, *gic_, *timer_);
    bus_.set_fast_path(fast_path_);
  }
  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }

  uart_->set_tx_observer([this](std::uint8_t byte) { on_uart_tx(byte); });
  timer_->set_cycles_per_step(timer_tick_scale_);
  cpu_.reset(kBootRamBase);
  timer_->rebase_to_steps(cpu_.steps());
  reset_perf_measurement_state();
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
  reset_perf_measurement_state();
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

void SoC::set_stop_on_uart_pattern(std::string pattern) {
  stop_on_uart_pattern_ = std::move(pattern);
  stop_on_uart_window_.clear();
}

void SoC::on_uart_tx(std::uint8_t byte) {
  if (stop_on_uart_pattern_.empty()) {
    return;
  }
  stop_on_uart_window_.push_back(static_cast<char>(byte));
  if (stop_on_uart_window_.size() > stop_on_uart_pattern_.size()) {
    stop_on_uart_window_.erase(0, stop_on_uart_window_.size() - stop_on_uart_pattern_.size());
  }
  if (stop_on_uart_window_ == stop_on_uart_pattern_) {
    request_stop();
  }
}

void SoC::request_stop() {
  stop_requested_ = true;
}

void SoC::perf_begin(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1) {
  perf_session_.active = true;
  perf_session_.case_id = case_id;
  perf_session_.arg0 = arg0;
  perf_session_.arg1 = arg1;
  perf_session_.start = collect_perf_counters();
  perf_session_.start_host_ns = host_monotonic_ns();
}

PerfResult SoC::perf_end(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1) {
  PerfResult result{};
  result.case_id = case_id;
  result.arg0 = arg0;
  result.arg1 = arg1;
  if (perf_session_.active) {
    const PerfCounters end = collect_perf_counters();
    result.host_ns = host_monotonic_ns() - perf_session_.start_host_ns;
    result.delta = perf_delta(end, perf_session_.start);
  }
  std::cerr << "PERF-RESULT"
            << " case_id=" << result.case_id
            << " arg0=" << result.arg0
            << " arg1=" << result.arg1
            << " host_ns=" << result.host_ns
            << " steps=" << result.delta.steps
            << " translate=" << result.delta.translate_calls
            << " tlb_lookup=" << result.delta.tlb_lookups
            << " tlb_hit=" << result.delta.tlb_hits
            << " tlb_miss=" << result.delta.tlb_misses
            << " tlb_insert=" << result.delta.tlb_inserts
            << " tlb_flush_all=" << result.delta.tlb_flush_all
            << " page_walk=" << result.delta.page_walks
            << " page_desc=" << result.delta.page_walk_desc_reads
            << " bus_read=" << result.delta.bus_reads
            << " bus_write=" << result.delta.bus_writes
            << " bus_find=" << result.delta.bus_find_calls
            << " dev_read=" << result.delta.bus_device_reads
            << " dev_write=" << result.delta.bus_device_writes
            << " ram_read=" << result.delta.ram_fast_reads
            << " ram_write=" << result.delta.ram_fast_writes
            << " gic_pending=" << result.delta.gic_has_pending
            << " gic_ack=" << result.delta.gic_acknowledge
            << " gic_recompute=" << result.delta.gic_recompute
            << " gic_level=" << result.delta.gic_set_level
            << " sync_devices=" << result.delta.soc_sync_devices
            << " run_chunks=" << result.delta.soc_run_chunks
            << '\n';
  perf_session_.active = false;
  return result;
}

void SoC::perf_flush_tlb() {
  cpu_.perf_flush_tlb_all();
}

PerfCounters SoC::collect_perf_counters() const {
  PerfCounters out{};
  out.steps = cpu_.steps();

  const auto& bus_perf = bus_.perf_counters();
  out.bus_reads = bus_perf.read_ops;
  out.bus_writes = bus_perf.write_ops;
  out.bus_read_bytes = bus_perf.read_bytes;
  out.bus_write_bytes = bus_perf.write_bytes;
  out.bus_find_calls = bus_perf.find_calls;
  out.bus_device_reads = bus_perf.device_reads;
  out.bus_device_writes = bus_perf.device_writes;

  if (fast_path_ != nullptr) {
    const auto& fast_perf = fast_path_->perf_counters();
    out.ram_fast_reads = fast_perf.read_ops;
    out.ram_fast_writes = fast_perf.write_ops;
    out.ram_fast_read_bytes = fast_perf.read_bytes;
    out.ram_fast_write_bytes = fast_perf.write_bytes;
  }

  const auto& cpu_perf = cpu_.perf_counters();
  out.translate_calls = cpu_perf.translate_calls;
  out.tlb_lookups = cpu_perf.tlb_lookups;
  out.tlb_hits = cpu_perf.tlb_hits;
  out.tlb_misses = cpu_perf.tlb_misses;
  out.tlb_inserts = cpu_perf.tlb_inserts;
  out.tlb_flush_all = cpu_perf.tlb_flush_all;
  out.tlb_flush_va = cpu_perf.tlb_flush_va;
  out.page_walks = cpu_perf.page_walks;
  out.page_walk_desc_reads = cpu_perf.page_walk_desc_reads;

  const auto& gic_perf = gic_->perf_counters();
  out.gic_has_pending = gic_perf.has_pending_calls;
  out.gic_acknowledge = gic_perf.acknowledge_calls;
  out.gic_recompute = gic_perf.recompute_calls;
  out.gic_set_level = gic_perf.set_level_calls;

  out.soc_sync_devices = local_perf_counters_.sync_devices;
  out.soc_run_chunks = local_perf_counters_.run_chunks;
  return out;
}

void SoC::reset_perf_measurement_state() {
  stop_requested_ = false;
  perf_session_ = {};
  local_perf_counters_ = {};
  bus_.reset_perf_counters();
  if (fast_path_ != nullptr) {
    fast_path_->reset_perf_counters();
  }
  cpu_.reset_perf_counters();
  gic_->reset_perf_counters();
}

bool SoC::run(std::size_t max_steps) {
  auto sync_devices = [&]() {
    ++local_perf_counters_.sync_devices;
    timer_->sync_to_steps(cpu_.steps());
    gic_->set_level(kTimerVirtIntId, timer_->irq_pending() || timer_->irq_pending_virtual());
    gic_->set_level(kTimerPhysIntId, timer_->irq_pending_physical());
    gic_->set_level(kUartIntId, uart_->irq_pending());
  };

  static constexpr std::size_t kMaxInternalChunk = 65536;
  std::size_t remaining = max_steps;
  while (remaining > 0 && !stop_requested_) {
    ++local_perf_counters_.run_chunks;
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
      if (stop_requested_) {
        return true;
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
  if (fast_path_ != nullptr) {
    fast_path_ = std::make_shared<BusFastPath>(*boot_ram_, *sdram_, *uart_, *perf_mailbox_, *gic_, *timer_);
    bus_.set_fast_path(fast_path_);
  }
  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->rebase_to_steps(cpu_.steps());
  reset_perf_measurement_state();
  return true;
}

} // namespace aarchvm
