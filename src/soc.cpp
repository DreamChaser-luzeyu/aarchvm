#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <utility>

#include "aarchvm/external_device_proxy.hpp"
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

bool debug_slow_mode_enabled() {
  static const bool enabled = env_enabled("AARCHVM_DEBUG_SLOW");
  return enabled;
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

std::optional<bool> env_scheduler_mode_is_legacy() {
  const char* mode_env = std::getenv("AARCHVM_SCHED_MODE");
  if (mode_env == nullptr || *mode_env == '\0') {
    return std::nullopt;
  }
  const std::string mode(mode_env);
  if (mode == "legacy") {
    return true;
  }
  if (mode == "event") {
    return false;
  }
  return std::nullopt;
}

std::optional<GenericTimer::ClockMode> env_arch_timer_mode() {
  const char* mode_env = std::getenv("AARCHVM_ARCH_TIMER_MODE");
  if (mode_env == nullptr || *mode_env == '\0') {
    return std::nullopt;
  }
  return GenericTimer::parse_clock_mode(mode_env);
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

void perf_accumulate(PerfCounters& dst, const PerfCounters& delta) {
  dst.steps += delta.steps;
  dst.bus_reads += delta.bus_reads;
  dst.bus_writes += delta.bus_writes;
  dst.bus_read_bytes += delta.bus_read_bytes;
  dst.bus_write_bytes += delta.bus_write_bytes;
  dst.bus_find_calls += delta.bus_find_calls;
  dst.bus_device_reads += delta.bus_device_reads;
  dst.bus_device_writes += delta.bus_device_writes;
  dst.ram_fast_reads += delta.ram_fast_reads;
  dst.ram_fast_writes += delta.ram_fast_writes;
  dst.ram_fast_read_bytes += delta.ram_fast_read_bytes;
  dst.ram_fast_write_bytes += delta.ram_fast_write_bytes;
  dst.translate_calls += delta.translate_calls;
  dst.tlb_lookups += delta.tlb_lookups;
  dst.tlb_hits += delta.tlb_hits;
  dst.tlb_misses += delta.tlb_misses;
  dst.tlb_inserts += delta.tlb_inserts;
  dst.tlb_flush_all += delta.tlb_flush_all;
  dst.tlb_flush_va += delta.tlb_flush_va;
  dst.page_walks += delta.page_walks;
  dst.page_walk_desc_reads += delta.page_walk_desc_reads;
  dst.gic_has_pending += delta.gic_has_pending;
  dst.gic_acknowledge += delta.gic_acknowledge;
  dst.gic_recompute += delta.gic_recompute;
  dst.gic_set_level += delta.gic_set_level;
  dst.soc_sync_devices += delta.soc_sync_devices;
  dst.soc_run_chunks += delta.soc_run_chunks;
}

} // namespace

SoC::SoC(std::size_t cpu_count)
    : boot_ram_(std::make_shared<Ram>(kBootRamSize)),
      framebuffer_ram_(std::make_shared<Ram>(kFramebufferSize)),
      sdram_(std::make_shared<Ram>(kSdramSize)),
      uart_(std::make_shared<UartPl011>()),
      kmi_(std::make_shared<Pl050Kmi>()),
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
      rtc_(std::make_shared<RtcPl031>()),
      virtio_blk_mmio_(std::make_shared<VirtioBlkMmio>(bus_)),
      gic_(std::make_shared<GicV3>()),
      timer_(std::make_shared<GenericTimer>()) {
  if (cpu_count == 0) {
    cpu_count = 1;
  }

  framebuffer_dirty_tracker_ = std::make_shared<FramebufferDirtyTracker>(kFramebufferWidth,
                                                                         kFramebufferHeight,
                                                                         kFramebufferStride,
                                                                         kFramebufferSize);
  framebuffer_ram_->set_write_observer(framebuffer_dirty_tracker_.get());

  bus_.map(kBootRamBase, kBootRamSize, boot_ram_);
  bus_.map(kFramebufferBase, kFramebufferSize, framebuffer_ram_);
  bus_.map(kSdramBase, kSdramSize, sdram_);
  bus_.map(kUartBase, kUartSize, uart_);
  bus_.map(kKmiBase, kKmiSize, kmi_);
  bus_.map(kPerfBase, kPerfSize, perf_mailbox_);
  bus_.map(kRtcBase, kRtcSize, rtc_);
  bus_.map(kVirtioBlkBase, kVirtioBlkSize, virtio_blk_mmio_);
  bus_.map(kGicBase, kGicSize, gic_);
  bus_.map(kTimerBase, kTimerSize, timer_);
  bus_.set_ram_write_observer([this](std::uint64_t pa, std::size_t size) {
    on_external_ram_write(pa, size);
  });

  gic_->set_cpu_count(cpu_count);
  timer_->set_cpu_count(cpu_count);
  last_timer_virt_levels_.assign(cpu_count, false);
  last_timer_phys_levels_.assign(cpu_count, false);

  cpus_.reserve(cpu_count);
  cpu_powered_on_.assign(cpu_count, true);
  for (std::size_t i = 0; i < cpu_count; ++i) {
    auto cpu = std::make_unique<Cpu>(bus_, *gic_, *timer_);
    cpu->set_callbacks(Cpu::Callbacks{
        .sev_broadcast = [this](Cpu& source) { broadcast_event(source); },
        .memory_write = [this](Cpu& source, std::uint64_t pa, std::size_t size) {
          on_cpu_memory_write(source, pa, size);
        },
        .tlbi_vmalle1_broadcast = [this](Cpu& source) { broadcast_tlbi_vmalle1(source); },
        .tlbi_vae1_broadcast = [this](Cpu& source, std::uint64_t operand, bool all_asids) {
          broadcast_tlbi_vae1(source, operand, all_asids);
        },
        .tlbi_aside1_broadcast = [this](Cpu& source, std::uint16_t asid) {
          broadcast_tlbi_aside1(source, asid);
        },
        .ic_ivau_broadcast = [this](Cpu& source) { broadcast_ic_ivau(source); },
        .smccc_call = [this](Cpu& source, bool is_hvc, std::uint16_t imm16) {
          return handle_smccc(source, is_hvc, imm16);
        },
        .time_steps = [this]() { return guest_time_ticks(); },
    });
    cpu->set_cpu_index(i);
    cpu->set_mpidr(cpu_mpidr(i));
    gic_->set_cpu_affinity(i, cpu_mpidr(i));
    cpus_.push_back(std::move(cpu));
  }

  if (!debug_slow_mode_enabled() && (env_enabled("AARCHVM_BUS_FASTPATH") || env_enabled("AARCHVM_RAM_FASTPATH"))) {
    rebuild_fast_path();
  }

  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }
  if (const auto mode = env_arch_timer_mode(); mode.has_value()) {
    arch_timer_mode_ = *mode;
  }
  if (const auto legacy = env_scheduler_mode_is_legacy(); legacy.has_value()) {
    scheduler_mode_ = *legacy ? SchedulerMode::Legacy : SchedulerMode::EventDriven;
  } else {
    scheduler_mode_ = SchedulerMode::EventDriven;
  }

  const char* fb_env = std::getenv("AARCHVM_FB_SDL");
  framebuffer_sdl_enabled_ = (fb_env == nullptr) ? true : env_enabled("AARCHVM_FB_SDL");
  uart_->set_tx_observer([this](std::uint8_t byte) { on_uart_tx(byte); });
  uart_->set_state_change_observer([this]() { invalidate_device_schedule(); });
  kmi_->set_state_change_observer([this]() { invalidate_device_schedule(); });
  virtio_blk_mmio_->set_state_change_observer([this]() { invalidate_device_schedule(); });
  timer_->set_state_change_observer([this]() { invalidate_device_schedule(); });
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->set_clock_mode(arch_timer_mode_, guest_time_ticks());

  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    cpus_[i]->reset(kBootRamBase);
    cpus_[i]->set_cpu_index(i);
    cpus_[i]->set_mpidr(cpu_mpidr(i));
    gic_->set_cpu_affinity(i, cpu_mpidr(i));
  }
  global_steps_ = 0;
  guest_time_fp_ = 0;
  timer_->rebase_to_steps(guest_time_ticks());
  reset_perf_measurement_state();
}

