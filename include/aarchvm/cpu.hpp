#pragma once

#include "aarchvm/bus.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"
#include "aarchvm/perf_types.hpp"
#include "aarchvm/system_registers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>

namespace aarchvm {

class Cpu {
public:
  explicit Cpu(Bus& bus, GicV3& gic, GenericTimer& timer);

  bool step();

  void reset(std::uint64_t pc);
  void set_cntvct(std::uint64_t value);
  void set_sp(std::uint64_t value);
  void set_x(std::uint32_t idx, std::uint64_t value) {
    if (idx < 32) {
      regs_[idx] = value;
    }
  }
  [[nodiscard]] bool halted() const { return halted_; }
  [[nodiscard]] std::uint64_t pc() const { return pc_; }
  [[nodiscard]] std::uint64_t steps() const { return steps_; }
  [[nodiscard]] std::uint64_t x(std::uint32_t idx) const { return reg(idx); }
  [[nodiscard]] std::uint64_t sp() const { return regs_[31]; }
  [[nodiscard]] std::uint64_t pstate_bits() const { return sysregs_.pstate_bits(); }
  [[nodiscard]] std::uint64_t icc_igrpen1_el1() const { return icc_igrpen1_el1_; }
  [[nodiscard]] std::uint32_t exception_depth() const { return exception_depth_; }
  [[nodiscard]] bool waiting_for_interrupt() const { return waiting_for_interrupt_; }
  [[nodiscard]] bool waiting_for_event() const { return waiting_for_event_; }
  [[nodiscard]] std::uint64_t vbar_el1() const { return sysregs_.vbar_el1(); }
  [[nodiscard]] bool irq_masked() const { return sysregs_.irq_masked(); }
  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 3);

  void perf_flush_tlb_all();
  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() { perf_counters_ = {}; }

private:
  enum class AccessType {
    Fetch,
    Read,
    Write,
  };

  enum class Shareability : std::uint8_t {
    NonShareable = 0,
    Reserved = 1,
    OuterShareable = 2,
    InnerShareable = 3,
  };

  enum class Cacheability : std::uint8_t {
    NonCacheable = 0,
    WriteBackReadWriteAllocate = 1,
    WriteThroughReadAllocate = 2,
    WriteBackReadAllocate = 3,
  };

  enum class MemoryType : std::uint8_t {
    Device,
    Normal,
    Unknown,
  };

  struct TranslationFault {
    enum class Kind {
      Translation,
      AddressSize,
      AccessFlag,
      Permission,
    };

    Kind kind = Kind::Translation;
    std::uint8_t level = 0;
    bool write = false;
  };

  struct TableAttrs {
    bool write_protect = false;
    bool pxn = false;
    bool uxn = false;
  };

  struct WalkAttributes {
    Cacheability inner = Cacheability::NonCacheable;
    Cacheability outer = Cacheability::NonCacheable;
    Shareability shareability = Shareability::NonShareable;
    std::uint8_t ips_bits = 32;
  };

  struct TranslationResult {
    std::uint64_t pa = 0;
    std::uint8_t level = 3;
    std::uint8_t attr_index = 0;
    std::uint8_t mair_attr = 0;
    bool writable = false;
    bool executable = true;
    bool pxn = false;
    bool uxn = false;
    MemoryType memory_type = MemoryType::Unknown;
    Shareability leaf_shareability = Shareability::NonShareable;
    WalkAttributes walk_attrs{};
  };

  struct TlbEntry {
    bool valid = false;
    std::uint64_t va_page = 0;
    std::uint64_t pa_page = 0;
    std::uint8_t level = 3;
    std::uint8_t attr_index = 0;
    std::uint8_t mair_attr = 0;
    bool writable = false;
    bool executable = true;
    bool pxn = false;
    bool uxn = false;
    MemoryType memory_type = MemoryType::Unknown;
    Shareability leaf_shareability = Shareability::NonShareable;
    WalkAttributes walk_attrs{};
  };

  bool try_take_irq();
  void enter_sync_exception(std::uint64_t fault_pc, std::uint32_t ec, std::uint32_t iss, bool far_valid, std::uint64_t far);
  [[nodiscard]] std::optional<TranslationResult> translate_address(std::uint64_t va,
                                                                   AccessType access,
                                                                   bool allow_tlb_fill,
                                                                   bool use_tlb = true);
  [[nodiscard]] std::optional<TranslationResult> walk_page_tables(std::uint64_t va,
                                                                  AccessType access,
                                                                  TranslationFault* fault);
  [[nodiscard]] bool access_permitted(const TranslationResult& result,
                                      AccessType access,
                                      std::uint8_t level,
                                      TranslationFault* fault) const;
  [[nodiscard]] std::uint32_t fault_status_code(const TranslationFault& fault) const;
  void set_par_el1_for_fault(const TranslationFault& fault);
  [[nodiscard]] WalkAttributes decode_walk_attributes(bool va_upper) const;
  [[nodiscard]] MemoryType decode_memory_type(std::uint8_t mair_attr) const;
  [[nodiscard]] std::uint8_t decode_ips_bits() const;
  [[nodiscard]] bool pa_within_ips(std::uint64_t pa, std::uint8_t ips_bits) const;
  void tlb_flush_all();
  void tlb_flush_va(std::uint64_t va);
  [[nodiscard]] static constexpr std::uint64_t tlb_page_mask() { return (1ull << 44u) - 1ull; }
  [[nodiscard]] static constexpr std::size_t tlb_set_index(std::uint64_t va_page) { return static_cast<std::size_t>(va_page) & (kTlbSets - 1u); }
  [[nodiscard]] const TlbEntry* tlb_lookup(std::uint64_t va_page) const;
  void tlb_insert(std::uint64_t va_page, const TranslationResult& result);
  void tlb_insert_entry(std::uint64_t va_page, const TlbEntry& entry);
  void tlb_invalidate_page(std::uint64_t va_page);
  void parse_pc_watch_list();
  void save_current_sp_to_bank();
  void load_current_sp_from_bank();

