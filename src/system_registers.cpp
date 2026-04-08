#include "aarchvm/system_registers.hpp"

#include "aarchvm/snapshot_io.hpp"

namespace aarchvm {

namespace {

constexpr std::uint32_t SysReg(std::uint32_t op0,
                               std::uint32_t op1,
                               std::uint32_t crn,
                               std::uint32_t crm,
                               std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}

constexpr std::uint64_t kSctlrEl1Span = 1ull << 23;
constexpr std::uint64_t kSctlrEl1Ee = 1ull << 25;
constexpr std::uint64_t kSctlrEl1E0e = 1ull << 24;
constexpr std::uint64_t kSctlrEl1Dze = 1ull << 14;
constexpr std::uint64_t kSctlrEl1Uct = 1ull << 15;
constexpr std::uint64_t kSctlrEl1Uma = 1ull << 9;
constexpr std::uint64_t kSctlrEl1NTwi = 1ull << 16;
constexpr std::uint64_t kSctlrEl1NTwe = 1ull << 18;
constexpr std::uint64_t kSctlrEl1Uci = 1ull << 26;
constexpr std::uint64_t kSctlrEl1BaseWritableMask =
    kSctlrEl1Uci |
    kSctlrEl1Span |
    kSctlrEl1NTwe |
    kSctlrEl1NTwi |
    kSctlrEl1Uct |
    kSctlrEl1Dze |
    (1ull << 19) | // WXN
    (1ull << 12) | // I
    kSctlrEl1Uma |
    (1ull << 4)  | // SA0
    (1ull << 3)  | // SA
    (1ull << 2)  | // C
    (1ull << 1)  | // A
    (1ull << 0);   // M
constexpr std::uint64_t kSctlrEl1Aa32Res1Mask =
    (1ull << 8) | // SED
    (1ull << 7);  // ITD
constexpr std::uint64_t kSctlrEl1LsmaocMask =
    (1ull << 29) | // LSMAOE
    (1ull << 28);  // nTLSMD
constexpr std::uint64_t kSctlrEl1PauthMask =
    (1ull << 31) | // EnIA
    (1ull << 30) | // EnIB
    (1ull << 27) | // EnDA
    (1ull << 13);  // EnDB
constexpr std::uint64_t kSctlrEl1Mte2Mask =
    (1ull << 43) | // ATA
    (1ull << 42) | // ATA0
    (0x3ull << 40) | // TCF
    (0x3ull << 38);  // TCF0
constexpr std::uint64_t kMdscrEl1Kde = 1ull << 13;
constexpr std::uint64_t kMdscrEl1Mde = 1ull << 15;
constexpr std::uint64_t kMdscrEl1Hde = 1ull << 14;
constexpr std::uint64_t kMdscrEl1Tdcc = 1ull << 12;
constexpr std::uint64_t kMdscrEl1Err = 1ull << 6;
constexpr std::uint64_t kMdscrEl1Ss = 1ull << 0;
constexpr std::uint64_t kMdscrEl1IntdisMask = 0x3ull << 22;
constexpr std::uint64_t kMdscrEl1Tda = 1ull << 21;
constexpr std::uint64_t kMdscrEl1Txu = 1ull << 26;
constexpr std::uint64_t kMdscrEl1Rxo = 1ull << 27;
constexpr std::uint64_t kMdscrEl1Txfull = 1ull << 29;
constexpr std::uint64_t kMdscrEl1Rxfull = 1ull << 30;
constexpr std::uint64_t kMdscrEl1DirectRwMask =
    kMdscrEl1Mde | kMdscrEl1Kde | kMdscrEl1Tdcc | kMdscrEl1Ss;
constexpr std::uint64_t kMdscrEl1OsLockRwMask =
    kMdscrEl1Rxfull | kMdscrEl1Txfull | kMdscrEl1Rxo | kMdscrEl1Txu |
    kMdscrEl1IntdisMask | kMdscrEl1Tda | kMdscrEl1Hde | kMdscrEl1Err;
constexpr std::uint64_t kMdscrEl1ArchitecturalMask =
    kMdscrEl1DirectRwMask | kMdscrEl1OsLockRwMask;
constexpr std::uint64_t kFpsrArchitecturalMask =
    (1ull << 27) | // QC
    (1ull << 7)  | // IDC
    0x1Full;       // IXC/UFC/OFC/DZC/IOC
constexpr std::uint64_t kFpcrArchitecturalMask =
    (1ull << 26) |  // AHP
    (1ull << 25) |  // DN
    (1ull << 24) |  // FZ
    (0x3ull << 22) | // RMode
    (0x3ull << 20) | // Stride
    (0x7ull << 16);  // Len
constexpr std::uint64_t kCpacrEl1ArchitecturalMask =
    0x3ull << 20;    // FPEN
constexpr std::uint64_t kCsselrEl1ArchitecturalMask =
    0xFull;          // Level[3:1], InD[0], TnD absent.
constexpr std::uint64_t kVbarEl1ArchitecturalMask =
    ~0x7FFull;
constexpr std::uint64_t kContextidrEl1ArchitecturalMask =
    0xFFFFFFFFull;
constexpr std::uint64_t kCntkctlEl1ArchitecturalMask =
    0x3FFull;
constexpr std::uint64_t kTcrEl1BaseArchitecturalMask =
    0xFFFFFFFFull |        // All architected low control fields...
    (0x7ull << 32) |       // ...plus IPS[34:32]...
    (0x3ull << 37);        // ...and TBI1/TBI0.
constexpr std::uint64_t kTcrEl1ReservedMask =
    (0x3ull << 62) |       // RES0
    (0x1ull << 35) |       // RES0
    (0x1ull << 6);         // RES0
constexpr std::uint64_t kTcrEl1MtxMask =
    (0x3ull << 60);
constexpr std::uint64_t kTcrEl1DsMask =
    (0x1ull << 59);
constexpr std::uint64_t kTcrEl1TcmaMask =
    (0x3ull << 57);
constexpr std::uint64_t kTcrEl1E0pdMask =
    (0x3ull << 55);
constexpr std::uint64_t kTcrEl1NfdMask =
    (0x3ull << 53);
constexpr std::uint64_t kTcrEl1TbidMask =
    (0x3ull << 51);
constexpr std::uint64_t kTcrEl1HwuMask =
    (0xFFull << 43);
constexpr std::uint64_t kTcrEl1HpdMask =
    (0x3ull << 41);
constexpr std::uint64_t kTcrEl1HaHdMask =
    (0x3ull << 39);
constexpr std::uint64_t kTcrEl1AsMask =
    (0x1ull << 36);
constexpr std::uint64_t kSpsrEl1SupportedMask =
    (0xFull << 28) | // NZCV
    (1ull << 22) |   // PAN (FEAT_PAN is implemented)
    (1ull << 21) |   // SS
    (1ull << 20) |   // IL
    (0xFull << 6) |  // DAIF
    0x1Full;         // M[4:0]
constexpr std::uint64_t kIdAa64Dfr0El1DebugMinimal =
    (0x0ull << 28) |  // CTX_CMPs: 1 context-aware breakpoint.
    (0x1ull << 20) |  // WRPs: 2 watchpoints.
    (0x1ull << 12) |  // BRPs: 2 breakpoints.
    0x6ull;           // DebugVer: Armv8.0 debug architecture.

constexpr std::uint32_t decode_debug_resource_count(std::uint64_t dfr0,
                                                    unsigned dfr0_shift,
                                                    std::uint64_t dfr1,
                                                    unsigned dfr1_shift) {
  const std::uint32_t extended = static_cast<std::uint32_t>((dfr1 >> dfr1_shift) & 0xFFu);
  if (extended != 0u) {
    return extended + 1u;
  }
  const std::uint32_t legacy = static_cast<std::uint32_t>((dfr0 >> dfr0_shift) & 0xFu);
  return legacy == 0xFu ? 16u : legacy + 1u;
}

std::uint64_t sanitize_sctlr_el1(std::uint64_t value,
                                 std::uint64_t mmfr0,
                                 std::uint64_t mmfr1,
                                 std::uint64_t mmfr2,
                                 std::uint64_t pfr0,
                                 std::uint64_t pfr1,
                                 std::uint64_t isar1,
                                 std::uint64_t isar2) {
  std::uint64_t writable_mask = kSctlrEl1BaseWritableMask;
  std::uint64_t res1_mask = 0;

  const std::uint32_t big_end = static_cast<std::uint32_t>((mmfr0 >> 8) & 0xFu);
  const std::uint32_t big_end_el0 = static_cast<std::uint32_t>((mmfr0 >> 16) & 0xFu);
  const std::uint32_t exs = static_cast<std::uint32_t>((mmfr0 >> 44) & 0xFu);
  const std::uint32_t tidcp1 = static_cast<std::uint32_t>((mmfr1 >> 52) & 0xFu);
  const std::uint32_t cmow = static_cast<std::uint32_t>((mmfr1 >> 56) & 0xFu);
  const std::uint32_t twed = static_cast<std::uint32_t>((mmfr1 >> 32) & 0xFu);
  const std::uint32_t iesb = static_cast<std::uint32_t>((mmfr2 >> 12) & 0xFu);
  const std::uint32_t lsm = static_cast<std::uint32_t>((mmfr2 >> 8) & 0xFu);
  const std::uint32_t lse2 = static_cast<std::uint32_t>((mmfr2 >> 32) & 0xFu);
  const std::uint32_t csv2 = static_cast<std::uint32_t>((pfr0 >> 56) & 0xFu);
  const std::uint32_t el0 = static_cast<std::uint32_t>(pfr0 & 0xFu);
  const std::uint32_t csv2_frac = static_cast<std::uint32_t>((pfr1 >> 32) & 0xFu);
  const std::uint32_t nmi = static_cast<std::uint32_t>((pfr1 >> 36) & 0xFu);
  const std::uint32_t mte = static_cast<std::uint32_t>((pfr1 >> 8) & 0xFu);
  const std::uint32_t mte_frac = static_cast<std::uint32_t>((pfr1 >> 40) & 0xFu);
  const std::uint32_t ssbs = static_cast<std::uint32_t>((pfr1 >> 4) & 0xFu);
  const std::uint32_t sme = static_cast<std::uint32_t>((pfr1 >> 24) & 0xFu);
  const std::uint32_t bti = static_cast<std::uint32_t>(pfr1 & 0xFu);
  const std::uint32_t ls64 = static_cast<std::uint32_t>((isar1 >> 60) & 0xFu);
  const std::uint32_t specres = static_cast<std::uint32_t>((isar1 >> 40) & 0xFu);
  const std::uint32_t mops = static_cast<std::uint32_t>((isar2 >> 16) & 0xFu);
  const bool pauth =
      (((isar1 >> 4) & 0xFu) != 0u) ||   // APA
      (((isar1 >> 8) & 0xFu) != 0u) ||   // API
      (((isar1 >> 24) & 0xFu) != 0u) ||  // GPA
      (((isar1 >> 28) & 0xFu) != 0u) ||  // GPI
      (((isar2 >> 8) & 0xFu) != 0u) ||   // GPA3
      (((isar2 >> 12) & 0xFu) != 0u);    // APA3

  if (tidcp1 != 0u) {
    writable_mask |= 1ull << 63;
  }
  if (nmi != 0u) {
    writable_mask |= (1ull << 62) | (1ull << 61);
  }
  if (sme != 0u) {
    writable_mask |= 1ull << 60;
  }
  if (ls64 != 0u) {
    writable_mask |= 1ull << 56;
  }
  if (ls64 >= 3u) {
    writable_mask |= 1ull << 55;
  }
  if (ls64 >= 2u) {
    writable_mask |= 1ull << 54;
  }
  if (twed != 0u) {
    writable_mask |= (0xFull << 46) | (1ull << 45);
  }
  if (ssbs != 0u) {
    writable_mask |= 1ull << 44;
  }
  if (mte >= 2u) {
    writable_mask |= kSctlrEl1Mte2Mask;
    if (mte_frac == 0u) {
      writable_mask |= 1ull << 37; // ITFSB
    }
  }
  if (bti != 0u) {
    writable_mask |= (1ull << 36) | (1ull << 35);
  }
  if (mops != 0u) {
    writable_mask |= 1ull << 33;
  }
  if (cmow != 0u) {
    writable_mask |= 1ull << 32;
  }
  if (pauth) {
    writable_mask |= kSctlrEl1PauthMask;
  }
  if (lsm != 0u) {
    writable_mask |= kSctlrEl1LsmaocMask;
  } else {
    res1_mask |= kSctlrEl1LsmaocMask;
  }
  if (big_end != 0u) {
    writable_mask |= kSctlrEl1Ee;
  }
  if (big_end_el0 != 0u) {
    writable_mask |= kSctlrEl1E0e;
  }
  if (exs != 0u) {
    writable_mask |= (1ull << 22) | (1ull << 11);
  } else {
    res1_mask |= (1ull << 22) | (1ull << 11);
  }
  if (iesb != 0u) {
    writable_mask |= 1ull << 21;
  }
  if (csv2 >= 2u || (csv2 == 1u && csv2_frac >= 2u)) {
    writable_mask |= 1ull << 20;
  } else {
    res1_mask |= 1ull << 20;
  }
  if (specres != 0u) {
    writable_mask |= 1ull << 10;
  }
  if (el0 == 2u) {
    writable_mask |= kSctlrEl1Aa32Res1Mask;
  } else {
    res1_mask |= kSctlrEl1Aa32Res1Mask;
  }
  if (lse2 != 0u) {
    writable_mask |= 1ull << 6;
  }

  return (value | res1_mask) & (writable_mask | res1_mask);
}

std::uint64_t sanitize_spsr_el1(std::uint64_t value) {
  // This model only supports AArch64 guest execution and currently keeps the
  // optional PSTATE extensions behind UAO/DIT/SSBS/BTI/MTE/NMI/EBEP/SEBEP/GCS/
  // PACM/UINJ absent, so the corresponding SPSR_EL1 bits are RES0.
  return value & kSpsrEl1SupportedMask;
}

std::uint64_t sanitize_mdscr_el1(std::uint64_t value) {
  return value & kMdscrEl1ArchitecturalMask;
}

std::uint64_t sanitize_cpacr_el1(std::uint64_t value) {
  return value & kCpacrEl1ArchitecturalMask;
}

std::uint64_t sanitize_csselr_el1(std::uint64_t value) {
  return value & kCsselrEl1ArchitecturalMask;
}

std::uint64_t sanitize_vbar_el1(std::uint64_t value) {
  return value & kVbarEl1ArchitecturalMask;
}

std::uint64_t sanitize_contextidr_el1(std::uint64_t value) {
  return value & kContextidrEl1ArchitecturalMask;
}

std::uint64_t sanitize_cntkctl_el1(std::uint64_t value) {
  return value & kCntkctlEl1ArchitecturalMask;
}

std::uint64_t sanitize_ttbr_el1(std::uint64_t value,
                                std::uint64_t mmfr0,
                                std::uint64_t mmfr2) {
  const std::uint32_t asid_bits = static_cast<std::uint32_t>((mmfr0 >> 4) & 0xFu);
  const std::uint32_t cnp = static_cast<std::uint32_t>((mmfr2 >> 12) & 0xFu);
  if (asid_bits != 2u) {
    value &= ~(0xFFull << 56);
  }
  if (cnp != 1u) {
    value &= ~0x1ull;
  }
  return value;
}

std::uint64_t sanitize_tcr_el1(std::uint64_t value,
                               std::uint64_t mmfr0,
                               std::uint64_t mmfr1,
                               std::uint64_t mmfr2,
                               std::uint64_t pfr0,
                               std::uint64_t pfr1,
                               std::uint64_t isar1,
                               std::uint64_t isar2) {
  value &= ~kTcrEl1ReservedMask;

  std::uint64_t mask = kTcrEl1BaseArchitecturalMask;
  const std::uint32_t asid_bits = static_cast<std::uint32_t>((mmfr0 >> 4) & 0xFu);
  const std::uint32_t hpds = static_cast<std::uint32_t>((mmfr1 >> 12) & 0xFu);
  const std::uint32_t hafdbs = static_cast<std::uint32_t>(mmfr1 & 0xFu);
  const std::uint32_t e0pd = static_cast<std::uint32_t>((mmfr2 >> 60) & 0xFu);
  const std::uint32_t sve = static_cast<std::uint32_t>((pfr0 >> 32) & 0xFu);
  const std::uint32_t mte = static_cast<std::uint32_t>((pfr1 >> 8) & 0xFu);
  const bool pauth =
      (((isar1 >> 4) & 0xFu) != 0u) ||   // APA
      (((isar1 >> 8) & 0xFu) != 0u) ||   // API
      (((isar1 >> 24) & 0xFu) != 0u) ||  // GPA
      (((isar1 >> 28) & 0xFu) != 0u) ||  // GPI
      (((isar2 >> 8) & 0xFu) != 0u) ||   // GPA3
      (((isar2 >> 12) & 0xFu) != 0u);    // APA3

  if (asid_bits == 2u) {
    mask |= kTcrEl1AsMask;
  }
  if (hafdbs != 0u) {
    mask |= kTcrEl1HaHdMask;
  }
  if (hpds != 0u) {
    mask |= kTcrEl1HpdMask;
  }
  if (hpds >= 2u) {
    mask |= kTcrEl1HwuMask;
  }
  if (pauth) {
    mask |= kTcrEl1TbidMask;
  }
  if (sve != 0u) {
    mask |= kTcrEl1NfdMask;
  }
  if (e0pd != 0u) {
    mask |= kTcrEl1E0pdMask;
  }
  if (mte != 0u) {
    mask |= kTcrEl1TcmaMask | kTcrEl1MtxMask;
  }

  // The current model does not implement FEAT_LPA2, so TCR_EL1.DS is RES0.
  value &= ~kTcrEl1DsMask;
  return value & mask;
}

std::uint64_t merge_mdscr_el1(std::uint64_t current, std::uint64_t value, bool os_lock_locked) {
  const std::uint64_t writable =
      kMdscrEl1Mde | kMdscrEl1Kde | kMdscrEl1Tdcc | kMdscrEl1Ss |
      (os_lock_locked ? kMdscrEl1OsLockRwMask : 0ull);
  const std::uint64_t merged = (current & ~writable) | (value & writable);
  return sanitize_mdscr_el1(merged);
}

std::uint64_t sanitize_dbgprcr_el1(std::uint64_t value) {
  return value & 0x1ull;
}

bool debug_exceptions_enabled_from_el(std::uint8_t from_el,
                                      bool d_masked,
                                      std::uint64_t mdscr_el1) {
  if (from_el == 0u) {
    return true;
  }
  if (from_el == 1u) {
    return (mdscr_el1 & kMdscrEl1Kde) != 0u && !d_masked;
  }
  return false;
}

} // namespace

void SystemRegisters::reset() {
  sctlr_el1_ = 0x30D00800;
  cpacr_el1_ = 0;
  midr_el1_ = 0x00000000410FD034ull;
  mpidr_el1_ = 0x0000000080000000ull;
  revidr_el1_ = 0;
  clidr_el1_ = 0;
  ctr_el0_ = 0x8444C004ull;
  dczid_el0_ = 0x4ull;
  id_aa64pfr0_el1_ = 0x0000000001000011ull;
  id_aa64pfr1_el1_ = 0;
  id_aa64dfr0_el1_ = kIdAa64Dfr0El1DebugMinimal;
  id_aa64dfr1_el1_ = 0;
  id_aa64isar0_el1_ = 0x0000000000210000ull;
  id_aa64isar1_el1_ = 0;
  id_aa64isar2_el1_ = 0;
  id_aa64isar3_el1_ = 0;
  id_aa64zfr0_el1_ = 0;
  id_aa64mmfr0_el1_ = 0x000000000FF00005ull;
  id_aa64mmfr1_el1_ = 0x0000000000100000ull;
  id_aa64mmfr2_el1_ = 0;
  id_aa64mmfr3_el1_ = 0;
  csselr_el1_ = 0;
  ccsidr_el1_ = 0;
  ttbr0_el1_ = 0;
  ttbr1_el1_ = 0;
  tcr_el1_ = 0;
  mair_el1_ = 0;
  contextidr_el1_ = 0;
  osdlr_el1_ = 0;
  oslar_el1_ = 1;
  oseccr_el1_ = 0;
  dbgclaim_el1_ = 0;
  dbgprcr_el1_ = 0;
  dbgbvr_el1_.fill(0);
  dbgbcr_el1_.fill(0);
  dbgwvr_el1_.fill(0);
  dbgwcr_el1_.fill(0);
  vbar_el1_ = 0;
  elr_el1_ = 0;
  spsr_el1_ = 0;
  sp_el0_ = 0;
  sp_el1_ = 0;
  spsel_ = 1;
  par_el1_ = 0;
  esr_el1_ = 0;
  far_el1_ = 0;
  mdscr_el1_ = 0;
  pmuserenr_el0_ = 0;
  amuserenr_el0_ = 0;
  tpidr_el0_ = 0;
  tpidr2_el0_ = 0;
  tpidrro_el0_ = 0;
  tpidr_el1_ = 0;
  tpidr_el2_ = 0;
  cntfrq_el0_ = 100000000;
  cntvct_el0_ = 0;
  cntkctl_el1_ = 0;
  fpcr_ = 0;
  fpsr_ = 0;
  pstate_ = {};
  pstate_.mode = 0x5u;
  sctlr_el1_ = sanitize_sctlr_el1(sctlr_el1_,
                                  id_aa64mmfr0_el1_,
                                  id_aa64mmfr1_el1_,
                                  id_aa64mmfr2_el1_,
                                  id_aa64pfr0_el1_,
                                  id_aa64pfr1_el1_,
                                  id_aa64isar1_el1_,
                                  id_aa64isar2_el1_);
}

void SystemRegisters::set_fpsr(std::uint64_t value) {
  fpsr_ = value & kFpsrArchitecturalMask;
}

void SystemRegisters::set_fpcr(std::uint64_t value) {
  fpcr_ = value & kFpcrArchitecturalMask;
}

void SystemRegisters::fp_or_fpsr(std::uint64_t bits) {
  fpsr_ = (fpsr_ | bits) & kFpsrArchitecturalMask;
}

bool SystemRegisters::read(std::uint32_t op0,
                           std::uint32_t op1,
                           std::uint32_t crn,
                           std::uint32_t crm,
                           std::uint32_t op2,
                           std::uint64_t& value) const {
  if (op0 == 2u && op1 == 0u && crn == 0u && crm < 16u) {
    const std::uint32_t limit =
        (op2 == 4u || op2 == 5u) ? breakpoint_resource_count() :
        (op2 == 6u || op2 == 7u) ? watchpoint_resource_count() : 0u;
    if (limit != 0u && crm >= limit) {
      return false;
    }
    switch (op2) {
    case 4u: value = dbgbvr_el1_[crm]; return true;
    case 5u: value = dbgbcr_el1(crm); return true;
    case 6u: value = dbgwvr_el1_[crm]; return true;
    case 7u: value = dbgwcr_el1_[crm]; return true;
    default: break;
    }
  }
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 0, 0, 0): value = midr_el1_; return true;
  case SysReg(3, 0, 0, 0, 5): value = mpidr_el1_; return true;
  case SysReg(3, 0, 0, 0, 6): value = revidr_el1_; return true;
  case SysReg(3, 1, 0, 0, 0): value = ccsidr_el1_; return true;
  case SysReg(3, 0, 1, 0, 0):
    value = sanitize_sctlr_el1(sctlr_el1_,
                               id_aa64mmfr0_el1_,
                               id_aa64mmfr1_el1_,
                               id_aa64mmfr2_el1_,
                               id_aa64pfr0_el1_,
                               id_aa64pfr1_el1_,
                               id_aa64isar1_el1_,
                               id_aa64isar2_el1_);
    return true;
  case SysReg(3, 0, 1, 0, 2): value = sanitize_cpacr_el1(cpacr_el1_); return true;
  case SysReg(3, 2, 0, 0, 0): value = sanitize_csselr_el1(csselr_el1_); return true;
  case SysReg(3, 0, 2, 0, 0): value = sanitize_ttbr_el1(ttbr0_el1_, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_); return true;
  case SysReg(3, 0, 2, 0, 1): value = sanitize_ttbr_el1(ttbr1_el1_, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_); return true;
  case SysReg(3, 0, 2, 0, 2):
    value = sanitize_tcr_el1(tcr_el1_,
                             id_aa64mmfr0_el1_,
                             id_aa64mmfr1_el1_,
                             id_aa64mmfr2_el1_,
                             id_aa64pfr0_el1_,
                             id_aa64pfr1_el1_,
                             id_aa64isar1_el1_,
                             id_aa64isar2_el1_);
    return true;
  case SysReg(3, 1, 0, 0, 1): value = clidr_el1_; return true;
  case SysReg(3, 0, 0, 4, 0): value = id_aa64pfr0_el1_; return true;
  case SysReg(3, 0, 0, 4, 1): value = id_aa64pfr1_el1_; return true;
  case SysReg(3, 0, 0, 5, 0): value = id_aa64dfr0_el1_; return true;
  case SysReg(3, 0, 0, 5, 1): value = id_aa64dfr1_el1_; return true;
  case SysReg(3, 0, 0, 6, 0): value = id_aa64isar0_el1_; return true;
  case SysReg(3, 0, 0, 6, 1): value = id_aa64isar1_el1_; return true;
  case SysReg(3, 0, 0, 6, 2): value = id_aa64isar2_el1_; return true;
  case SysReg(3, 0, 0, 6, 3): value = id_aa64isar3_el1_; return true;
  case SysReg(3, 0, 0, 4, 4): value = id_aa64zfr0_el1_; return true;
  case SysReg(3, 0, 0, 7, 0): value = id_aa64mmfr0_el1_; return true;
  case SysReg(3, 0, 0, 7, 1): value = id_aa64mmfr1_el1_; return true;
  case SysReg(3, 0, 0, 7, 2): value = id_aa64mmfr2_el1_; return true;
  case SysReg(3, 0, 0, 7, 3): value = id_aa64mmfr3_el1_; return true;
  case SysReg(3, 0, 0, 7, 4): value = 0; return true; // ID_AA64MMFR4_EL1
  case SysReg(3, 0, 0, 4, 2): value = 0; return true; // ID_AA64PFR2_EL1
  case SysReg(3, 0, 0, 4, 5): value = 0; return true; // ID_AA64SMFR0_EL1
  case SysReg(3, 0, 0, 4, 7): value = 0; return true; // Reserved/unknown feature ID, keep disabled
  case SysReg(2, 0, 1, 0, 0): value = 0; return true; // MDRAR_EL1: no debug ROM exposed.
  case SysReg(3, 0, 10, 2, 0): value = mair_el1_; return true;
  case SysReg(3, 0, 13, 0, 1): value = sanitize_contextidr_el1(contextidr_el1_); return true;
  case SysReg(2, 0, 1, 3, 4): value = osdlr_el1_; return true;
  case SysReg(2, 0, 1, 1, 4): value = 0x8ull | ((oslar_el1_ & 1ull) << 1); return true;
  case SysReg(2, 0, 0, 6, 2): value = (oslar_el1_ & 1ull) ? (oseccr_el1_ & 0xFFFFFFFFull) : 0ull; return true;
  case SysReg(2, 0, 7, 8, 6): value = dbgclaim_el1_ & 0xFFull; return true;
  case SysReg(2, 0, 7, 9, 6): value = dbgclaim_el1_ & 0xFFull; return true;
  case SysReg(2, 0, 7, 14, 6): value = 0xAull; return true; // Non-secure debug implemented but disabled.
  case SysReg(2, 0, 1, 4, 4): value = dbgprcr_el1_; return true;
  case SysReg(3, 0, 12, 0, 0): value = sanitize_vbar_el1(vbar_el1_); return true;
  case SysReg(3, 0, 4, 0, 1): value = elr_el1_; return true;
  case SysReg(3, 0, 4, 0, 0): value = spsr_el1_; return true;
  case SysReg(3, 0, 4, 2, 2): value = static_cast<std::uint64_t>(current_el()) << 2; return true;
  case SysReg(3, 0, 5, 2, 0): value = esr_el1_; return true;
  case SysReg(3, 0, 6, 0, 0): value = far_el1_; return true;
  case SysReg(2, 0, 0, 2, 2): value = sanitize_mdscr_el1(mdscr_el1_); return true;
  case SysReg(3, 3, 13, 0, 2): value = tpidr_el0_; return true;
  case SysReg(3, 3, 13, 0, 3): value = tpidrro_el0_; return true;
  case SysReg(3, 0, 13, 0, 4): value = tpidr_el1_; return true;
  case SysReg(3, 4, 13, 0, 2): value = tpidr_el2_; return true;
  case SysReg(3, 0, 4, 1, 0):
    if (current_el() == 0u || current_uses_sp_el0()) {
      return false;
    }
    value = sp_el0_;
    return true;
  case SysReg(3, 4, 4, 1, 0):
    if (current_el() <= 1u) {
      return false;
    }
    value = sp_el1_;
    return true;
  case SysReg(3, 0, 7, 4, 0): value = par_el1_; return true;
  case SysReg(3, 3, 4, 2, 1): value = daif(); return true;
  case SysReg(3, 0, 4, 2, 0): value = spsel_; return true;
  case SysReg(3, 3, 0, 0, 1): value = ctr_el0_; return true;
  case SysReg(3, 3, 0, 0, 7): value = dczid_el0_; return true;
  case SysReg(3, 3, 14, 0, 0): value = cntfrq_el0_; return true;
  case SysReg(3, 3, 14, 0, 1): value = cntvct_el0_; return true; // CNTPCT_EL0 (minimal alias)
  case SysReg(3, 3, 14, 0, 2): value = cntvct_el0_; return true;
  case SysReg(3, 0, 14, 1, 0): value = sanitize_cntkctl_el1(cntkctl_el1_); return true;
  case SysReg(3, 3, 4, 4, 0): value = fpcr_; return true;
  case SysReg(3, 3, 4, 4, 1): value = fpsr_; return true;
  case SysReg(3, 0, 4, 2, 3): value = pstate_.pan ? 1ull : 0ull; return true;
  case SysReg(3, 3, 4, 2, 0): value = nzcv(); return true;
  default:
    return false;
  }
}