void SoC::rebuild_fast_path() {
  fast_path_ = std::make_shared<BusFastPath>(*boot_ram_,
                                             *framebuffer_ram_,
                                             *sdram_,
                                             *uart_,
                                             *perf_mailbox_,
                                             *virtio_blk_mmio_,
                                             *gic_,
                                             *timer_,
                                             framebuffer_dirty_tracker_.get());
  bus_.set_fast_path(fast_path_);
}

void SoC::broadcast_event(Cpu& source) {
  (void)source;
  bool woke_waiter = false;
  for (auto& cpu : cpus_) {
    woke_waiter |= cpu->waiting_for_event();
    cpu->signal_event();
  }
  runnable_state_dirty_ = runnable_state_dirty_ || woke_waiter;
}

void SoC::on_cpu_memory_write(Cpu& source, std::uint64_t pa, std::size_t size) {
  bool woke_waiter = false;
  for (auto& cpu : cpus_) {
    if (cpu.get() == &source) {
      continue;
    }
    woke_waiter |= cpu->waiting_for_event();
    cpu->notify_external_memory_write(pa, size);
  }
  runnable_state_dirty_ = runnable_state_dirty_ || woke_waiter;
}

void SoC::on_external_ram_write(std::uint64_t pa, std::size_t size) {
  bool woke_waiter = false;
  for (auto& cpu : cpus_) {
    woke_waiter |= cpu->waiting_for_event();
    cpu->notify_external_memory_write(pa, size);
  }
  runnable_state_dirty_ = runnable_state_dirty_ || woke_waiter;
}

void SoC::broadcast_tlbi_vmalle1(Cpu& source) {
  for (auto& cpu : cpus_) {
    if (cpu.get() == &source) {
      continue;
    }
    cpu->notify_tlbi_vmalle1();
  }
}

void SoC::broadcast_tlbi_vae1(Cpu& source, std::uint64_t operand, bool all_asids) {
  for (auto& cpu : cpus_) {
    if (cpu.get() == &source) {
      continue;
    }
    cpu->notify_tlbi_vae1(operand, all_asids);
  }
}

void SoC::broadcast_tlbi_aside1(Cpu& source, std::uint16_t asid) {
  for (auto& cpu : cpus_) {
    if (cpu.get() == &source) {
      continue;
    }
    cpu->notify_tlbi_aside1(asid);
  }
}

void SoC::broadcast_ic_ivau(Cpu& source) {
  for (auto& cpu : cpus_) {
    if (cpu.get() == &source) {
      continue;
    }
    cpu->notify_ic_ivau();
  }
}


std::optional<std::size_t> SoC::cpu_index_from_mpidr(std::uint64_t mpidr) const {
  const std::uint64_t aff = mpidr & 0xFF00FFFFFFull;
  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    if ((cpus_[i]->mpidr_el1() & 0xFF00FFFFFFull) == aff) {
      return i;
    }
  }
  return std::nullopt;
}

bool SoC::handle_smccc(Cpu& source, bool is_hvc, std::uint16_t imm16) {
  (void)is_hvc;
  (void)imm16;

  constexpr std::uint64_t kPsciVersion = 0x84000000ull;
  constexpr std::uint64_t kPsciCpuOff = 0x84000002ull;
  constexpr std::uint64_t kPsciCpuOn = 0xC4000003ull;
  constexpr std::uint64_t kPsciAffinityInfo = 0xC4000004ull;
  constexpr std::uint64_t kPsciMigrateInfoType = 0x84000006ull;
  constexpr std::uint64_t kPsciSystemOff = 0x84000008ull;
  constexpr std::uint64_t kPsciSystemReset = 0x84000009ull;
  constexpr std::uint64_t kPsciFeatures = 0x8400000Aull;

  constexpr std::int64_t kPsciRetSuccess = 0;
  constexpr std::int64_t kPsciRetNotSupported = -1;
  constexpr std::int64_t kPsciRetInvalidParams = -2;
  constexpr std::int64_t kPsciRetAlreadyOn = -4;
  constexpr std::uint64_t kPsciAffinityOn = 0;
  constexpr std::uint64_t kPsciAffinityOff = 1;
  constexpr std::uint64_t kPsciTosMp = 2;

  const std::uint64_t fid = source.x(0);
  switch (fid) {
    case kPsciVersion:
      source.set_x(0, 0x0000000000000002ull);
      return true;
    case kPsciFeatures: {
      const std::uint64_t query = source.x(1);
      switch (query) {
        case kPsciVersion:
        case kPsciCpuOff:
        case kPsciCpuOn:
        case kPsciAffinityInfo:
        case kPsciMigrateInfoType:
        case kPsciSystemOff:
        case kPsciSystemReset:
          source.set_x(0, 0);
          return true;
        default:
          source.set_x(0, static_cast<std::uint64_t>(kPsciRetNotSupported));
          return true;
      }
    }
    case kPsciCpuOn: {
      const auto target = cpu_index_from_mpidr(source.x(1));
      if (!target.has_value() || *target == 0) {
        source.set_x(0, static_cast<std::uint64_t>(kPsciRetInvalidParams));
        return true;
      }
      if (cpu_powered_on_[*target]) {
        source.set_x(0, static_cast<std::uint64_t>(kPsciRetAlreadyOn));
        return true;
      }
      cpus_[*target]->reset(source.x(2));
      cpus_[*target]->set_cpu_index(*target);
      cpus_[*target]->set_mpidr(cpu_mpidr(*target));
      cpus_[*target]->set_x(0, source.x(3));
      cpu_powered_on_[*target] = true;
      runnable_state_dirty_ = true;
      source.set_x(0, static_cast<std::uint64_t>(kPsciRetSuccess));
      return true;
    }
    case kPsciAffinityInfo: {
      const auto target = cpu_index_from_mpidr(source.x(1));
      if (!target.has_value()) {
        source.set_x(0, static_cast<std::uint64_t>(kPsciRetInvalidParams));
        return true;
      }
      source.set_x(0, cpu_powered_on_[*target] ? kPsciAffinityOn : kPsciAffinityOff);
      return true;
    }
    case kPsciMigrateInfoType:
      source.set_x(0, kPsciTosMp);
      return true;
    case kPsciCpuOff:
      source.set_x(0, static_cast<std::uint64_t>(kPsciRetNotSupported));
      return true;
    case kPsciSystemOff:
    case kPsciSystemReset:
      request_stop();
      source.set_x(0, static_cast<std::uint64_t>(kPsciRetSuccess));
      return true;
    default:
      return false;
  }
}

