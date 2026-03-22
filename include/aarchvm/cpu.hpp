#pragma once

#include "aarchvm/bus.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"
#include "aarchvm/perf_types.hpp"
#include "aarchvm/system_registers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <unordered_map>

namespace aarchvm {

class Cpu {
public:
  struct Callbacks {
    std::function<void(Cpu&)> sev_broadcast;
    std::function<void(Cpu&, std::uint64_t, std::size_t)> memory_write;
    std::function<void(Cpu&)> tlbi_vmalle1_broadcast;
    std::function<void(Cpu&, std::uint64_t, bool)> tlbi_vae1_broadcast;
    std::function<void(Cpu&, std::uint16_t)> tlbi_aside1_broadcast;
    std::function<void(Cpu&)> ic_ivau_broadcast;
    std::function<bool(Cpu&, bool, std::uint16_t)> smccc_call;
    std::function<std::uint64_t()> time_steps;
  };

  explicit Cpu(Bus& bus, GicV3& gic, GenericTimer& timer);

  bool step();

  void reset(std::uint64_t pc);
  void set_callbacks(Callbacks callbacks) { callbacks_ = std::move(callbacks); }
  void set_predecode_enabled(bool enabled);
  [[nodiscard]] bool predecode_enabled() const { return predecode_enabled_; }
  void invalidate_decode_all();
  void set_cntvct(std::uint64_t value);
  void set_sp(std::uint64_t value);
  void set_cpu_index(std::size_t value) { cpu_index_ = value; }
  void set_mpidr(std::uint64_t value) { sysregs_.set_mpidr_el1(value); }
  void set_x(std::uint32_t idx, std::uint64_t value) {
    if (idx < 32) {
      regs_[idx] = value;
    }
  }
  [[nodiscard]] bool halted() const { return halted_; }
  void clear_halt() { halted_ = false; }
  [[nodiscard]] std::uint64_t pc() const { return pc_; }
  [[nodiscard]] std::uint64_t steps() const { return steps_; }
  [[nodiscard]] std::uint64_t x(std::uint32_t idx) const { return reg(idx); }
  [[nodiscard]] std::uint64_t sp() const { return regs_[31]; }
  [[nodiscard]] std::size_t cpu_index() const { return cpu_index_; }
  [[nodiscard]] std::uint64_t mpidr_el1() const { return sysregs_.mpidr_el1(); }
  [[nodiscard]] std::uint64_t pstate_bits() const { return sysregs_.pstate_bits(); }
  [[nodiscard]] std::uint64_t icc_igrpen1_el1() const { return icc_igrpen1_el1_; }
  [[nodiscard]] std::uint32_t exception_depth() const { return exception_depth_; }
  [[nodiscard]] bool waiting_for_interrupt() const { return waiting_for_interrupt_; }
  [[nodiscard]] bool waiting_for_event() const { return waiting_for_event_; }
  [[nodiscard]] bool waiting() const { return waiting_for_interrupt_ || waiting_for_event_; }
  [[nodiscard]] std::uint64_t vbar_el1() const { return sysregs_.vbar_el1(); }
  [[nodiscard]] bool irq_masked() const { return sysregs_.irq_masked(); }
  [[nodiscard]] bool save_state(std::ostream& out) const;
  [[nodiscard]] bool load_state(std::istream& in, std::uint32_t version = 3);
  bool ready_to_run();
  void signal_event();
  void notify_external_memory_write(std::uint64_t pa, std::size_t size);
  void notify_tlbi_vmalle1();
  void notify_tlbi_vae1(std::uint64_t operand, bool all_asids);
  void notify_tlbi_aside1(std::uint16_t asid);
  void notify_ic_ivau();

  void perf_flush_tlb_all();
  [[nodiscard]] const PerfCounters& perf_counters() const { return perf_counters_; }
  void reset_perf_counters() { perf_counters_ = {}; }

private:
  enum class AccessType {
    Fetch,
    Read,
    Write,
    UnprivilegedRead,
    UnprivilegedWrite,
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
    bool user_no_access = false;
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
    std::uint16_t asid = 0;
    std::uint8_t level = 3;
    std::uint8_t attr_index = 0;
    std::uint8_t mair_attr = 0;
    bool writable = false;
    bool user_accessible = false;
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
    std::uint16_t asid = 0;
    std::uint8_t level = 3;
    std::uint8_t attr_index = 0;
    std::uint8_t mair_attr = 0;
    bool writable = false;
    bool user_accessible = false;
    bool executable = true;
    bool pxn = false;
    bool uxn = false;
    MemoryType memory_type = MemoryType::Unknown;
    Shareability leaf_shareability = Shareability::NonShareable;
    WalkAttributes walk_attrs{};
  };