bool SystemRegisters::write(std::uint32_t op0,
                            std::uint32_t op1,
                            std::uint32_t crn,
                            std::uint32_t crm,
                            std::uint32_t op2,
                            std::uint64_t value) {
  if (op0 == 2u && op1 == 0u && crn == 0u && crm < 16u) {
    const std::uint32_t limit =
        (op2 == 4u || op2 == 5u) ? breakpoint_resource_count() :
        (op2 == 6u || op2 == 7u) ? watchpoint_resource_count() : 0u;
    if (limit != 0u && crm >= limit) {
      return false;
    }
    switch (op2) {
    case 4u: dbgbvr_el1_[crm] = value; return true;
    case 5u: dbgbcr_el1_[crm] = value; return true;
    case 6u: dbgwvr_el1_[crm] = value; return true;
    case 7u: dbgwcr_el1_[crm] = value; return true;
    default: break;
    }
  }
  switch (make_key(op0, op1, crn, crm, op2)) {
  case SysReg(3, 0, 1, 0, 0):
    sctlr_el1_ = sanitize_sctlr_el1(value,
                                    id_aa64mmfr0_el1_,
                                    id_aa64mmfr1_el1_,
                                    id_aa64mmfr2_el1_,
                                    id_aa64pfr0_el1_,
                                    id_aa64pfr1_el1_,
                                    id_aa64isar1_el1_,
                                    id_aa64isar2_el1_);
    return true;
  case SysReg(3, 0, 1, 0, 2): cpacr_el1_ = sanitize_cpacr_el1(value); return true;
  case SysReg(3, 2, 0, 0, 0): csselr_el1_ = sanitize_csselr_el1(value); return true;
  case SysReg(3, 0, 2, 0, 0): ttbr0_el1_ = sanitize_ttbr_el1(value, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_); return true;
  case SysReg(3, 0, 2, 0, 1): ttbr1_el1_ = sanitize_ttbr_el1(value, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_); return true;
  case SysReg(3, 0, 2, 0, 2):
    tcr_el1_ = sanitize_tcr_el1(value,
                                id_aa64mmfr0_el1_,
                                id_aa64mmfr1_el1_,
                                id_aa64mmfr2_el1_,
                                id_aa64pfr0_el1_,
                                id_aa64pfr1_el1_,
                                id_aa64isar1_el1_,
                                id_aa64isar2_el1_);
    return true;
  case SysReg(3, 0, 10, 2, 0): mair_el1_ = value; return true;
  case SysReg(3, 0, 13, 0, 1): contextidr_el1_ = sanitize_contextidr_el1(value); return true;
  case SysReg(2, 0, 1, 3, 4): osdlr_el1_ = value; return true;
  case SysReg(2, 0, 1, 0, 4): oslar_el1_ = value & 1ull; return true;
  case SysReg(2, 0, 0, 6, 2):
    if ((oslar_el1_ & 1ull) != 0u) {
      oseccr_el1_ = value & 0xFFFFFFFFull;
    }
    return true;
  case SysReg(2, 0, 7, 8, 6):
    dbgclaim_el1_ |= value & 0xFFull;
    return true;
  case SysReg(2, 0, 7, 9, 6):
    dbgclaim_el1_ &= ~(value & 0xFFull);
    return true;
  case SysReg(2, 0, 1, 4, 4):
    dbgprcr_el1_ = sanitize_dbgprcr_el1(value);
    return true;
  case SysReg(3, 0, 12, 0, 0): vbar_el1_ = sanitize_vbar_el1(value); return true;
  case SysReg(3, 0, 4, 0, 1): elr_el1_ = value; return true;
  case SysReg(3, 0, 4, 0, 0): spsr_el1_ = sanitize_spsr_el1(value); return true;
  case SysReg(3, 0, 5, 2, 0): esr_el1_ = value; return true;
  case SysReg(3, 0, 6, 0, 0): far_el1_ = value; return true;
  case SysReg(2, 0, 0, 2, 2):
    mdscr_el1_ = merge_mdscr_el1(mdscr_el1_, value, (oslar_el1_ & 1ull) != 0u);
    return true;
  case SysReg(3, 3, 13, 0, 2): tpidr_el0_ = value; return true;
  case SysReg(3, 3, 13, 0, 3): tpidrro_el0_ = value; return true;
  case SysReg(3, 0, 13, 0, 4): tpidr_el1_ = value; return true;
  case SysReg(3, 4, 13, 0, 2): tpidr_el2_ = value; return true;
  case SysReg(3, 0, 4, 1, 0):
    if (current_el() == 0u || current_uses_sp_el0()) {
      return false;
    }
    sp_el0_ = value;
    return true;
  case SysReg(3, 4, 4, 1, 0):
    if (current_el() <= 1u) {
      return false;
    }
    sp_el1_ = value;
    return true;
  case SysReg(3, 0, 7, 4, 0): par_el1_ = value; return true;
  case SysReg(3, 3, 4, 2, 1): set_daif(value); return true;
  case SysReg(3, 0, 4, 2, 0): set_spsel(value); return true;
  case SysReg(3, 0, 14, 1, 0): cntkctl_el1_ = sanitize_cntkctl_el1(value); return true;
  case SysReg(3, 0, 4, 2, 3): pstate_.pan = (value & 1u) != 0; return true;
  case SysReg(3, 3, 4, 4, 0): set_fpcr(value); return true;
  case SysReg(3, 3, 4, 4, 1): set_fpsr(value); return true;
  case SysReg(3, 3, 4, 2, 0): set_nzcv(value); return true;
  default:
    return false;
  }
}