std::uint64_t SoC::cpu_mpidr(std::size_t cpu_index) {
  return 0x0000000080000000ull | static_cast<std::uint64_t>(cpu_index & 0xFFu);
}

bool SoC::load_image(std::uint64_t addr, const std::vector<std::uint32_t>& words) {
  std::uint64_t offset = 0;
  bool ok = false;
  if (in_range(addr, kBootRamBase, kBootRamSize, offset)) {
    ok = boot_ram_->load(offset, words);
  } else if (in_range(addr, kSdramBase, kSdramSize, offset)) {
    ok = sdram_->load(offset, words);
  }
  if (ok) {
    for (auto& cpu : cpus_) {
      cpu->invalidate_decode_all();
    }
  }
  return ok;
}

bool SoC::load_binary(std::uint64_t addr, const std::vector<std::uint8_t>& bytes) {
  std::uint64_t offset = 0;
  bool ok = false;
  if (in_range(addr, kBootRamBase, kBootRamSize, offset)) {
    ok = boot_ram_->load_bytes(offset, bytes);
  } else if (in_range(addr, kFramebufferBase, kFramebufferSize, offset)) {
    ok = framebuffer_ram_->load_bytes(offset, bytes);
  } else if (in_range(addr, kSdramBase, kSdramSize, offset)) {
    ok = sdram_->load_bytes(offset, bytes);
  }
  if (ok) {
    for (auto& cpu : cpus_) {
      cpu->invalidate_decode_all();
    }
  }
  return ok;
}

bool SoC::load_block_image(const std::vector<std::uint8_t>& bytes) {
  virtio_blk_mmio_->set_image(bytes);
  return true;
}

bool SoC::attach_external_plugin(const ExternalPluginConfig& config, std::string& error) {
  if (!bus_.is_range_free(config.mmio_base, config.mmio_size)) {
    error = "plugin MMIO window overlaps an existing bus mapping";
    return false;
  }

  auto proxy = ExternalDeviceProxy::spawn(
      config,
      [this](const std::string& message) {
        std::cerr << "PLUGIN-FAULT " << message << '\n';
        request_stop();
      },
      error);
  if (!proxy) {
    return false;
  }
  bus_.map(config.mmio_base, config.mmio_size, proxy);
  external_devices_.push_back(std::move(proxy));
  invalidate_device_schedule();
  return true;
}

void SoC::set_framebuffer_sdl_enabled(bool enabled) {
  framebuffer_sdl_enabled_ = enabled;
  if (!enabled) {
    framebuffer_sdl_.reset();
    return;
  }
  framebuffer_sdl_ = std::make_unique<FramebufferSdl>(framebuffer_ram_->bytes(),
                                                      kFramebufferWidth,
                                                      kFramebufferHeight,
                                                      kFramebufferStride,
                                                      framebuffer_dirty_tracker_,
                                                      [this](std::uint8_t byte) { inject_ps2_rx(byte); });
  if (!framebuffer_sdl_->available()) {
    framebuffer_sdl_.reset();
  }
}

void SoC::set_arch_timer_mode(GenericTimer::ClockMode mode) {
  arch_timer_mode_ = mode;
  timer_->set_clock_mode(arch_timer_mode_, guest_time_ticks());
  invalidate_device_schedule();
}

void SoC::reset(std::uint64_t entry_pc) {
  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    cpus_[i]->reset(entry_pc);
    cpus_[i]->set_cpu_index(i);
    cpus_[i]->set_mpidr(cpu_mpidr(i));
    gic_->set_cpu_affinity(i, cpu_mpidr(i));
    cpu_powered_on_[i] = (i == 0u) || (secondary_boot_mode_ == SecondaryBootMode::AllStart);
  }
  global_steps_ = 0;
  guest_time_fp_ = 0;
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->set_clock_mode(arch_timer_mode_, guest_time_ticks());
  perf_mailbox_->reset_state();
  reset_perf_measurement_state();
  stop_on_uart_window_.clear();
  uart_tx_match_window_.clear();
  uart_tx_match_reply_armed_ = !uart_tx_match_pattern_.empty() && !uart_tx_match_reply_text_.empty();
}

void SoC::set_predecode_enabled(bool enabled) {
  for (auto& cpu : cpus_) {
    cpu->set_predecode_enabled(enabled);
  }
}

void SoC::set_sp(std::uint64_t sp) {
  primary_cpu().set_sp(sp);
}

void SoC::set_x(std::uint32_t idx, std::uint64_t value) {
  primary_cpu().set_x(idx, value);
}

void SoC::inject_uart_rx(std::uint8_t byte) {
  uart_->inject_rx(byte);
  if (uart_->irq_pending()) {
    gic_->set_pending(kUartIntId);
  }
  invalidate_device_schedule();
}

void SoC::inject_ps2_rx(std::uint8_t byte) {
  kmi_->inject_rx(byte);
  invalidate_device_schedule();
}

void SoC::set_stop_on_uart_pattern(std::string pattern) {
  stop_on_uart_pattern_ = std::move(pattern);
  stop_on_uart_window_.clear();
}

void SoC::set_uart_tx_match_reply(std::string pattern, std::string reply_text) {
  uart_tx_match_pattern_ = std::move(pattern);
  uart_tx_match_reply_text_ = std::move(reply_text);
  uart_tx_match_window_.clear();
  uart_tx_match_reply_armed_ = !uart_tx_match_pattern_.empty() && !uart_tx_match_reply_text_.empty();
}

