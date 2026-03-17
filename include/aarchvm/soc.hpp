#pragma once

#include "aarchvm/block_mmio.hpp"
#include "aarchvm/bus.hpp"
#include "aarchvm/bus_fast_path.hpp"
#include "aarchvm/cpu.hpp"
#include "aarchvm/framebuffer_dirty_tracker.hpp"
#include "aarchvm/framebuffer_sdl.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"
#include "aarchvm/perf_mailbox.hpp"
#include "aarchvm/pl050_kmi.hpp"
#include "aarchvm/perf_types.hpp"
#include "aarchvm/ram.hpp"
#include "aarchvm/uart_pl011.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aarchvm {

class SoC {
public:
  enum class SecondaryBootMode {
    AllStart,
    PsciOff,
  };

  explicit SoC(std::size_t cpu_count = 1);

  bool load_image(std::uint64_t addr, const std::vector<std::uint32_t>& words);
  bool load_binary(std::uint64_t addr, const std::vector<std::uint8_t>& bytes);
  bool load_block_image(const std::vector<std::uint8_t>& bytes);
  void set_framebuffer_sdl_enabled(bool enabled);
  void set_secondary_boot_mode(SecondaryBootMode mode) { secondary_boot_mode_ = mode; }
  void reset(std::uint64_t entry_pc);
  void set_predecode_enabled(bool enabled);
  void set_sp(std::uint64_t sp);
  void set_x(std::uint32_t idx, std::uint64_t value);
  void inject_uart_rx(std::uint8_t byte);
  void inject_ps2_rx(std::uint8_t byte);
  void set_stop_on_uart_pattern(std::string pattern);
  bool run(std::size_t max_steps);
  [[nodiscard]] bool stop_requested() const { return stop_requested_; }
  [[nodiscard]] std::optional<std::uint8_t> read_u8(std::uint64_t addr) const;
  [[nodiscard]] std::uint64_t pc() const;
  [[nodiscard]] std::uint64_t cpu_pc(std::size_t cpu_index) const;
  [[nodiscard]] std::uint64_t steps() const;
  [[nodiscard]] std::uint64_t cpu_steps(std::size_t cpu_index) const;
  [[nodiscard]] std::size_t cpu_count() const { return cpus_.size(); }
  [[nodiscard]] std::uint64_t x(std::uint32_t idx) const;
  [[nodiscard]] std::uint64_t cpu_x(std::size_t cpu_index, std::uint32_t reg_index) const;
  [[nodiscard]] std::uint64_t sp() const;
  [[nodiscard]] std::uint64_t cpu_sp(std::size_t cpu_index) const;
  [[nodiscard]] std::uint64_t cpu_mpidr_value(std::size_t cpu_index) const;
  [[nodiscard]] std::uint64_t uart_tx_count() const;
  [[nodiscard]] std::uint64_t uart_mmio_reads() const;
  [[nodiscard]] std::uint64_t uart_mmio_writes() const;
  [[nodiscard]] std::uint64_t uart_config_writes() const;
  [[nodiscard]] std::uint64_t uart_id_reads() const;
  [[nodiscard]] std::size_t uart_rx_fifo_size() const;
  [[nodiscard]] std::uint64_t uart_rx_injected_count() const;
  [[nodiscard]] std::uint32_t uart_cr() const;
  [[nodiscard]] std::uint32_t uart_imsc() const;
  [[nodiscard]] std::uint32_t uart_ris() const;
  [[nodiscard]] std::uint64_t pstate_bits() const;
  [[nodiscard]] std::uint64_t cpu_pstate_bits(std::size_t cpu_index) const;
  [[nodiscard]] std::uint64_t icc_igrpen1_el1() const;
  [[nodiscard]] std::uint32_t exception_depth() const;
  [[nodiscard]] std::uint32_t cpu_exception_depth(std::size_t cpu_index) const;
  [[nodiscard]] bool cpu_waiting_for_interrupt() const;
  [[nodiscard]] bool cpu_waiting_for_interrupt(std::size_t cpu_index) const;
  [[nodiscard]] bool cpu_waiting_for_event() const;
  [[nodiscard]] bool cpu_waiting_for_event(std::size_t cpu_index) const;
  [[nodiscard]] bool cpu_halted(std::size_t cpu_index) const;
  [[nodiscard]] bool cpu_powered_on(std::size_t cpu_index) const;
  [[nodiscard]] std::uint64_t vbar_el1() const;
  [[nodiscard]] bool irq_masked() const;
  [[nodiscard]] bool cpu_irq_masked(std::size_t cpu_index) const;
  [[nodiscard]] bool gic_pending(std::uint32_t intid) const;
  [[nodiscard]] bool gic_enabled(std::uint32_t intid) const;
  [[nodiscard]] std::uint32_t gicd_ctlr() const;
  [[nodiscard]] std::uint64_t timer_counter() const;
  [[nodiscard]] std::uint64_t timer_cntv_ctl() const;
  [[nodiscard]] std::uint64_t timer_cntv_cval() const;
  [[nodiscard]] std::uint64_t timer_cntv_tval() const;
  [[nodiscard]] std::uint64_t timer_cntp_ctl() const;
  [[nodiscard]] std::uint64_t timer_cntp_cval() const;
  [[nodiscard]] std::uint64_t timer_cntp_tval() const;
  [[nodiscard]] bool save_snapshot(const std::string& path) const;
  [[nodiscard]] bool load_snapshot(const std::string& path);

private:
  enum class SchedulerMode : std::uint8_t {
    Legacy,
    EventDriven,
  };

