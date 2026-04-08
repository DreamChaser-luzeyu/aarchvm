#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#define private public
#include "aarchvm/block_mmio.hpp"
#include "aarchvm/cpu.hpp"
#include "aarchvm/ram.hpp"
#include "aarchvm/virtio_blk_mmio.hpp"
#undef private

#include "aarchvm/bus.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"

namespace {

using aarchvm::Bus;
using aarchvm::BlockMmio;
using aarchvm::Cpu;
using aarchvm::GenericTimer;
using aarchvm::GicV3;
using aarchvm::Ram;
using aarchvm::VirtioBlkMmio;

constexpr std::uint64_t kCanonicalUpperVa = 0xFFFF000000001000ull;
constexpr std::uint64_t kTaggedUpperVa = 0xABFF000000001000ull;
constexpr std::uint64_t kPa = 0x0000000000002000ull;
constexpr std::uint32_t kInsn = 0xD2800820u; // mov x0, #0x41
constexpr std::uint32_t kSnapshotVersion = 24u;
constexpr std::uint64_t kOldFar = 0xDEADBEEFCAFEBABEull;
constexpr std::uint64_t kFaultVa = 0x0000000000001000ull;
constexpr std::uint64_t kWalkAbortVa = 0x0000000000000000ull;
constexpr std::uint64_t kL0 = 0x1000ull;
constexpr std::uint64_t kL1 = 0x2000ull;
constexpr std::uint64_t kL2 = 0x3000ull;
constexpr std::uint64_t kL3 = 0x4000ull;
constexpr std::uint64_t kUnmappedWalkBase = 0x30000ull;

std::size_t decode_page_index(std::uint64_t va) {
  return static_cast<std::size_t>((va >> 12) & (Cpu::kDecodeCachePages - 1u));
}

bool write_desc(Bus& bus, std::uint64_t pa, std::uint64_t desc) {
  return bus.write(pa, desc, 8u);
}

bool enable_4k_stage1_mmu(Cpu& cpu, std::uint64_t ttbr0) {
  return cpu.sysregs_.write(3u, 0u, 2u, 0u, 0u, ttbr0) &&      // TTBR0_EL1
         cpu.sysregs_.write(3u, 0u, 2u, 0u, 2u, 16u) &&        // TCR_EL1, 48-bit VA, 4K TG0
         cpu.sysregs_.write(3u, 0u, 10u, 2u, 0u, 0u) &&        // MAIR_EL1 attr0 = Device-nGnRnE
         cpu.sysregs_.write(3u, 0u, 1u, 0u, 0u, 1u);           // SCTLR_EL1.M
}

bool install_translation_fault_tables(Bus& bus) {
  return write_desc(bus, kL0, kL1 | 0x3u) &&
         write_desc(bus, kL1, kL2 | 0x3u) &&
         write_desc(bus, kL2, kL3 | 0x3u);
}

bool prime_decode_page(Cpu& cpu) {
  cpu.set_predecode_enabled(true);
  const Cpu::DecodedInsn* decoded = cpu.lookup_decoded(kCanonicalUpperVa, kPa, kInsn);
  if (decoded == nullptr) {
    return false;
  }
  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  return cpu.decode_pages_[page_idx].valid &&
         cpu.decode_pages_[page_idx].va_page == (kCanonicalUpperVa & ~0xFFFull) &&
         cpu.decode_pages_[page_idx].pa_page == (kPa & ~0xFFFull) &&
         cpu.decode_last_page_ == &cpu.decode_pages_[page_idx];
}

bool test_snapshot_load_clears_decode_cache() {
  Bus bus;
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!prime_decode_page(cpu)) {
    return false;
  }

  std::stringstream snapshot(std::ios::in | std::ios::out | std::ios::binary);
  if (!cpu.save_state(snapshot)) {
    return false;
  }
  snapshot.seekg(0);
  if (!cpu.load_state(snapshot, kSnapshotVersion)) {
    return false;
  }