void SoC::on_uart_tx(std::uint8_t byte) {
  if (!stop_on_uart_pattern_.empty()) {
    stop_on_uart_window_.push_back(static_cast<char>(byte));
    if (stop_on_uart_window_.size() > stop_on_uart_pattern_.size()) {
      stop_on_uart_window_.erase(0, stop_on_uart_window_.size() - stop_on_uart_pattern_.size());
    }
    if (stop_on_uart_window_ == stop_on_uart_pattern_) {
      request_stop();
    }
  }

  if (!uart_tx_match_reply_armed_) {
    return;
  }
  uart_tx_match_window_.push_back(static_cast<char>(byte));
  if (uart_tx_match_window_.size() > uart_tx_match_pattern_.size()) {
    uart_tx_match_window_.erase(0, uart_tx_match_window_.size() - uart_tx_match_pattern_.size());
  }
  if (uart_tx_match_window_ != uart_tx_match_pattern_) {
    return;
  }

  uart_tx_match_reply_armed_ = false;
  for (unsigned char ch : uart_tx_match_reply_text_) {
    inject_uart_rx(static_cast<std::uint8_t>(ch));
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
  perf_session_.accumulated_host_ns = 0;
  perf_session_.accumulated = {};
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
    result.host_ns = perf_session_.accumulated_host_ns + (host_monotonic_ns() - perf_session_.start_host_ns);
    result.delta = perf_session_.accumulated;
    perf_accumulate(result.delta, perf_delta(end, perf_session_.start));
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
  for (auto& cpu : cpus_) {
    cpu->perf_flush_tlb_all();
  }
}

std::uint64_t SoC::guest_time_ticks() const {
  return guest_time_fp_ >> kGuestTimeFracBits;
}

void SoC::advance_guest_time(std::uint64_t executed_instructions, std::size_t active_cpu_count) {
  if (executed_instructions == 0 || active_cpu_count == 0) {
    return;
  }
  const std::uint64_t active = static_cast<std::uint64_t>(active_cpu_count);
  const std::uint64_t whole = executed_instructions / active;
  const std::uint64_t rem = executed_instructions % active;
  guest_time_fp_ += whole * kGuestTimeFracOne;
  guest_time_fp_ += (rem * kGuestTimeFracOne) / active;
}

void SoC::advance_guest_time_to(std::uint64_t guest_tick) {
  const std::uint64_t target_fp = guest_tick << kGuestTimeFracBits;
  if (target_fp <= guest_time_fp_) {
    return;
  }
  guest_time_fp_ = target_fp;
}

void SoC::invalidate_device_schedule() {
  device_sync_valid_ = false;
  device_schedule_valid_ = false;
  device_schedule_dirty_ = true;
  next_device_event_.reset();
}

SoC::CpuDispatchState SoC::inspect_cpu_dispatch_state() const {
  CpuDispatchState state{};
  state.all_powered_on_halted = true;
  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    if (!cpu_powered_on_[i]) {
      continue;
    }
    state.any_powered_on = true;
    if (cpus_[i]->halted()) {
      continue;
    }
    state.all_powered_on_halted = false;
    ++state.active_cpu_count;
    if (cpus_[i]->ready_to_run()) {
      if (state.first_runnable_cpu == std::numeric_limits<std::size_t>::max()) {
        state.first_runnable_cpu = i;
      }
      ++state.runnable_cpu_count;
    }
  }
  if (!state.any_powered_on) {
    state.all_powered_on_halted = false;
  }
  return state;
}

bool SoC::any_other_cpu_waiting_for_interrupt(std::size_t source_cpu) const {
  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    if (i == source_cpu || !cpu_powered_on_[i] || cpus_[i]->halted()) {
      continue;
    }
    if (cpus_[i]->waiting_for_interrupt()) {
      return true;
    }
  }
  return false;
}

std::size_t SoC::active_cpu_count() const {
  return inspect_cpu_dispatch_state().active_cpu_count;
}

std::size_t SoC::runnable_cpu_count() {
  return inspect_cpu_dispatch_state().runnable_cpu_count;
}

std::optional<SoC::ScheduledDeviceEvent> SoC::next_device_event(std::uint64_t guest_tick) const {
  if (timer_->uses_host_clock()) {
    auto& self = *const_cast<SoC*>(this);
    self.device_schedule_valid_ = true;
    self.device_schedule_dirty_ = false;
    self.next_device_event_.reset();
    return std::nullopt;
  }
  if (device_schedule_valid_ && !device_schedule_dirty_) {
    if (!next_device_event_.has_value() || next_device_event_->guest_tick >= guest_tick) {
      return next_device_event_;
    }
  }

  auto& self = *const_cast<SoC*>(this);
  self.device_schedule_valid_ = true;
  self.device_schedule_dirty_ = false;
  self.next_device_event_.reset();

  constexpr std::uint64_t kNoDeadline = std::numeric_limits<std::uint64_t>::max() / 4u;
  const std::uint64_t timer_steps = timer_->steps_until_irq(guest_tick, kNoDeadline);
  if (timer_steps != kNoDeadline) {
    self.next_device_event_ = ScheduledDeviceEvent{
        .type = ScheduledDeviceEvent::Type::TimerDeadline,
        .guest_tick = guest_tick + timer_steps,
    };
  }
  return self.next_device_event_;
}

PerfCounters SoC::collect_perf_counters() const {
  PerfCounters out{};
  out.steps = global_steps_;

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

  for (const auto& cpu : cpus_) {
    const auto& cpu_perf = cpu->perf_counters();
    out.translate_calls += cpu_perf.translate_calls;
    out.tlb_lookups += cpu_perf.tlb_lookups;
    out.tlb_hits += cpu_perf.tlb_hits;
    out.tlb_misses += cpu_perf.tlb_misses;
    out.tlb_inserts += cpu_perf.tlb_inserts;
    out.tlb_flush_all += cpu_perf.tlb_flush_all;
    out.tlb_flush_va += cpu_perf.tlb_flush_va;
    out.page_walks += cpu_perf.page_walks;
    out.page_walk_desc_reads += cpu_perf.page_walk_desc_reads;
  }

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
  device_sync_valid_ = false;
  device_sync_guest_ticks_ = 0;
  device_schedule_valid_ = false;
  device_schedule_dirty_ = true;
  next_device_event_.reset();
  std::fill(last_timer_virt_levels_.begin(), last_timer_virt_levels_.end(), false);
  std::fill(last_timer_phys_levels_.begin(), last_timer_phys_levels_.end(), false);
  last_uart_level_ = false;
  last_kmi_level_ = false;
  last_virtio_blk_level_ = false;
  runnable_state_dirty_ = false;
  bus_.reset_perf_counters();
  if (fast_path_ != nullptr) {
    fast_path_->reset_perf_counters();
  }
  for (auto& cpu : cpus_) {
    cpu->reset_perf_counters();
  }
  gic_->reset_perf_counters();
}