std::uint32_t SystemRegisters::breakpoint_resource_count() const {
  return decode_debug_resource_count(id_aa64dfr0_el1_, 12u, id_aa64dfr1_el1_, 8u);
}

std::uint32_t SystemRegisters::watchpoint_resource_count() const {
  return decode_debug_resource_count(id_aa64dfr0_el1_, 20u, id_aa64dfr1_el1_, 16u);
}

std::uint64_t SystemRegisters::nzcv() const {
  return (static_cast<std::uint64_t>(pstate_.n) << 31) |
         (static_cast<std::uint64_t>(pstate_.z) << 30) |
         (static_cast<std::uint64_t>(pstate_.c) << 29) |
         (static_cast<std::uint64_t>(pstate_.v) << 28);
}

void SystemRegisters::set_nzcv(std::uint64_t value) {
  pstate_.n = ((value >> 31) & 1u) != 0;
  pstate_.z = ((value >> 30) & 1u) != 0;
  pstate_.c = ((value >> 29) & 1u) != 0;
  pstate_.v = ((value >> 28) & 1u) != 0;
}

std::uint64_t SystemRegisters::daif() const {
  return (static_cast<std::uint64_t>(pstate_.d) << 9) |
         (static_cast<std::uint64_t>(pstate_.a) << 8) |
         (static_cast<std::uint64_t>(pstate_.i) << 7) |
         (static_cast<std::uint64_t>(pstate_.f) << 6);
}