  enum class DecodedKind : std::uint8_t {
    Fallback,
    BImm,
    BCond,
    Cbz,
    Tbz,
    MovWide,
    AddSubImm,
    AddSubShifted,
    LoadStoreUImm,
    LoadStore,
    LogicalShifted,
    LogicalImm,
    Bitfield,
  };

  struct DecodedInsn {
    DecodedKind kind = DecodedKind::Fallback;
    std::int32_t simm = 0;
    std::uint32_t imm = 0;
    std::uint8_t rd = 31;
    std::uint8_t rn = 31;
    std::uint8_t rm = 31;
    std::uint8_t aux = 0;
    std::uint8_t aux2 = 0;
    std::uint16_t flags = 0;
  };

  static constexpr std::size_t kDecodePageBytes = 4096;
  static constexpr std::size_t kDecodePageSlots = kDecodePageBytes / 4;
  static constexpr std::size_t kDecodePageValidWords = kDecodePageSlots / 64;
  static constexpr std::size_t kDecodeCachePages = 64;

  struct DecodePage {
    bool valid = false;
    std::uint64_t context_epoch = 0;
    std::uint64_t va_page = 0;
    std::uint64_t pa_page = 0;
    std::array<std::uint32_t, kDecodePageSlots> raws{};
    std::array<std::uint64_t, kDecodePageValidWords> valid_words{};
    std::array<DecodedInsn, kDecodePageSlots> insns{};
  };