bool SoC::run(std::size_t max_steps) {
  const auto log_cpu_halt = [&](const Cpu& cpu) {
    std::cerr << "CPU-HALT cpu=" << cpu.cpu_index()
              << " mpidr=0x" << std::hex << cpu.mpidr_el1()
              << " pc=0x" << cpu.pc()
              << " sp=0x" << cpu.sp()
              << " pstate=0x" << cpu.pstate_bits()
              << std::dec
              << " steps(cpu)=" << cpu.steps()
              << " steps(global)=" << global_steps_
              << " exc_depth=" << cpu.exception_depth()
              << " wfi=" << (cpu.waiting_for_interrupt() ? 1 : 0)
              << " wfe=" << (cpu.waiting_for_event() ? 1 : 0)
              << '\n';
  };
  auto sync_devices = [&](bool force_timer_sync = false) {
    ++local_perf_counters_.sync_devices;
    const std::uint64_t guest_ticks = guest_time_ticks();
    if (force_timer_sync || !device_sync_valid_ || device_sync_guest_ticks_ != guest_ticks) {
      timer_->sync_to_steps(guest_ticks);
      device_sync_guest_ticks_ = guest_ticks;
    }

    for (std::size_t cpu = 0; cpu < cpus_.size(); ++cpu) {
      const bool timer_virt_level = timer_->irq_pending() || timer_->irq_pending_virtual(cpu);
      if (!device_sync_valid_ || timer_virt_level != last_timer_virt_levels_[cpu]) {
        gic_->set_level(cpu, kTimerVirtIntId, timer_virt_level);
        last_timer_virt_levels_[cpu] = timer_virt_level;
      }

      const bool timer_phys_level = timer_->irq_pending_physical(cpu);
      if (!device_sync_valid_ || timer_phys_level != last_timer_phys_levels_[cpu]) {
        gic_->set_level(cpu, kTimerPhysIntId, timer_phys_level);
        last_timer_phys_levels_[cpu] = timer_phys_level;
      }
    }

    const bool uart_level = uart_->irq_pending();
    if (!device_sync_valid_ || uart_level != last_uart_level_) {
      gic_->set_level(kUartIntId, uart_level);
      last_uart_level_ = uart_level;
    }

    const bool kmi_level = kmi_->irq_pending();
    if (!device_sync_valid_ || kmi_level != last_kmi_level_) {
      gic_->set_level(kKmiIntId, kmi_level);
      last_kmi_level_ = kmi_level;
    }

    const bool virtio_blk_level = virtio_blk_mmio_->irq_pending();
    if (!device_sync_valid_ || virtio_blk_level != last_virtio_blk_level_) {
      gic_->set_level(kVirtioBlkIntId, virtio_blk_level);
      last_virtio_blk_level_ = virtio_blk_level;
    }

    if (framebuffer_sdl_ != nullptr) {
      framebuffer_sdl_->present(guest_ticks);
    }

    device_sync_valid_ = true;
    device_schedule_valid_ = false;
    device_schedule_dirty_ = false;
    next_device_event_.reset();
  };
  const auto event_due_now = [&](std::uint64_t guest_ticks) {
    if (timer_->uses_host_clock()) {
      return true;
    }
    if (!device_sync_valid_ || device_schedule_dirty_) {
      return true;
    }
    const auto event = next_device_event(guest_ticks);
    return event.has_value() && event->guest_tick <= guest_ticks;
  };
  const auto deadline_driven_window_needed = [&]() {
    constexpr std::uint64_t kShortDeadlineWindow = 256;
    if (timer_->uses_host_clock()) {
      return true;
    }
    if (scheduler_mode_ == SchedulerMode::Legacy) {
      return false;
    }
    bool any_powered_on = false;
    bool all_waiting = true;
    for (std::size_t i = 0; i < cpus_.size(); ++i) {
      if (!cpu_powered_on_[i] || cpus_[i]->halted()) {
        continue;
      }
      any_powered_on = true;
      if (!cpus_[i]->waiting_for_interrupt() && !cpus_[i]->waiting_for_event()) {
        all_waiting = false;
      }
    }
    if (!any_powered_on) {
      return false;
    }
    if (all_waiting) {
      return true;
    }

    const std::uint64_t guest_ticks = guest_time_ticks();
    const auto event = next_device_event(guest_ticks);
    if (!event.has_value()) {
      return false;
    }
    if (event->guest_tick <= guest_ticks) {
      return true;
    }
    return (event->guest_tick - guest_ticks) <= kShortDeadlineWindow;
  };
  const auto maybe_sync_devices = [&]() {
    if (timer_->uses_host_clock()) {
      sync_devices(true);
      return;
    }
    if (scheduler_mode_ == SchedulerMode::Legacy) {
      sync_devices(true);
      return;
    }
    if (!deadline_driven_window_needed()) {
      sync_devices(true);
      return;
    }
    const std::uint64_t guest_ticks = guest_time_ticks();
    if (!device_sync_valid_ || device_schedule_dirty_ || event_due_now(guest_ticks)) {
      sync_devices(true);
    }
  };
  const auto fast_forward_to_next_guest_event = [&]() {
    if (timer_->uses_host_clock()) {
      return false;
    }
    if (scheduler_mode_ == SchedulerMode::Legacy) {
      return false;
    }
    const std::uint64_t guest_ticks = guest_time_ticks();
    const auto event = next_device_event(guest_ticks);
    if (!event.has_value() || event->guest_tick <= guest_ticks) {
      return false;
    }
    advance_guest_time_to(event->guest_tick);
    device_sync_valid_ = false;
    sync_devices(true);
    return true;
  };
  const auto compute_run_window = [&](std::size_t budget,
                                      std::size_t cpu_divisor,
                                      bool deadline_sensitive,
                                      std::uint64_t guest_ticks) {
    static constexpr std::size_t kHostTimerSyncChunk = 2048;
    static constexpr std::size_t kMaxLegacyInternalChunk = 65536;
    std::size_t chunk = std::max<std::size_t>(1, (budget + cpu_divisor - 1u) / cpu_divisor);
    if (timer_->uses_host_clock()) {
      return std::max<std::size_t>(1, std::min<std::size_t>(chunk, kHostTimerSyncChunk));
    }
    if (scheduler_mode_ == SchedulerMode::Legacy) {
      return std::max<std::size_t>(1, std::min<std::size_t>(chunk, kMaxLegacyInternalChunk));
    }
    if (!deadline_sensitive) {
      return chunk;
    }
    if (const auto event = next_device_event(guest_ticks); event.has_value()) {
      if (event->guest_tick <= guest_ticks) {
        chunk = 1;
      } else {
        const std::uint64_t until_event = event->guest_tick - guest_ticks;
        chunk = static_cast<std::size_t>(std::min<std::uint64_t>(chunk, until_event));
      }
    }
    return std::max<std::size_t>(1, chunk);
  };

  std::size_t remaining = max_steps;

  if (cpus_.size() == 1) {
    Cpu& cpu = primary_cpu();
    while (remaining > 0 && !stop_requested_) {
      maybe_sync_devices();
      if (!cpu.ready_to_run() && fast_forward_to_next_guest_event()) {
        continue;
      }

      const std::uint64_t guest_ticks = guest_time_ticks();
      const bool deadline_sensitive = deadline_driven_window_needed();
      std::size_t window = compute_run_window(remaining, 1u, deadline_sensitive, guest_ticks);
      while (window > 0 && remaining > 0 && !stop_requested_) {
        ++local_perf_counters_.run_chunks;
        const std::size_t chunk = window;
        std::size_t executed = 0;
        while (executed < chunk) {
          const bool ok = cpu.step();
          ++global_steps_;
          advance_guest_time(1, 1);
          ++executed;
          if (!ok) {
            if (cpu.halted()) {
              log_cpu_halt(cpu);
              request_stop();
              return true;
            }
            return false;
          }
          if (stop_requested_) {
            return true;
          }
          if (device_schedule_dirty_) {
            break;
          }
        }
        remaining -= executed;
        window -= std::min(window, executed);
        if (device_schedule_dirty_) {
          break;
        }
      }
    }

    sync_devices(true);
    return true;
  }

  while (remaining > 0 && !stop_requested_) {
    maybe_sync_devices();

    const CpuDispatchState dispatch = inspect_cpu_dispatch_state();
    std::size_t runnable_cpus = dispatch.runnable_cpu_count;
    bool fallback_poll_waiters = false;
    if (runnable_cpus == 0) {
      if (dispatch.all_powered_on_halted) {
        request_stop();
        break;
      }
      if (fast_forward_to_next_guest_event()) {
        continue;
      }
      runnable_cpus = dispatch.active_cpu_count;
      fallback_poll_waiters = true;
    }
    if (runnable_cpus == 0) {
      if (dispatch.all_powered_on_halted) {
        request_stop();
      }
      break;
    }

    if (!fallback_poll_waiters && runnable_cpus == 1) {
      const std::size_t runnable_cpu_idx = dispatch.first_runnable_cpu;
      if (runnable_cpu_idx < cpus_.size()) {
        Cpu& cpu = *cpus_[runnable_cpu_idx];
        const std::uint64_t guest_ticks = guest_time_ticks();
        std::size_t window = compute_run_window(remaining, 1u, true, guest_ticks);
        const bool waiting_irq_peer = any_other_cpu_waiting_for_interrupt(runnable_cpu_idx);
        while (window > 0 && remaining > 0 && !stop_requested_) {
          ++local_perf_counters_.run_chunks;
          const std::size_t chunk = window;
          std::size_t executed = 0;
          runnable_state_dirty_ = false;
          const std::uint64_t gic_epoch_before = gic_->state_epoch();
          while (executed < chunk) {
            const bool ok = cpu.step();
            ++global_steps_;
            advance_guest_time(1, 1);
            ++executed;
            --remaining;
            if (!ok) {
              if (cpu.halted()) {
                log_cpu_halt(cpu);
                break;
              }
              return false;
            }
            if (stop_requested_) {
              return true;
            }
            if (device_schedule_dirty_ || runnable_state_dirty_ || cpu.waiting()) {
              break;
            }
            if (waiting_irq_peer && gic_->state_epoch() != gic_epoch_before) {
              break;
            }
          }
          window -= std::min(window, executed);
          if (device_schedule_dirty_ || runnable_state_dirty_ || cpu.waiting() || cpu.halted()) {
            break;
          }
          if (executed == 0) {
            break;
          }
        }
        continue;
      }
    }

    ++local_perf_counters_.run_chunks;
    const std::uint64_t guest_ticks = guest_time_ticks();
    const bool deadline_sensitive = deadline_driven_window_needed();
    const std::size_t round_budget = compute_run_window(remaining, runnable_cpus, deadline_sensitive, guest_ticks);

    for (std::size_t round = 0; round < round_budget && remaining > 0 && !stop_requested_; ++round) {
      bool any_active = false;
      std::size_t round_active_cpu_count = 0;
      std::uint64_t executed_in_round = 0;
      for (std::size_t cpu_idx = 0; cpu_idx < cpus_.size(); ++cpu_idx) {
        auto& cpu = cpus_[cpu_idx];
        if (remaining == 0 || stop_requested_) {
          break;
        }
        if (!cpu_powered_on_[cpu_idx] || cpu->halted()) {
          continue;
        }
        if (!fallback_poll_waiters && !cpu->ready_to_run()) {
          continue;
        }
        any_active = true;
        ++round_active_cpu_count;
        const bool ok = cpu->step();
        ++global_steps_;
        ++executed_in_round;
        --remaining;
        if (!ok && cpu->halted()) {
          log_cpu_halt(*cpu);
        }
        if (!ok && !cpu->halted()) {
          return false;
        }
      }

      if (!any_active) {
        if (inspect_cpu_dispatch_state().all_powered_on_halted) {
          request_stop();
        }
        break;
      }

      advance_guest_time(executed_in_round, round_active_cpu_count);
      if (device_schedule_dirty_) {
        break;
      }
    }
  }

  sync_devices(true);
  return std::all_of(cpus_.begin(), cpus_.end(), [](const auto& cpu) { return cpu->halted(); }) ||
         remaining == 0 || stop_requested_;
}