std::uint64_t SystemRegisters::pstate_bits() const {
  return nzcv() |
         (pstate_.ss ? (1ull << 21) : 0ull) |
         (pstate_.il ? (1ull << 20) : 0ull) |
         daif() |
         (pstate_.pan ? (1ull << 22) : 0ull) |
         (static_cast<std::uint64_t>(pstate_.mode) & 0xFu);
}

void SystemRegisters::set_pstate_bits(std::uint64_t value) {
  set_nzcv(value);
  pstate_.ss = ((value >> 21) & 1u) != 0;
  pstate_.il = ((value >> 20) & 1u) != 0;
  set_daif(value);
  pstate_.pan = ((value >> 22) & 1u) != 0;
  pstate_.mode = static_cast<std::uint8_t>(value & 0xFu);
  if (current_el() == 1u) {
    spsel_ = pstate_.mode & 0x1u;
  } else {
    spsel_ = 0;
  }
}

void SystemRegisters::set_daif(std::uint64_t value) {
  pstate_.d = ((value >> 9) & 1u) != 0;
  pstate_.a = ((value >> 8) & 1u) != 0;
  pstate_.i = ((value >> 7) & 1u) != 0;
  pstate_.f = ((value >> 6) & 1u) != 0;
}

void SystemRegisters::set_spsel(std::uint64_t value) {
  spsel_ = value & 1u;
  if (current_el() == 1u) {
    pstate_.mode = spsel_ != 0 ? 0x5u : 0x4u;
  }
}