  bool try_take_irq();
  void enter_sync_exception(std::uint64_t fault_pc, std::uint32_t ec, std::uint32_t iss, bool far_valid, std::uint64_t far);
  [[nodiscard]] bool translate_address(std::uint64_t va,
                                     AccessType access,
                                     TranslationResult* out_result,
                                     bool allow_tlb_fill,
                                     bool use_tlb = true,
                                     bool apply_pan = true);
  [[nodiscard]] bool translate_cache_maintenance_address(std::uint64_t va,
                                                         TranslationResult* out_result,
                                                         bool fault_on_el0_no_read_permission,
                                                         bool allow_tlb_fill = true,
                                                         bool use_tlb = true);
  [[nodiscard]] bool walk_page_tables(std::uint64_t va,
                                  AccessType access,
                                  TranslationResult* out_result,
                                  TranslationFault* fault,
                                  bool check_permissions = true,
                                  bool apply_pan = true);
  [[nodiscard]] bool access_permitted(const TranslationResult& result,
                                      AccessType access,
                                      std::uint8_t level,
                                      TranslationFault* fault,
                                      bool apply_pan = true) const {
    const auto permission_fault = [&](bool write) {
      if (fault != nullptr) {
        *fault = TranslationFault{.kind = TranslationFault::Kind::Permission, .level = level, .write = write};
      }
      return false;
    };
    const bool write = access_is_write(access);
    // FEAT_UAO is not implemented in this model, so load/store unprivileged
    // instructions always use EL0 permission checks when executed above EL0.
    const bool use_el0_permissions = sysregs_.in_el0() || access_is_unprivileged(access);
    if (use_el0_permissions && !result.user_accessible) {
      return permission_fault(write);
    }
    if (apply_pan && !use_el0_permissions && access != AccessType::Fetch && sysregs_.pan() && result.user_accessible) {
      return permission_fault(write);
    }
    if (write && !result.writable) {
      return permission_fault(true);
    }
    if (access == AccessType::Fetch) {
      const bool execute_blocked = use_el0_permissions ? result.uxn : result.pxn;
      if (execute_blocked) {
        return permission_fault(false);
      }
    }
    return true;
  }
  [[nodiscard]] std::uint32_t fault_status_code(const TranslationFault& fault) const;
  [[nodiscard]] std::uint32_t data_abort_iss(const TranslationFault& fault,
                                             bool cache_maintenance_or_translation) const;
  void set_par_el1_for_fault(const TranslationFault& fault);
  [[nodiscard]] WalkAttributes decode_walk_attributes(bool va_upper) const;
  [[nodiscard]] MemoryType decode_memory_type(std::uint8_t mair_attr) const;
  [[nodiscard]] std::uint8_t decode_ips_bits() const;
  [[nodiscard]] bool pa_within_ips(std::uint64_t pa, std::uint8_t ips_bits) const;
  void tlb_flush_all();
  void tlb_flush_va(std::uint64_t va);
  void tlb_flush_asid(std::uint16_t asid);
  [[nodiscard]] static constexpr std::uint64_t tlb_page_mask() { return (1ull << 44u) - 1ull; }
  [[nodiscard]] static constexpr std::size_t tlb_set_index(std::uint64_t va_page) { return static_cast<std::size_t>(va_page) & (kTlbSets - 1u); }
  [[nodiscard]] std::uint16_t mmu_asid_mask() const;
  [[nodiscard]] std::uint16_t ttbr_asid(std::uint64_t ttbr) const;
  [[nodiscard]] std::uint16_t current_translation_asid(bool va_upper) const;
  [[nodiscard]] std::uint16_t tlbi_operand_asid(std::uint64_t operand) const;
  [[nodiscard]] const TlbEntry* tlb_lookup(std::uint64_t va_page, std::uint16_t asid) const {
    const auto& set = tlb_entries_[tlb_set_index(va_page)];
    for (const TlbEntry& entry : set) {
      if (entry.valid && entry.va_page == va_page && entry.asid == asid) {
        return &entry;
      }
    }
    return nullptr;
  }
  void tlb_insert(std::uint64_t va_page, const TranslationResult& result);
  void tlb_insert_entry(std::uint64_t va_page, const TlbEntry& entry);
  void tlb_invalidate_page(std::uint64_t va_page, std::uint16_t asid, bool match_asid);
  void data_abort(std::uint64_t va, bool cache_maintenance_or_translation = false);
  [[nodiscard]] bool alignment_check_enabled() const;
  [[nodiscard]] bool sp_alignment_check_enabled() const;
  [[nodiscard]] bool maybe_take_sp_alignment_fault(std::uint32_t base_reg);
  [[nodiscard]] bool maybe_take_data_alignment_fault(std::uint64_t addr,
                                                     std::size_t align,
                                                     AccessType access,
                                                     bool force_check = false);
  [[nodiscard]] static constexpr bool access_is_write(AccessType access) {
    return access == AccessType::Write || access == AccessType::UnprivilegedWrite;
  }
  [[nodiscard]] static constexpr bool access_is_unprivileged(AccessType access) {
    return access == AccessType::UnprivilegedRead || access == AccessType::UnprivilegedWrite;
  }
  [[nodiscard]] bool translate_data_address_fast(std::uint64_t va, bool write, std::uint64_t* out_pa);
  [[nodiscard]] bool translate_data_address_fast(std::uint64_t va, AccessType access, std::uint64_t* out_pa);
  void invalidate_ram_page_caches();
  [[nodiscard]] bool mmu_read_value(std::uint64_t va, std::size_t size, std::uint64_t* out);
  [[nodiscard]] bool mmu_read_value(std::uint64_t va, std::size_t size, std::uint64_t* out, AccessType access);
  [[nodiscard]] bool mmu_write_value(std::uint64_t va, std::uint64_t value, std::size_t size);
  [[nodiscard]] bool mmu_write_value(std::uint64_t va, std::uint64_t value, std::size_t size, AccessType access);
  void clear_exclusive_monitor();
  [[nodiscard]] bool capture_exclusive_phys_addrs(std::uint64_t va,
                                                  std::size_t size,
                                                  bool write,
                                                  std::array<std::uint64_t, 16>* out_pas);
  void set_exclusive_monitor(std::uint64_t va, std::size_t size);
  [[nodiscard]] bool check_exclusive_monitor(std::uint64_t va, std::size_t size, bool* matched);
  [[nodiscard]] bool exclusive_monitor_overlaps(std::uint64_t pa, std::size_t size) const;
  [[nodiscard]] DecodedInsn decode_insn(std::uint32_t insn) const;
  [[nodiscard]] const DecodedInsn* lookup_decoded(std::uint64_t va, std::uint64_t pa, std::uint32_t insn);
  bool exec_decoded_add_sub_imm(const DecodedInsn& decoded);
  bool exec_decoded_add_sub_shifted(const DecodedInsn& decoded);
  bool exec_decoded_logical_shifted(const DecodedInsn& decoded);
  bool exec_decoded_logical_imm(const DecodedInsn& decoded);
  bool exec_decoded_bitfield(const DecodedInsn& decoded);
  bool exec_decoded_load_store_uimm(const DecodedInsn& decoded);
  bool exec_decoded(const DecodedInsn& decoded);
  void invalidate_decode_va(std::uint64_t va, std::size_t size);
  void invalidate_decode_pa(std::uint64_t pa, std::size_t size);
  void invalidate_decode_va_page(std::uint64_t va_page);
  void invalidate_decode_pa_page(std::uint64_t pa_page);
  void invalidate_decode_tlb_context();
  [[nodiscard]] bool fp_asimd_traps_enabled_for_current_el() const;
  [[nodiscard]] bool insn_uses_fp_asimd(std::uint32_t insn) const;
  void trap_fp_asimd_access();
  bool irq_wakeup_ready();
  void on_code_write(std::uint64_t va, std::uint64_t pa, std::size_t size);
  void parse_pc_watch_list();
  void save_current_sp_to_bank();
  void load_current_sp_from_bank();
  [[nodiscard]] std::uint64_t shared_timer_steps() const;
  void refresh_local_timer_irq_lines();
  [[nodiscard]] std::uint16_t compute_irq_threshold() const {
    return std::min<std::uint16_t>(static_cast<std::uint16_t>(icc_pmr_el1_ & 0xFFu), running_priority_);
  }
  void refresh_irq_threshold_cache() { irq_delivery_threshold_ = compute_irq_threshold(); }
  [[nodiscard]] bool should_try_irq() const {
    if (sysregs_.irq_masked() || icc_igrpen1_el1_ == 0) {
      return false;
    }
    return !(irq_query_negative_valid_ &&
             irq_query_epoch_ == gic_.state_epoch() &&
             irq_query_threshold_ == irq_delivery_threshold_);
  }

