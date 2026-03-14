#pragma once

#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"
#include "aarchvm/perf_mailbox.hpp"
#include "aarchvm/ram.hpp"
#include "aarchvm/uart_pl011.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

namespace aarchvm {

class BusFastPath {
public:
  struct PerfCounters {
    std::uint64_t read_ops = 0;
    std::uint64_t write_ops = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
  };

  static constexpr std::uint64_t kBootRamBase = 0x00000000ull;
  static constexpr std::uint64_t kBootRamSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kSdramBase = 0x40000000ull;
  static constexpr std::uint64_t kSdramSize = 128ull * 1024ull * 1024ull;
  static constexpr std::uint64_t kUartBase = 0x09000000ull;
  static constexpr std::uint64_t kUartSize = 0x1000ull;
  static constexpr std::uint64_t kPerfBase = 0x09020000ull;
  static constexpr std::uint64_t kPerfSize = 0x1000ull;
  static constexpr std::uint64_t kGicBase = 0x08000000ull;
  static constexpr std::uint64_t kGicSize = 0x100000ull;
  static constexpr std::uint64_t kTimerBase = 0x0A000000ull;
  static constexpr std::uint64_t kTimerSize = 0x1000ull;

  BusFastPath(Ram& boot_ram,
              Ram& sdram,
              UartPl011& uart,
              PerfMailbox& perf_mailbox,
              GicV3& gic,
              GenericTimer& timer)
      : boot_ram_bytes_(boot_ram.bytes().data()),
        boot_ram_mutable_(const_cast<std::uint8_t*>(boot_ram.bytes().data())),
        sdram_bytes_(sdram.bytes().data()),
        sdram_mutable_(const_cast<std::uint8_t*>(sdram.bytes().data())),
        uart_(&uart),
        perf_mailbox_(&perf_mailbox),
        gic_(&gic),
        timer_(&timer) {}

  [[nodiscard]] bool read(std::uint64_t addr, std::size_t size, std::uint64_t& value) const {
    if (read_ram_only(addr, size, value)) {
      return true;
    }

    if (kUartBase <= addr && addr < kUartBase + kUartSize) {
      const std::uint64_t uart_off = addr - kUartBase;
      if (size <= (kUartSize - uart_off)) {
        ++perf_counters_.read_ops;
        perf_counters_.read_bytes += size;
        value = uart_->read(uart_off, size);
        return true;
      }
    }

    if (kPerfBase <= addr && addr < kPerfBase + kPerfSize) {
      const std::uint64_t perf_off = addr - kPerfBase;
      if (size <= (kPerfSize - perf_off)) {
        ++perf_counters_.read_ops;
        perf_counters_.read_bytes += size;
        value = perf_mailbox_->read(perf_off, size);
        return true;
      }
    }

    if (kGicBase <= addr && addr < kGicBase + kGicSize) {
      const std::uint64_t gic_off = addr - kGicBase;
      if (size <= (kGicSize - gic_off)) {
        ++perf_counters_.read_ops;
        perf_counters_.read_bytes += size;
        value = gic_->read(gic_off, size);
        return true;
      }
    }

    if (kTimerBase <= addr && addr < kTimerBase + kTimerSize) {
      const std::uint64_t timer_off = addr - kTimerBase;
      if (size <= (kTimerSize - timer_off)) {
        ++perf_counters_.read_ops;
        perf_counters_.read_bytes += size;
        value = timer_->read(timer_off, size);
        return true;
      }
    }

    return false;
  }

  [[nodiscard]] bool write(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
    if (write_ram_only(addr, value, size)) {
      return true;
    }

    if (kUartBase <= addr && addr < kUartBase + kUartSize) {
      const std::uint64_t uart_off = addr - kUartBase;
      if (size <= (kUartSize - uart_off)) {
        ++perf_counters_.write_ops;
        perf_counters_.write_bytes += size;
        uart_->write(uart_off, value, size);
        return true;
      }
    }

    if (kPerfBase <= addr && addr < kPerfBase + kPerfSize) {
      const std::uint64_t perf_off = addr - kPerfBase;
      if (size <= (kPerfSize - perf_off)) {
        ++perf_counters_.write_ops;
        perf_counters_.write_bytes += size;
        perf_mailbox_->write(perf_off, value, size);
        return true;
      }
    }

    if (kGicBase <= addr && addr < kGicBase + kGicSize) {
      const std::uint64_t gic_off = addr - kGicBase;
      if (size <= (kGicSize - gic_off)) {
        ++perf_counters_.write_ops;
        perf_counters_.write_bytes += size;
        gic_->write(gic_off, value, size);
        return true;
      }
    }

    if (kTimerBase <= addr && addr < kTimerBase + kTimerSize) {
      const std::uint64_t timer_off = addr - kTimerBase;
      if (size <= (kTimerSize - timer_off)) {
        ++perf_counters_.write_ops;
        perf_counters_.write_bytes += size;
        timer_->write(timer_off, value, size);
        return true;
      }
    }

    return false;
  }