bool SystemRegisters::software_step_enabled() const {
  return (mdscr_el1_ & kMdscrEl1Ss) != 0u;
}

bool SystemRegisters::os_lock_active() const {
  return (oslar_el1_ & 1u) != 0u;
}

bool SystemRegisters::double_lock_active() const {
  return (osdlr_el1_ & 1u) != 0u && (dbgprcr_el1_ & 1u) == 0u;
}

bool SystemRegisters::monitor_debug_enabled() const {
  return (mdscr_el1_ & kMdscrEl1Mde) != 0u;
}

bool SystemRegisters::kernel_debug_enabled() const {
  return (mdscr_el1_ & kMdscrEl1Kde) != 0u;
}

bool SystemRegisters::debug_exceptions_enabled_current() const {
  return debug_exceptions_enabled_from_el(current_el(), pstate_.d, mdscr_el1_);
}

bool SystemRegisters::breakpoint_watchpoint_enabled_current() const {
  return monitor_debug_enabled() &&
         debug_exceptions_enabled_current() &&
         !os_lock_active() &&
         !double_lock_active();
}

bool SystemRegisters::software_step_active_pending() const {
  return software_step_enabled() &&
         debug_exceptions_enabled_current() &&
         !os_lock_active() &&
         !double_lock_active() &&
         !pstate_.ss;
}