  if (cpu.decode_last_page_ != nullptr) {
    return false;
  }
  for (const auto& page : cpu.decode_pages_) {
    if (page.valid) {
      return false;
    }
  }
  return true;
}

bool test_tlbi_tagged_upper_invalidates_canonical_decode_page() {
  Bus bus;
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!prime_decode_page(cpu)) {
    return false;
  }

  const std::uint64_t tlbi_operand = (kTaggedUpperVa >> 12) & Cpu::tlb_page_mask();
  cpu.set_x(0, tlbi_operand);
  if (!cpu.exec_system(0xD5088720u)) { // tlbi vae1, x0
    return false;
  }

  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  return !cpu.decode_pages_[page_idx].valid && cpu.decode_last_page_ == nullptr;
}

bool test_ic_ivau_tagged_upper_invalidates_canonical_decode_page() {
  Bus bus;
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!prime_decode_page(cpu)) {
    return false;
  }

  if (!cpu.sysregs_.write(3u, 0u, 2u, 0u, 2u, 1ull << 38)) { // TCR_EL1.TBI1
    return false;
  }
  cpu.set_x(0, kTaggedUpperVa);
  if (!cpu.exec_system(0xD50B7520u)) { // ic ivau, x0
    return false;
  }

  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  return !cpu.decode_pages_[page_idx].valid && cpu.decode_last_page_ == nullptr;
}

bool test_pa_invalidation_clears_decode_page_with_different_cache_index() {
  Bus bus;
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!prime_decode_page(cpu)) {
    return false;
  }

  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  if (page_idx == static_cast<std::size_t>((kPa >> 12) & (Cpu::kDecodeCachePages - 1u))) {
    return false;
  }

  cpu.notify_external_memory_write(kPa, 4u);
  return !cpu.decode_pages_[page_idx].valid && cpu.decode_last_page_ == nullptr;
}

bool test_block_dma_write_notifies_cpu_decode_and_exclusive() {
  Bus bus;
  auto ram = std::make_shared<Ram>(0x10000);
  bus.map(0, 0x10000, ram);
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  bus.set_ram_write_observer([&cpu](std::uint64_t pa, std::size_t size) {
    cpu.notify_external_memory_write(pa, size);
  });
  if (!prime_decode_page(cpu)) {
    return false;
  }

  cpu.set_exclusive_monitor(kPa, 8u);
  if (!cpu.exclusive_valid_) {
    return false;
  }

  BlockMmio block(bus);
  std::vector<std::uint8_t> image(512u, 0x5Au);
  block.set_image(image);
  block.buffer_addr_ = kPa;
  block.lba_ = 0;
  block.count_ = 1;
  if (!block.transfer_from_image()) {
    return false;
  }

  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  return !cpu.decode_pages_[page_idx].valid && cpu.decode_last_page_ == nullptr && !cpu.exclusive_valid_;
}

bool test_virtio_dma_write_notifies_cpu_decode_and_exclusive() {
  Bus bus;
  auto ram = std::make_shared<Ram>(0x10000);
  bus.map(0, 0x10000, ram);
  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  bus.set_ram_write_observer([&cpu](std::uint64_t pa, std::size_t size) {
    cpu.notify_external_memory_write(pa, size);
  });
  if (!prime_decode_page(cpu)) {
    return false;
  }

  cpu.set_exclusive_monitor(kPa, 8u);
  if (!cpu.exclusive_valid_) {
    return false;
  }

  VirtioBlkMmio virtio(bus);
  const std::array<std::uint8_t, 4> payload = {0x11u, 0x22u, 0x33u, 0x44u};
  if (!virtio.write_guest(kPa, payload.data(), payload.size())) {
    return false;
  }

  const auto page_idx = decode_page_index(kCanonicalUpperVa);
  return !cpu.decode_pages_[page_idx].valid && cpu.decode_last_page_ == nullptr && !cpu.exclusive_valid_;
}