std::optional<std::uint8_t> SoC::read_u8(std::uint64_t addr) const {
  std::uint64_t value = 0;
  if (!bus_.read(addr, 1, value)) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(value & 0xFFu);
}

std::uint64_t SoC::pc() const {
  return primary_cpu().pc();
}

std::uint64_t SoC::cpu_pc(std::size_t cpu_index) const {
  return cpu(cpu_index).pc();
}

std::uint64_t SoC::steps() const {
  return global_steps_;
}

std::uint64_t SoC::cpu_steps(std::size_t cpu_index) const {
  return cpu(cpu_index).steps();
}

std::uint64_t SoC::x(std::uint32_t idx) const {
  return primary_cpu().x(idx);
}

std::uint64_t SoC::cpu_x(std::size_t cpu_index, std::uint32_t reg_index) const {
  return cpu(cpu_index).x(reg_index);
}

std::uint64_t SoC::sp() const {
  return primary_cpu().sp();
}

std::uint64_t SoC::cpu_sp(std::size_t cpu_index) const {
  return cpu(cpu_index).sp();
}

std::uint64_t SoC::cpu_mpidr_value(std::size_t cpu_index) const {
  return cpu(cpu_index).mpidr_el1();
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
  return primary_cpu().pstate_bits();
}

std::uint64_t SoC::cpu_pstate_bits(std::size_t cpu_index) const {
  return cpu(cpu_index).pstate_bits();
}