  [[nodiscard]] std::uint64_t reg(std::uint32_t idx) const {
    if (idx >= 31) {
      return 0;
    }
    return regs_[idx];
  }
  [[nodiscard]] std::uint32_t reg32(std::uint32_t idx) const {
    return static_cast<std::uint32_t>(reg(idx) & 0xFFFFFFFFu);
  }
  void set_reg(std::uint32_t idx, std::uint64_t value) {
    if (idx >= 31) {
      return;
    }
    regs_[idx] = value;
  }
  void set_reg32(std::uint32_t idx, std::uint32_t value) {
    set_reg(idx, value);
  }
  [[nodiscard]] std::uint64_t sp_or_reg(std::uint32_t idx) const {
    if (idx == 31) {
      return regs_[31];
    }
    return reg(idx);
  }
  void set_sp_or_reg(std::uint32_t idx, std::uint64_t value, bool is_32bit) {
    if (idx == 31) {
      regs_[31] = is_32bit ? static_cast<std::uint32_t>(value) : value;
      return;
    }
    if (is_32bit) {
      set_reg32(idx, static_cast<std::uint32_t>(value));
    } else {
      set_reg(idx, value);
    }
  }
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
  static constexpr std::size_t kExceptionStackCapacity = 64;

  Bus& bus_;
  GicV3& gic_;
  GenericTimer& timer_;
  std::array<std::uint64_t, 32> regs_{};
  std::array<std::array<std::uint64_t, 2>, 32> qregs_{};
  SystemRegisters sysregs_{};
  std::uint32_t exception_depth_ = 0;
  std::array<bool, kExceptionStackCapacity> exception_is_irq_stack_{};
  std::array<std::uint32_t, kExceptionStackCapacity> exception_intid_stack_{};
  std::array<std::uint16_t, kExceptionStackCapacity> exception_prev_prio_stack_{};
  std::array<bool, kExceptionStackCapacity> exception_prio_dropped_stack_{};
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
  std::uint8_t exclusive_pa_count_ = 0;
  std::array<std::uint64_t, 16> exclusive_phys_addrs_{};
  std::array<std::array<TlbEntry, kTlbWays>, kTlbSets> tlb_entries_{};
  std::array<std::uint8_t, kTlbSets> tlb_next_replace_{};
  TlbEntry tlb_last_fetch_{};
  TlbEntry tlb_last_data_{};
  PerfCounters perf_counters_{};
  std::optional<TranslationFault> last_translation_fault_;
  std::optional<std::uint64_t> last_data_fault_va_;
  std::uint64_t pc_ = 0;
  std::uint64_t steps_ = 0;
  std::uint64_t irq_query_epoch_ = 0;
  std::uint16_t irq_query_threshold_ = 0;
  std::uint16_t irq_delivery_threshold_ = 0xFF;
  bool irq_query_negative_valid_ = false;
  bool halted_ = false;
  bool predecode_enabled_ = true;
  std::size_t cpu_index_ = 0;
  Callbacks callbacks_{};
  std::uint64_t decode_context_epoch_ = 1;
  std::array<DecodePage, kDecodeCachePages> decode_pages_{};
  DecodePage* decode_last_page_ = nullptr;
};

} // namespace aarchvm