  [[nodiscard]] const std::uint8_t* ram_ptr(std::uint64_t addr, std::size_t size) const {
    if (likely(kSdramBase <= addr && addr < kSdramBase + kSdramSize)) {
      const std::uint64_t sdram_off = addr - kSdramBase;
      if (size <= (kSdramSize - sdram_off)) {
        return sdram_bytes_ + static_cast<std::size_t>(sdram_off);
      }
      return nullptr;
    }
    if (kBootRamBase <= addr && addr < kBootRamBase + kBootRamSize) {
      const std::uint64_t boot_off = addr - kBootRamBase;
      if (size <= (kBootRamSize - boot_off)) {
        return boot_ram_bytes_ + static_cast<std::size_t>(boot_off);
      }
      return nullptr;
    }
    return nullptr;
  }

  [[nodiscard]] std::uint8_t* ram_mut_ptr(std::uint64_t addr, std::size_t size) const {
    if (kBootRamBase <= addr && addr < kBootRamBase + kBootRamSize) {
      const std::uint64_t boot_off = addr - kBootRamBase;
      if (size <= (kBootRamSize - boot_off)) {
        return boot_ram_mutable_ + static_cast<std::size_t>(boot_off);
      }
      return nullptr;
    }
    if (kSdramBase <= addr && addr < kSdramBase + kSdramSize) {
      const std::uint64_t sdram_off = addr - kSdramBase;
      if (size <= (kSdramSize - sdram_off)) {
        return sdram_mutable_ + static_cast<std::size_t>(sdram_off);
      }
      return nullptr;
    }
    return nullptr;
  }

  [[nodiscard]] bool read_ram_only(std::uint64_t addr, std::size_t size, std::uint64_t& value) const {
    if (const std::uint8_t* base = ram_ptr(addr, size); base != nullptr) {
      if (load_ram(base, size, value)) {
        ++perf_counters_.read_ops;
        perf_counters_.read_bytes += size;
        return true;
      }
      return false;
    }

    return false;
  }

  [[nodiscard]] bool write_ram_only(std::uint64_t addr, std::uint64_t value, std::size_t size) const {
    if (std::uint8_t* base = ram_mut_ptr(addr, size); base != nullptr) {
      if (store_ram(base, value, size)) {
        ++perf_counters_.write_ops;
        perf_counters_.write_bytes += size;
        return true;
      }
      return false;
    }

    return false;
  }

  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() const { perf_counters_ = {}; }

private:
  static bool load_ram(const std::uint8_t* base, std::size_t size, std::uint64_t& value) {
    switch (size) {
      case 1u:
        value = base[0];
        return true;
      case 2u: {
        std::uint16_t v;
        std::memcpy(&v, base, sizeof(v));
        value = v;
        return true;
      }
      case 4u: {
        std::uint32_t v;
        std::memcpy(&v, base, sizeof(v));
        value = v;
        return true;
      }
      case 8u:
        std::memcpy(&value, base, sizeof(value));
        return true;
      default:
        value = 0;
        return false;
    }
  }

  static bool store_ram(std::uint8_t* base, std::uint64_t value, std::size_t size) {
    switch (size) {
      case 1u:
        base[0] = static_cast<std::uint8_t>(value);
        return true;
      case 2u: {
        const std::uint16_t v = static_cast<std::uint16_t>(value);
        std::memcpy(base, &v, sizeof(v));
        return true;
      }
      case 4u: {
        const std::uint32_t v = static_cast<std::uint32_t>(value);
        std::memcpy(base, &v, sizeof(v));
        return true;
      }
      case 8u:
        std::memcpy(base, &value, sizeof(value));
        return true;
      default:
        return false;
    }
  }

  const std::uint8_t* boot_ram_bytes_ = nullptr;
  std::uint8_t* boot_ram_mutable_ = nullptr;
  const std::uint8_t* sdram_bytes_ = nullptr;
  std::uint8_t* sdram_mutable_ = nullptr;
  UartPl011* uart_ = nullptr;
  PerfMailbox* perf_mailbox_ = nullptr;
  GicV3* gic_ = nullptr;
  GenericTimer* timer_ = nullptr;
  mutable PerfCounters perf_counters_{};
};

} // namespace aarchvm