  static constexpr std::uint32_t kGuestTimeFracBits = 16;
  static constexpr std::uint64_t kGuestTimeFracOne = 1ull << kGuestTimeFracBits;
  static constexpr std::uint64_t kBootRamBase = 0x00000000;
  static constexpr std::uint64_t kBootRamSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kFramebufferBase = 0x10000000;
  static constexpr std::uint64_t kFramebufferSize = 0x00400000;
  static constexpr std::uint32_t kFramebufferWidth = 800;
  static constexpr std::uint32_t kFramebufferHeight = 600;
  static constexpr std::uint32_t kFramebufferStride = kFramebufferWidth * 4u;
  static constexpr std::uint64_t kSdramBase = 0x40000000;
  static constexpr std::uint64_t kSdramSize = 1024ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kUartBase = 0x09000000;
  static constexpr std::uint64_t kUartSize = 0x1000;
  static constexpr std::uint64_t kKmiBase = 0x09010000;
  static constexpr std::uint64_t kKmiSize = 0x1000;
  static constexpr std::uint64_t kPerfBase = 0x09020000;
  static constexpr std::uint64_t kPerfSize = 0x1000;
  static constexpr std::uint64_t kBlockBase = 0x09040000;
  static constexpr std::uint64_t kBlockSize = 0x1000;
  static constexpr std::uint64_t kGicBase = 0x08000000;
  static constexpr std::uint64_t kGicSize = 0x100000;
  static constexpr std::uint64_t kTimerBase = 0x0A000000;
  static constexpr std::uint64_t kTimerSize = 0x1000;
  static constexpr std::uint32_t kTimerVirtIntId = 27; // PPI 11 => INTID 27
  static constexpr std::uint32_t kTimerPhysIntId = 30; // PPI 14 => INTID 30
  static constexpr std::uint32_t kUartIntId = 33;
  static constexpr std::uint32_t kKmiIntId = 34;

  void request_stop();
  void on_uart_tx(std::uint8_t byte);
  void perf_begin(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1);
  [[nodiscard]] PerfResult perf_end(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1);
  void perf_flush_tlb();
  [[nodiscard]] PerfCounters collect_perf_counters() const;
  void reset_perf_measurement_state();
  [[nodiscard]] std::uint64_t guest_time_ticks() const;
  void advance_guest_time(std::uint64_t executed_instructions, std::size_t active_cpu_count);
  void invalidate_device_schedule();
  [[nodiscard]] std::size_t active_cpu_count() const;
  struct ScheduledDeviceEvent {
    enum class Type : std::uint8_t {
      TimerDeadline,
    };