bool SystemRegisters::software_step_active_not_pending() const {
  return software_step_enabled() &&
         debug_exceptions_enabled_current() &&
         !os_lock_active() &&
         !double_lock_active() &&
         pstate_.ss;
}

void SystemRegisters::exception_enter_irq(std::uint64_t return_pc,
                                          std::uint64_t saved_pstate_bits) {
  elr_el1_ = return_pc;
  spsr_el1_ = sanitize_spsr_el1(saved_pstate_bits);
  esr_el1_ = 0;
  pstate_.mode = 0x5u;
  spsel_ = 1u;
  pstate_.ss = false;
  pstate_.il = false;
  pstate_.d = true;
  pstate_.a = true;
  if ((sctlr_el1_ & kSctlrEl1Span) == 0u) {
    pstate_.pan = true;
  }
  pstate_.i = true;
  pstate_.f = true;
}

void SystemRegisters::exception_enter_sync(std::uint64_t return_pc,
                                           std::uint64_t saved_pstate_bits,
                                           std::uint32_t ec,
                                           std::uint32_t iss,
                                           bool far_valid,
                                           std::uint64_t far) {
  elr_el1_ = return_pc;
  spsr_el1_ = sanitize_spsr_el1(saved_pstate_bits);
  // This model only supports AArch64 guest execution, so synchronous exceptions
  // are always reported as arising from a 32-bit A64 instruction.
  esr_el1_ = (static_cast<std::uint64_t>(ec & 0x3Fu) << 26) |
             (1ull << 25) |
             (iss & 0x1FFFFFFu);
  far_el1_ = far_valid ? far : 0;
  pstate_.mode = 0x5u;
  spsel_ = 1u;
  pstate_.ss = false;
  pstate_.il = false;
  pstate_.d = true;
  pstate_.a = true;
  if ((sctlr_el1_ & kSctlrEl1Span) == 0u) {
    pstate_.pan = true;
  }
  pstate_.i = true;
  pstate_.f = true;
}