std::uint64_t SoC::icc_igrpen1_el1() const {
  return primary_cpu().icc_igrpen1_el1();
}

std::uint32_t SoC::exception_depth() const {
  return primary_cpu().exception_depth();
}

std::uint32_t SoC::cpu_exception_depth(std::size_t cpu_index) const {
  return cpu(cpu_index).exception_depth();
}

bool SoC::cpu_waiting_for_interrupt() const {
  return primary_cpu().waiting_for_interrupt();
}

bool SoC::cpu_waiting_for_interrupt(std::size_t cpu_index) const {
  return cpu(cpu_index).waiting_for_interrupt();
}

bool SoC::cpu_waiting_for_event() const {
  return primary_cpu().waiting_for_event();
}

bool SoC::cpu_waiting_for_event(std::size_t cpu_index) const {
  return cpu(cpu_index).waiting_for_event();
}

bool SoC::cpu_halted(std::size_t cpu_index) const {
  return cpu(cpu_index).halted();
}

bool SoC::cpu_powered_on(std::size_t cpu_index) const {
  return cpu_powered_on_.at(cpu_index);
}

std::uint64_t SoC::vbar_el1() const {
  return primary_cpu().vbar_el1();
}

bool SoC::irq_masked() const {
  return primary_cpu().irq_masked();
}

bool SoC::cpu_irq_masked(std::size_t cpu_index) const {
  return cpu(cpu_index).irq_masked();
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
  return timer_->counter_at_steps(guest_time_ticks());
}

std::uint64_t SoC::timer_cntv_ctl() const {
  return timer_->read_cntv_ctl_el0(0, guest_time_ticks());
}

std::uint64_t SoC::timer_cntv_cval() const {
  return timer_->read_cntv_cval_el0(0);
}

std::uint64_t SoC::timer_cntv_tval() const {
  return timer_->read_cntv_tval_el0(0, guest_time_ticks());
}

std::uint64_t SoC::timer_cntp_ctl() const {
  return timer_->read_cntp_ctl_el0(0, guest_time_ticks());
}

std::uint64_t SoC::timer_cntp_cval() const {
  return timer_->read_cntp_cval_el0(0);
}

std::uint64_t SoC::timer_cntp_tval() const {
  return timer_->read_cntp_tval_el0(0, guest_time_ticks());
}