    Type type = Type::TimerDeadline;
    std::uint64_t guest_tick = 0;
  };
  [[nodiscard]] std::optional<ScheduledDeviceEvent> next_device_event(std::uint64_t guest_tick) const;
  void rebuild_fast_path();
  void broadcast_event(Cpu& source);
  void on_cpu_memory_write(Cpu& source, std::uint64_t pa, std::size_t size);
  void broadcast_tlbi_vmalle1(Cpu& source);
  void broadcast_tlbi_vae1(Cpu& source, std::uint64_t operand, bool all_asids);
  void broadcast_tlbi_aside1(Cpu& source, std::uint16_t asid);
  void broadcast_ic_ivau(Cpu& source);
  [[nodiscard]] bool handle_smccc(Cpu& source, bool is_hvc, std::uint16_t imm16);
  [[nodiscard]] std::optional<std::size_t> cpu_index_from_mpidr(std::uint64_t mpidr) const;
  [[nodiscard]] Cpu& primary_cpu() { return *cpus_.front(); }
  [[nodiscard]] const Cpu& primary_cpu() const { return *cpus_.front(); }
  [[nodiscard]] Cpu& cpu(std::size_t cpu_index) { return *cpus_.at(cpu_index); }
  [[nodiscard]] const Cpu& cpu(std::size_t cpu_index) const { return *cpus_.at(cpu_index); }
  [[nodiscard]] static std::uint64_t cpu_mpidr(std::size_t cpu_index);

  struct PerfSession {
    bool active = false;
    std::uint64_t case_id = 0;
    std::uint64_t arg0 = 0;
    std::uint64_t arg1 = 0;
    std::uint64_t accumulated_host_ns = 0;
    PerfCounters accumulated{};
    PerfCounters start{};
    std::uint64_t start_host_ns = 0;
  };

  struct LocalPerfCounters {
    std::uint64_t sync_devices = 0;
    std::uint64_t run_chunks = 0;
  };

  Bus bus_;
  std::shared_ptr<Ram> boot_ram_;
  std::shared_ptr<Ram> framebuffer_ram_;
  std::shared_ptr<Ram> sdram_;
  std::shared_ptr<UartPl011> uart_;
  std::shared_ptr<Pl050Kmi> kmi_;
  std::shared_ptr<PerfMailbox> perf_mailbox_;
  std::shared_ptr<BlockMmio> block_mmio_;
  std::shared_ptr<GicV3> gic_;
  std::shared_ptr<GenericTimer> timer_;
  std::shared_ptr<BusFastPath> fast_path_;
  std::shared_ptr<FramebufferDirtyTracker> framebuffer_dirty_tracker_;
  std::unique_ptr<FramebufferSdl> framebuffer_sdl_;
  bool framebuffer_sdl_enabled_ = true;
  std::vector<std::unique_ptr<Cpu>> cpus_;
  std::vector<bool> cpu_powered_on_;
  SecondaryBootMode secondary_boot_mode_ = SecondaryBootMode::AllStart;
  SchedulerMode scheduler_mode_ = SchedulerMode::EventDriven;
  std::uint64_t timer_tick_scale_ = 1;
  std::uint64_t global_steps_ = 0;
  std::uint64_t guest_time_fp_ = 0;
  bool stop_requested_ = false;
  PerfSession perf_session_{};
  mutable LocalPerfCounters local_perf_counters_{};
  std::string stop_on_uart_pattern_;
  std::string stop_on_uart_window_;
  bool device_sync_valid_ = false;
  std::uint64_t device_sync_guest_ticks_ = 0;
  bool device_schedule_valid_ = false;
  bool device_schedule_dirty_ = true;
  std::optional<ScheduledDeviceEvent> next_device_event_;
  bool last_timer_virt_level_ = false;
  bool last_timer_phys_level_ = false;
  bool last_uart_level_ = false;
  bool last_kmi_level_ = false;
};

} // namespace aarchvm
