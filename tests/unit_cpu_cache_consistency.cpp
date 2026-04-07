#include <cstdint>
#include <iostream>
#include <sstream>

#define private public
#include "aarchvm/cpu.hpp"
#undef private

#include "aarchvm/bus.hpp"
#include "aarchvm/generic_timer.hpp"
#include "aarchvm/gicv3.hpp"

namespace {

using aarchvm::Bus;
using aarchvm::Cpu;
using aarchvm::GenericTimer;
using aarchvm::GicV3;

constexpr std::uint64_t kCanonicalUpperVa = 0xFFFF000000001000ull;
constexpr std::uint64_t kTaggedUpperVa = 0xABFF000000001000ull;
constexpr std::uint64_t kPa = 0x0000000000002000ull;
constexpr std::uint32_t kInsn = 0xD2800820u; // mov x0, #0x41
constexpr std::uint32_t kSnapshotVersion = 24u;

std::size_t decode_page_index(std::uint64_t va) {
  return static_cast<std::size_t>((va >> 12) & (Cpu::kDecodeCachePages - 1u));
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