bool SoC::save_snapshot(const std::string& path) const {
  if (!external_devices_.empty()) {
    std::cerr << "Snapshot save is not supported while external plugins are attached\n";
    return false;
  }
  const_cast<GenericTimer&>(*timer_).sync_to_steps(guest_time_ticks());
  if (stop_requested_) {
    for (auto& cpu : const_cast<SoC*>(this)->cpus_) {
      cpu->clear_halt();
    }
  }

  PerfSession snapshot_perf_session = perf_session_;
  if (snapshot_perf_session.active) {
    const PerfCounters current = collect_perf_counters();
    perf_accumulate(snapshot_perf_session.accumulated, perf_delta(current, perf_session_.start));
    snapshot_perf_session.accumulated_host_ns += host_monotonic_ns() - perf_session_.start_host_ns;
    snapshot_perf_session.start = current;
    snapshot_perf_session.start_host_ns = 0;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  static constexpr char kMagic[8] = {'A', 'A', 'R', 'C', 'H', 'S', 'N', 'P'};
  static constexpr std::uint32_t kVersion = 24;
  const std::uint32_t snapshot_cpu_count = static_cast<std::uint32_t>(cpus_.size());
  out.write(kMagic, sizeof(kMagic));
  if (!out ||
      !snapshot_io::write(out, kVersion) ||
      !snapshot_io::write(out, kBootRamBase) ||
      !snapshot_io::write(out, kBootRamSize) ||
      !snapshot_io::write(out, kSdramBase) ||
      !snapshot_io::write(out, kSdramSize) ||
      !snapshot_io::write(out, timer_tick_scale_) ||
      !snapshot_io::write(out, snapshot_cpu_count) ||
      !snapshot_io::write(out, global_steps_) ||
      !snapshot_io::write(out, guest_time_fp_) ||
      !snapshot_io::write(out, static_cast<std::uint32_t>(secondary_boot_mode_ == SecondaryBootMode::PsciOff ? 1u : 0u)) ||
      !boot_ram_->save_state(out) ||
      !sdram_->save_state(out) ||
      !uart_->save_state(out) ||
      !kmi_->save_state(out) ||
      !gic_->save_state(out) ||
      !timer_->save_state(out) ||
      !perf_mailbox_->save_state(out)) {
    return false;
  }
  for (const auto& cpu : cpus_) {
    if (!cpu->save_state(out)) {
      return false;
    }
  }
  for (bool powered_on : cpu_powered_on_) {
    if (!snapshot_io::write_bool(out, powered_on)) {
      return false;
    }
  }
  if (!framebuffer_ram_->save_state(out) ||
      !virtio_blk_mmio_->save_state(out) ||
      !snapshot_io::write_bool(out, snapshot_perf_session.active) ||
      !snapshot_io::write(out, snapshot_perf_session.case_id) ||
      !snapshot_io::write(out, snapshot_perf_session.arg0) ||
      !snapshot_io::write(out, snapshot_perf_session.arg1) ||
      !snapshot_io::write(out, snapshot_perf_session.accumulated_host_ns) ||
      !snapshot_io::write(out, snapshot_perf_session.accumulated) ||
      !rtc_->save_state(out)) {
    return false;
  }
  return static_cast<bool>(out);
}

bool SoC::load_snapshot(const std::string& path) {
  if (!external_devices_.empty()) {
    std::cerr << "Snapshot load is not supported while external plugins are attached\n";
    return false;
  }
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
      (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != 6 &&
      version != 7 && version != 8 && version != 9 && version != 10 && version != 11 && version != 12 &&
      version != 13 && version != 14 && version != 15 && version != 16 && version != 17 &&
      version != 18 && version != 19 && version != 20 && version != 21 && version != 22 &&
      version != 23 && version != 24) ||
      boot_ram_base != kBootRamBase || boot_ram_size != kBootRamSize ||
      sdram_base != kSdramBase || sdram_size != kSdramSize ||
      !snapshot_io::read(in, timer_tick_scale_)) {
    return false;
  }

  PerfSession restored_perf_session{};

  std::uint32_t snapshot_cpu_count = 1;
  if (version >= 8) {
    if (!snapshot_io::read(in, snapshot_cpu_count) || snapshot_cpu_count != cpus_.size() ||
        !snapshot_io::read(in, global_steps_)) {
      return false;
    }
    if (version >= 13) {
      if (!snapshot_io::read(in, guest_time_fp_)) {
        return false;
      }
    } else if (version >= 10) {
      std::uint64_t guest_ticks = 0;
      if (!snapshot_io::read(in, guest_ticks)) {
        return false;
      }
      guest_time_fp_ = guest_ticks << kGuestTimeFracBits;
    } else {
      guest_time_fp_ = global_steps_ << kGuestTimeFracBits;
    }
    if (version >= 9) {
      std::uint32_t boot_mode = 0;
      if (!snapshot_io::read(in, boot_mode)) {
        return false;
      }
      secondary_boot_mode_ = (boot_mode != 0u) ? SecondaryBootMode::PsciOff : SecondaryBootMode::AllStart;
    }
  } else {
    if (cpus_.size() != 1) {
      return false;
    }
    global_steps_ = 0;
    guest_time_fp_ = 0;
  }

  if (!boot_ram_->load_state(in) || !sdram_->load_state(in) || !uart_->load_state(in) ||
      (version >= 6 && !kmi_->load_state(in)) || !gic_->load_state(in, version) ||
      !timer_->load_state(in, version) ||
      (version >= 11 && !perf_mailbox_->load_state(in))) {
    return false;
  }
  if (version < 11) {
    perf_mailbox_->reset_state();
  }

  if (version >= 8) {
    for (auto& cpu : cpus_) {
      if (!cpu->load_state(in, version)) {
        return false;
      }
    }
    cpu_powered_on_.assign(cpus_.size(), true);
    if (version >= 9) {
      for (std::size_t i = 0; i < cpu_powered_on_.size(); ++i) {
        bool powered_on = true;
        if (!snapshot_io::read_bool(in, powered_on)) {
          return false;
        }
        cpu_powered_on_[i] = powered_on;
      }
    }
  } else {
    if (!primary_cpu().load_state(in, version)) {
      return false;
    }
    global_steps_ = primary_cpu().steps();
    guest_time_fp_ = global_steps_ << kGuestTimeFracBits;
    cpu_powered_on_.assign(cpus_.size(), true);
  }

  if (version < 6) {
    kmi_->reset();
  }
  if (version >= 5) {
    if (!framebuffer_ram_->load_state(in)) {
      return false;
    }
    const bool block_state_ok =
        (version >= 24) ? virtio_blk_mmio_->load_state(in) : virtio_blk_mmio_->load_legacy_block_mmio_state(in);
    if (!block_state_ok) {
      return false;
    }
  } else {
    framebuffer_ram_->load_bytes(0, std::vector<std::uint8_t>(kFramebufferSize, 0));
    virtio_blk_mmio_->set_image({});
  }
  if (version >= 11) {
    if (!snapshot_io::read_bool(in, restored_perf_session.active) ||
        !snapshot_io::read(in, restored_perf_session.case_id) ||
        !snapshot_io::read(in, restored_perf_session.arg0) ||
        !snapshot_io::read(in, restored_perf_session.arg1) ||
        !snapshot_io::read(in, restored_perf_session.accumulated_host_ns) ||
        !snapshot_io::read(in, restored_perf_session.accumulated)) {
      return false;
    }
  }
  if (version >= 23) {
    if (!rtc_->load_state(in)) {
      return false;
    }
  } else {
    rtc_->reset();
  }

  for (std::size_t i = 0; i < cpus_.size(); ++i) {
    cpus_[i]->set_callbacks(Cpu::Callbacks{
        .sev_broadcast = [this](Cpu& source) { broadcast_event(source); },
        .memory_write = [this](Cpu& source, std::uint64_t pa, std::size_t size) {
          on_cpu_memory_write(source, pa, size);
        },
        .tlbi_vmalle1_broadcast = [this](Cpu& source) { broadcast_tlbi_vmalle1(source); },
        .tlbi_vae1_broadcast = [this](Cpu& source, std::uint64_t operand, bool all_asids) {
          broadcast_tlbi_vae1(source, operand, all_asids);
        },
        .tlbi_aside1_broadcast = [this](Cpu& source, std::uint16_t asid) {
          broadcast_tlbi_aside1(source, asid);
        },
        .ic_ivau_broadcast = [this](Cpu& source) { broadcast_ic_ivau(source); },
        .smccc_call = [this](Cpu& source, bool is_hvc, std::uint16_t imm16) {
          return handle_smccc(source, is_hvc, imm16);
        },
        .time_steps = [this]() { return guest_time_ticks(); },
    });
    cpus_[i]->set_cpu_index(i);
    gic_->set_cpu_affinity(i, cpu_mpidr(i));
    if (version < 8) {
      cpus_[i]->set_mpidr(cpu_mpidr(i));
    }
  }

  if (!debug_slow_mode_enabled() && fast_path_ != nullptr) {
    rebuild_fast_path();
  } else if (debug_slow_mode_enabled()) {
    fast_path_.reset();
    bus_.set_fast_path(nullptr);
  }
  if (framebuffer_sdl_enabled_) {
    set_framebuffer_sdl_enabled(true);
  }
  stop_requested_ = false;
  stop_on_uart_window_.clear();
  uart_tx_match_window_.clear();
  uart_tx_match_reply_armed_ = !uart_tx_match_pattern_.empty() && !uart_tx_match_reply_text_.empty();
  device_sync_valid_ = false;
  if (const auto scale = env_timer_scale(); scale.has_value()) {
    timer_tick_scale_ = *scale;
  }
  timer_->set_cycles_per_step(timer_tick_scale_);
  timer_->rebase_to_steps(guest_time_ticks());
  timer_->set_clock_mode(arch_timer_mode_, guest_time_ticks());
  reset_perf_measurement_state();
  if (version >= 11 && restored_perf_session.active) {
    perf_session_.active = true;
    perf_session_.case_id = restored_perf_session.case_id;
    perf_session_.arg0 = restored_perf_session.arg0;
    perf_session_.arg1 = restored_perf_session.arg1;
    perf_session_.accumulated_host_ns = restored_perf_session.accumulated_host_ns;
    perf_session_.accumulated = restored_perf_session.accumulated;
    perf_session_.start = collect_perf_counters();
    perf_session_.start_host_ns = host_monotonic_ns();
  }
  return true;
}

} // namespace aarchvm