bool SystemRegisters::illegal_exception_return() const {
  if ((spsr_el1_ & (1ull << 4)) != 0u) {
    return true; // AArch32 return state is unsupported in this model.
  }

  switch (static_cast<std::uint8_t>(spsr_el1_ & 0xFu)) {
    case 0x0u: // EL0t
    case 0x4u: // EL1t
    case 0x5u: // EL1h
      return false;
    default:
      return true;
  }
}

std::uint64_t SystemRegisters::exception_return(bool illegal_psr_state) {
  const bool enabled_at_source = debug_exceptions_enabled_from_el(current_el(), pstate_.d, mdscr_el1_);
  const std::uint8_t dest_el = illegal_psr_state ? current_el()
                                                 : static_cast<std::uint8_t>((spsr_el1_ >> 2) & 0x3u);
  const bool dest_d_masked = ((spsr_el1_ >> 9) & 1u) != 0u;
  const bool enabled_at_dest = debug_exceptions_enabled_from_el(dest_el, dest_d_masked, mdscr_el1_);
  const bool restore_software_step =
      software_step_enabled() &&
      !enabled_at_source &&
      enabled_at_dest &&
      (((spsr_el1_ >> 21) & 1u) != 0u);
  if (illegal_psr_state) {
    set_nzcv(spsr_el1_);
    set_daif(spsr_el1_);
    pstate_.pan = ((spsr_el1_ >> 22) & 1u) != 0;
    pstate_.ss = restore_software_step;
    pstate_.il = true;
    return elr_el1_;
  }

  set_pstate_bits(spsr_el1_);
  pstate_.ss = restore_software_step;
  return elr_el1_;
}

void SystemRegisters::daif_set(std::uint8_t imm4) {
  if ((imm4 & 0x8u) != 0) {
    pstate_.d = true;
  }
  if ((imm4 & 0x4u) != 0) {
    pstate_.a = true;
  }
  if ((imm4 & 0x2u) != 0) {
    pstate_.i = true;
  }
  if ((imm4 & 0x1u) != 0) {
    pstate_.f = true;
  }
}

void SystemRegisters::daif_clr(std::uint8_t imm4) {
  if ((imm4 & 0x8u) != 0) {
    pstate_.d = false;
  }
  if ((imm4 & 0x4u) != 0) {
    pstate_.a = false;
  }
  if ((imm4 & 0x2u) != 0) {
    pstate_.i = false;
  }
  if ((imm4 & 0x1u) != 0) {
    pstate_.f = false;
  }
}

std::uint32_t SystemRegisters::make_key(std::uint32_t op0,
                                        std::uint32_t op1,
                                        std::uint32_t crn,
                                        std::uint32_t crm,
                                        std::uint32_t op2) {
  return (op0 << 14) | (op1 << 11) | (crn << 7) | (crm << 3) | op2;
}


bool SystemRegisters::save_state(std::ostream& out) const {
  return snapshot_io::write(out, sctlr_el1_) &&
         snapshot_io::write(out, cpacr_el1_) &&
         snapshot_io::write(out, midr_el1_) &&
         snapshot_io::write(out, mpidr_el1_) &&
         snapshot_io::write(out, revidr_el1_) &&
         snapshot_io::write(out, clidr_el1_) &&
         snapshot_io::write(out, ctr_el0_) &&
         snapshot_io::write(out, dczid_el0_) &&
         snapshot_io::write(out, id_aa64pfr0_el1_) &&
         snapshot_io::write(out, id_aa64pfr1_el1_) &&
         snapshot_io::write(out, id_aa64dfr0_el1_) &&
         snapshot_io::write(out, id_aa64dfr1_el1_) &&
         snapshot_io::write(out, id_aa64isar0_el1_) &&
         snapshot_io::write(out, id_aa64isar1_el1_) &&
         snapshot_io::write(out, id_aa64isar2_el1_) &&
         snapshot_io::write(out, id_aa64isar3_el1_) &&
         snapshot_io::write(out, id_aa64zfr0_el1_) &&
         snapshot_io::write(out, id_aa64mmfr0_el1_) &&
         snapshot_io::write(out, id_aa64mmfr1_el1_) &&
         snapshot_io::write(out, id_aa64mmfr2_el1_) &&
         snapshot_io::write(out, id_aa64mmfr3_el1_) &&
         snapshot_io::write(out, csselr_el1_) &&
         snapshot_io::write(out, ccsidr_el1_) &&
         snapshot_io::write(out, ttbr0_el1_) &&
         snapshot_io::write(out, ttbr1_el1_) &&
         snapshot_io::write(out, tcr_el1_) &&
         snapshot_io::write(out, mair_el1_) &&
         snapshot_io::write(out, contextidr_el1_) &&
         snapshot_io::write(out, osdlr_el1_) &&
         snapshot_io::write(out, oslar_el1_) &&
         snapshot_io::write(out, oseccr_el1_) &&
         snapshot_io::write(out, dbgclaim_el1_) &&
         snapshot_io::write(out, dbgprcr_el1_) &&
         snapshot_io::write_array(out, dbgbvr_el1_) &&
         snapshot_io::write_array(out, dbgbcr_el1_) &&
         snapshot_io::write_array(out, dbgwvr_el1_) &&
         snapshot_io::write_array(out, dbgwcr_el1_) &&
         snapshot_io::write(out, vbar_el1_) &&
         snapshot_io::write(out, elr_el1_) &&
         snapshot_io::write(out, spsr_el1_) &&
         snapshot_io::write(out, sp_el0_) &&
         snapshot_io::write(out, sp_el1_) &&
         snapshot_io::write(out, spsel_) &&
         snapshot_io::write(out, par_el1_) &&
         snapshot_io::write(out, esr_el1_) &&
         snapshot_io::write(out, far_el1_) &&
         snapshot_io::write(out, mdscr_el1_) &&
         snapshot_io::write(out, pmuserenr_el0_) &&
         snapshot_io::write(out, amuserenr_el0_) &&
         snapshot_io::write(out, tpidr_el0_) &&
         snapshot_io::write(out, tpidr2_el0_) &&
         snapshot_io::write(out, tpidrro_el0_) &&
         snapshot_io::write(out, tpidr_el1_) &&
         snapshot_io::write(out, tpidr_el2_) &&
         snapshot_io::write(out, cntfrq_el0_) &&
         snapshot_io::write(out, cntvct_el0_) &&
         snapshot_io::write(out, cntkctl_el1_) &&
         snapshot_io::write(out, fpcr_) &&
         snapshot_io::write(out, fpsr_) &&
         snapshot_io::write_bool(out, pstate_.n) &&
         snapshot_io::write_bool(out, pstate_.z) &&
         snapshot_io::write_bool(out, pstate_.c) &&
         snapshot_io::write_bool(out, pstate_.v) &&
         snapshot_io::write_bool(out, pstate_.ss) &&
         snapshot_io::write_bool(out, pstate_.il) &&
         snapshot_io::write_bool(out, pstate_.d) &&
         snapshot_io::write_bool(out, pstate_.a) &&
         snapshot_io::write_bool(out, pstate_.i) &&
         snapshot_io::write_bool(out, pstate_.f) &&
         snapshot_io::write_bool(out, pstate_.pan) &&
         snapshot_io::write(out, pstate_.mode);
}