bool test_cache_maintenance_translation_fault_uses_requested_far() {
  Bus bus;
  auto ram = std::make_shared<Ram>(0x20000);
  bus.map(0, 0x20000, ram);
  if (!install_translation_fault_tables(bus)) {
    return false;
  }

  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!enable_4k_stage1_mmu(cpu, kL0)) {
    return false;
  }

  cpu.pc_ = 0x8000;
  cpu.last_data_fault_va_ = kOldFar;

  Cpu::TranslationResult result{};
  if (cpu.translate_cache_maintenance_address(kFaultVa, &result, true, false, false)) {
    return false;
  }
  if (!cpu.last_translation_fault_.has_value() ||
      cpu.last_translation_fault_->kind != Cpu::TranslationFault::Kind::Translation ||
      cpu.last_translation_fault_->level != 3u ||
      cpu.last_data_fault_va_.has_value()) {
    return false;
  }

  cpu.data_abort(kFaultVa, true);
  const std::uint64_t esr = cpu.sysregs_.esr_el1();
  return cpu.sysregs_.far_el1() == kFaultVa &&
         (esr >> 26) == 0x25u &&
         ((esr >> 8) & 0x1u) != 0u &&
         ((esr >> 6) & 0x1u) != 0u;
}

bool test_address_translation_walk_abort_uses_requested_far() {
  Bus bus;
  auto ram = std::make_shared<Ram>(0x20000);
  bus.map(0, 0x20000, ram);

  GicV3 gic;
  GenericTimer timer;
  Cpu cpu(bus, gic, timer);
  if (!enable_4k_stage1_mmu(cpu, kUnmappedWalkBase)) {
    return false;
  }

  cpu.pc_ = 0x8000;
  cpu.last_data_fault_va_ = kOldFar;

  Cpu::TranslationResult result{};
  if (cpu.translate_address(kWalkAbortVa, Cpu::AccessType::Read, &result, false, false, false)) {
    return false;
  }
  if (!cpu.last_translation_fault_.has_value() ||
      cpu.last_translation_fault_->kind != Cpu::TranslationFault::Kind::ExternalAbortOnWalk ||
      cpu.last_translation_fault_->level != 0u ||
      cpu.last_data_fault_va_.has_value()) {
    return false;
  }

  cpu.data_abort(kWalkAbortVa, true);
  const std::uint64_t esr = cpu.sysregs_.esr_el1();
  return cpu.sysregs_.far_el1() == kWalkAbortVa &&
         (esr >> 26) == 0x25u &&
         ((esr >> 8) & 0x1u) != 0u &&
         ((esr >> 6) & 0x1u) != 0u &&
         (esr & 0x3Fu) == 0x14u;
}

} // namespace

int main() {
  struct TestCase {
    const char* name;
    bool (*fn)();
  };

  const TestCase tests[] = {
      {"snapshot_load_clears_decode_cache", test_snapshot_load_clears_decode_cache},
      {"tlbi_tagged_upper_invalidates_canonical_decode_page",
       test_tlbi_tagged_upper_invalidates_canonical_decode_page},
      {"ic_ivau_tagged_upper_invalidates_canonical_decode_page",
       test_ic_ivau_tagged_upper_invalidates_canonical_decode_page},
      {"pa_invalidation_clears_decode_page_with_different_cache_index",
       test_pa_invalidation_clears_decode_page_with_different_cache_index},
      {"block_dma_write_notifies_cpu_decode_and_exclusive",
       test_block_dma_write_notifies_cpu_decode_and_exclusive},
      {"virtio_dma_write_notifies_cpu_decode_and_exclusive",
       test_virtio_dma_write_notifies_cpu_decode_and_exclusive},
      {"cache_maintenance_translation_fault_uses_requested_far",
       test_cache_maintenance_translation_fault_uses_requested_far},
      {"address_translation_walk_abort_uses_requested_far",
       test_address_translation_walk_abort_uses_requested_far},
  };

  for (const auto& test : tests) {
    if (!test.fn()) {
      std::cerr << "FAIL: " << test.name << '\n';
      return 1;
    }
  }

  std::cout << "PASS\n";
  return 0;
}