  [[nodiscard]] std::uint64_t reg(std::uint32_t idx) const;
  [[nodiscard]] std::uint32_t reg32(std::uint32_t idx) const;
  void set_reg(std::uint32_t idx, std::uint64_t value);
  void set_reg32(std::uint32_t idx, std::uint32_t value);
  [[nodiscard]] std::uint64_t sp_or_reg(std::uint32_t idx) const;
  void set_sp_or_reg(std::uint32_t idx, std::uint64_t value, bool is_32bit);
  [[nodiscard]] std::uint64_t q_reg_lane(std::uint32_t idx, std::size_t lane) const;
  void set_q_reg_lane(std::uint32_t idx, std::size_t lane, std::uint64_t value);

  bool exec_branch(std::uint32_t insn);
  bool exec_system(std::uint32_t insn);
  bool exec_data_processing(std::uint32_t insn);
  bool exec_load_store(std::uint32_t insn);
  bool condition_holds(std::uint32_t cond) const;
  void set_flags_add(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, bool is_32bit);
  void set_flags_sub(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t result, bool is_32bit);
  void set_flags_logic(std::uint64_t result, bool is_32bit);

  static std::int64_t sign_extend(std::uint64_t value, std::uint32_t bits);

  static constexpr std::size_t kTlbEntries = 4096;
  static constexpr std::size_t kTlbWays = 4;
  static constexpr std::size_t kTlbSets = kTlbEntries / kTlbWays;

  Bus& bus_;
  GicV3& gic_;
  GenericTimer& timer_;
  std::array<std::uint64_t, 32> regs_{};
  std::array<std::array<std::uint64_t, 2>, 32> qregs_{};
  SystemRegisters sysregs_{};
  std::uint32_t exception_depth_ = 0;
  std::array<bool, 8> exception_is_irq_stack_{};
  std::array<std::uint32_t, 8> exception_intid_stack_{};
  std::array<std::uint16_t, 8> exception_prev_prio_stack_{};
  std::array<bool, 8> exception_prio_dropped_stack_{};
  bool sync_reported_ = false;
  bool trace_exceptions_ = false;
  bool trace_all_exceptions_ = false;
  bool trace_brk_ = false;
  std::optional<std::uint64_t> trace_va_;
  bool trace_va_hit_ = false;
  std::uint64_t trace_svc_limit_ = 0;
  std::uint64_t trace_svc_count_ = 0;
  std::uint64_t trace_eret_lower_limit_ = 0;
  std::uint64_t trace_eret_lower_count_ = 0;
  std::uint64_t trace_lower_sync_limit_ = 0;
  std::uint64_t trace_lower_sync_count_ = 0;
  std::unordered_map<std::uint64_t, bool> pc_watch_hits_;
  bool pc_watch_enabled_ = false;
  bool waiting_for_interrupt_ = false;
  bool waiting_for_event_ = false;
  bool event_register_ = false;
  std::uint64_t icc_pmr_el1_ = 0xFF;
  std::uint16_t running_priority_ = 0x100;
  std::uint64_t icc_ctlr_el1_ = 0;
  std::uint64_t icc_sre_el1_ = 0;
  std::uint64_t icc_bpr1_el1_ = 0;
  std::uint64_t icc_igrpen1_el1_ = 0;
  std::array<std::uint64_t, 4> icc_ap0r_el1_{};
  std::array<std::uint64_t, 4> icc_ap1r_el1_{};
  bool exclusive_valid_ = false;
  std::uint64_t exclusive_addr_ = 0;
  std::uint8_t exclusive_size_ = 0;
  std::array<std::array<TlbEntry, kTlbWays>, kTlbSets> tlb_entries_{};
  std::array<std::uint8_t, kTlbSets> tlb_next_replace_{};
  TlbEntry tlb_last_fetch_{};
  TlbEntry tlb_last_data_{};
  PerfCounters perf_counters_{};
  std::optional<TranslationFault> last_translation_fault_;
  std::uint64_t pc_ = 0;
  std::uint64_t steps_ = 0;
  bool halted_ = false;
};

} // namespace aarchvm
