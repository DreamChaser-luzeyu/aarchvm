#pragma once

#include "aarchvm/bus.hpp"
#include "aarchvm/cpu.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"
#include "aarchvm/ram.hpp"
#include "aarchvm/uart_pl011.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace aarchvm {

class SoC {
public:
  SoC();

  bool load_image(std::uint64_t addr, const std::vector<std::uint32_t>& words);
  bool load_binary(std::uint64_t addr, const std::vector<std::uint8_t>& bytes);
  void reset(std::uint64_t entry_pc);
  void set_sp(std::uint64_t sp);
  void set_x(std::uint32_t idx, std::uint64_t value);
  void inject_uart_rx(std::uint8_t byte);
  bool run(std::size_t max_steps);
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

private:
  static constexpr std::uint64_t kBootRamBase = 0x00000000;
  static constexpr std::uint64_t kBootRamSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kSdramBase = 0x40000000;
  static constexpr std::uint64_t kSdramSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kUartBase = 0x09000000;
  static constexpr std::uint64_t kUartSize = 0x1000;
  static constexpr std::uint64_t kGicBase = 0x08000000;
  static constexpr std::uint64_t kGicSize = 0x10000;
  static constexpr std::uint64_t kTimerBase = 0x0A000000;
  static constexpr std::uint64_t kTimerSize = 0x1000;
  static constexpr std::uint32_t kTimerIntId = 11;

  Bus bus_;
  std::shared_ptr<Ram> boot_ram_;
  std::shared_ptr<Ram> sdram_;
  std::shared_ptr<UartPl011> uart_;
  std::shared_ptr<GicV3> gic_;
  std::shared_ptr<GenericTimer> timer_;
  Cpu cpu_;
  std::uint64_t timer_tick_scale_ = 1;
};

} // namespace aarchvm
