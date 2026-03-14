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
  SoC();

  bool load_image(std::uint64_t addr, const std::vector<std::uint32_t>& words);
  bool load_binary(std::uint64_t addr, const std::vector<std::uint8_t>& bytes);
  bool load_block_image(const std::vector<std::uint8_t>& bytes);
  void set_framebuffer_sdl_enabled(bool enabled);
  void reset(std::uint64_t entry_pc);
  void set_predecode_enabled(bool enabled);
  void set_sp(std::uint64_t sp);
  void set_x(std::uint32_t idx, std::uint64_t value);
  void inject_uart_rx(std::uint8_t byte);
  void set_stop_on_uart_pattern(std::string pattern);
  bool run(std::size_t max_steps);
  [[nodiscard]] bool stop_requested() const { return stop_requested_; }
  [[nodiscard]] std::optional<std::uint8_t> read_u8(std::uint64_t addr) const;
  [[nodiscard]] std::uint64_t pc() const;
  [[nodiscard]] std::uint64_t steps() const;
  [[nodiscard]] std::uint64_t x(std::uint32_t idx) const;
  [[nodiscard]] std::uint64_t sp() const;
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
  [[nodiscard]] std::uint64_t icc_igrpen1_el1() const;
  [[nodiscard]] std::uint32_t exception_depth() const;
  [[nodiscard]] bool cpu_waiting_for_interrupt() const;
  [[nodiscard]] bool cpu_waiting_for_event() const;
  [[nodiscard]] std::uint64_t vbar_el1() const;
  [[nodiscard]] bool irq_masked() const;
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
  static constexpr std::uint64_t kBootRamBase = 0x00000000;
  static constexpr std::uint64_t kBootRamSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kFramebufferBase = 0x10000000;
  static constexpr std::uint64_t kFramebufferSize = 0x00400000;
  static constexpr std::uint32_t kFramebufferWidth = 800;
  static constexpr std::uint32_t kFramebufferHeight = 600;
  static constexpr std::uint32_t kFramebufferStride = kFramebufferWidth * 4u;
  static constexpr std::uint64_t kSdramBase = 0x40000000;
  static constexpr std::uint64_t kSdramSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kUartBase = 0x09000000;
  static constexpr std::uint64_t kUartSize = 0x1000;
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

  void request_stop();
  void on_uart_tx(std::uint8_t byte);
  void perf_begin(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1);
  [[nodiscard]] PerfResult perf_end(std::uint64_t case_id, std::uint64_t arg0, std::uint64_t arg1);
  void perf_flush_tlb();
  [[nodiscard]] PerfCounters collect_perf_counters() const;
  void reset_perf_measurement_state();
  void rebuild_fast_path();

  struct PerfSession {
    bool active = false;
    std::uint64_t case_id = 0;
    std::uint64_t arg0 = 0;
    std::uint64_t arg1 = 0;
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
  std::shared_ptr<PerfMailbox> perf_mailbox_;
  std::shared_ptr<BlockMmio> block_mmio_;
  std::shared_ptr<GicV3> gic_;
  std::shared_ptr<GenericTimer> timer_;
  std::shared_ptr<BusFastPath> fast_path_;
  std::shared_ptr<FramebufferDirtyTracker> framebuffer_dirty_tracker_;
  std::unique_ptr<FramebufferSdl> framebuffer_sdl_;
  bool framebuffer_sdl_enabled_ = true;
  Cpu cpu_;
  std::uint64_t timer_tick_scale_ = 1;
  bool stop_requested_ = false;
  PerfSession perf_session_{};
  mutable LocalPerfCounters local_perf_counters_{};
  std::string stop_on_uart_pattern_;
  std::string stop_on_uart_window_;
  bool device_sync_valid_ = false;
  std::uint64_t device_sync_steps_ = 0;
  bool last_timer_virt_level_ = false;
  bool last_timer_phys_level_ = false;
  bool last_uart_level_ = false;
};

} // namespace aarchvm