bool SystemRegisters::load_state(std::istream& in, std::uint32_t version) {
  pstate_.ss = false;
  pstate_.il = false;
  return snapshot_io::read(in, sctlr_el1_) &&
         snapshot_io::read(in, cpacr_el1_) &&
         snapshot_io::read(in, midr_el1_) &&
         snapshot_io::read(in, mpidr_el1_) &&
         snapshot_io::read(in, revidr_el1_) &&
         snapshot_io::read(in, clidr_el1_) &&
         snapshot_io::read(in, ctr_el0_) &&
         snapshot_io::read(in, dczid_el0_) &&
         snapshot_io::read(in, id_aa64pfr0_el1_) &&
         snapshot_io::read(in, id_aa64pfr1_el1_) &&
         snapshot_io::read(in, id_aa64dfr0_el1_) &&
         snapshot_io::read(in, id_aa64dfr1_el1_) &&
         snapshot_io::read(in, id_aa64isar0_el1_) &&
         snapshot_io::read(in, id_aa64isar1_el1_) &&
         snapshot_io::read(in, id_aa64isar2_el1_) &&
         snapshot_io::read(in, id_aa64isar3_el1_) &&
         snapshot_io::read(in, id_aa64zfr0_el1_) &&
         snapshot_io::read(in, id_aa64mmfr0_el1_) &&
         snapshot_io::read(in, id_aa64mmfr1_el1_) &&
         snapshot_io::read(in, id_aa64mmfr2_el1_) &&
         snapshot_io::read(in, id_aa64mmfr3_el1_) &&
         snapshot_io::read(in, csselr_el1_) &&
         snapshot_io::read(in, ccsidr_el1_) &&
         snapshot_io::read(in, ttbr0_el1_) &&
         snapshot_io::read(in, ttbr1_el1_) &&
         snapshot_io::read(in, tcr_el1_) &&
         snapshot_io::read(in, mair_el1_) &&
         snapshot_io::read(in, contextidr_el1_) &&
         snapshot_io::read(in, osdlr_el1_) &&
         snapshot_io::read(in, oslar_el1_) &&
         ((version < 18) || snapshot_io::read(in, oseccr_el1_)) &&
         ((version < 18) || snapshot_io::read(in, dbgclaim_el1_)) &&
         ((version < 19) || snapshot_io::read(in, dbgprcr_el1_)) &&
         snapshot_io::read_array(in, dbgbvr_el1_) &&
         snapshot_io::read_array(in, dbgbcr_el1_) &&
         snapshot_io::read_array(in, dbgwvr_el1_) &&
         snapshot_io::read_array(in, dbgwcr_el1_) &&
         snapshot_io::read(in, vbar_el1_) &&
         snapshot_io::read(in, elr_el1_) &&
         snapshot_io::read(in, spsr_el1_) &&
         snapshot_io::read(in, sp_el0_) &&
         snapshot_io::read(in, sp_el1_) &&
         snapshot_io::read(in, spsel_) &&
         snapshot_io::read(in, par_el1_) &&
         snapshot_io::read(in, esr_el1_) &&
         snapshot_io::read(in, far_el1_) &&
         snapshot_io::read(in, mdscr_el1_) &&
         snapshot_io::read(in, pmuserenr_el0_) &&
         snapshot_io::read(in, amuserenr_el0_) &&
         snapshot_io::read(in, tpidr_el0_) &&
         snapshot_io::read(in, tpidr2_el0_) &&
         snapshot_io::read(in, tpidrro_el0_) &&
         snapshot_io::read(in, tpidr_el1_) &&
         snapshot_io::read(in, tpidr_el2_) &&
         snapshot_io::read(in, cntfrq_el0_) &&
         snapshot_io::read(in, cntvct_el0_) &&
         snapshot_io::read(in, cntkctl_el1_) &&
         snapshot_io::read(in, fpcr_) &&
         snapshot_io::read(in, fpsr_) &&
         snapshot_io::read_bool(in, pstate_.n) &&
         snapshot_io::read_bool(in, pstate_.z) &&
         snapshot_io::read_bool(in, pstate_.c) &&
         snapshot_io::read_bool(in, pstate_.v) &&
         ((version < 16) || snapshot_io::read_bool(in, pstate_.ss)) &&
         ((version < 15) || snapshot_io::read_bool(in, pstate_.il)) &&
         snapshot_io::read_bool(in, pstate_.d) &&
         snapshot_io::read_bool(in, pstate_.a) &&
         snapshot_io::read_bool(in, pstate_.i) &&
         snapshot_io::read_bool(in, pstate_.f) &&
         snapshot_io::read_bool(in, pstate_.pan) &&
         snapshot_io::read(in, pstate_.mode) &&
         ((version >= 18) || ((oseccr_el1_ = 0), true)) &&
         ((version >= 18) || ((dbgclaim_el1_ = 0), true)) &&
         ((version >= 19) || ((dbgprcr_el1_ = 0), true)) &&
         ((sctlr_el1_ = sanitize_sctlr_el1(sctlr_el1_,
                                           id_aa64mmfr0_el1_,
                                           id_aa64mmfr1_el1_,
                                           id_aa64mmfr2_el1_,
                                           id_aa64pfr0_el1_,
                                           id_aa64pfr1_el1_,
                                           id_aa64isar1_el1_,
                                           id_aa64isar2_el1_)), true) &&
         ((cpacr_el1_ = sanitize_cpacr_el1(cpacr_el1_)), true) &&
         ((csselr_el1_ = sanitize_csselr_el1(csselr_el1_)), true) &&
         ((ttbr0_el1_ = sanitize_ttbr_el1(ttbr0_el1_, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_)), true) &&
         ((ttbr1_el1_ = sanitize_ttbr_el1(ttbr1_el1_, id_aa64mmfr0_el1_, id_aa64mmfr2_el1_)), true) &&
         ((tcr_el1_ = sanitize_tcr_el1(tcr_el1_,
                                       id_aa64mmfr0_el1_,
                                       id_aa64mmfr1_el1_,
                                       id_aa64mmfr2_el1_,
                                       id_aa64pfr0_el1_,
                                       id_aa64pfr1_el1_,
                                       id_aa64isar1_el1_,
                                       id_aa64isar2_el1_)), true) &&
         ((vbar_el1_ = sanitize_vbar_el1(vbar_el1_)), true) &&
         ((contextidr_el1_ = sanitize_contextidr_el1(contextidr_el1_)), true) &&
         ((cntkctl_el1_ = sanitize_cntkctl_el1(cntkctl_el1_)), true) &&
         ((spsr_el1_ = sanitize_spsr_el1(spsr_el1_)), true) &&
         ((mdscr_el1_ = sanitize_mdscr_el1(mdscr_el1_)), true) &&
         ((set_fpcr(fpcr_)), true) &&
         ((set_fpsr(fpsr_)), true) &&
         ((dbgprcr_el1_ = sanitize_dbgprcr_el1(dbgprcr_el1_)), true);
}


} // namespace aarchvm
