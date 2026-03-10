#include "aarchvm/soc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {

struct BinaryLoad {
  std::string path;
  std::uint64_t addr = 0;
};

struct Options {
  std::string bin_path;
  std::uint64_t load_addr = 0;
  std::uint64_t entry_pc = 0;
  std::size_t max_steps = 1000000;
  std::uint64_t init_sp = 0;
  std::optional<std::string> dtb_path;
  std::uint64_t dtb_addr = 0x40000000ull;
  std::vector<BinaryLoad> extra_bins;
};

std::uint64_t parse_u64(const std::string& text) {
  return std::stoull(text, nullptr, 0);
}

bool read_binary_file(const std::string& path, std::vector<std::uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);
  out.resize(static_cast<std::size_t>(size));
  if (size == 0) {
    return true;
  }
  in.read(reinterpret_cast<char*>(out.data()), size);
  return static_cast<std::size_t>(in.gcount()) == out.size();
}

void dump_bytes(const aarchvm::SoC& soc, std::uint64_t base, std::size_t len, const char* tag) {
  std::cerr << "DUMP[" << tag << "] addr=0x" << std::hex << base
            << " len=0x" << len << std::dec << '\n';
  for (std::size_t i = 0; i < len; ++i) {
    if ((i % 16) == 0) {
      std::cerr << "  " << std::hex << std::setw(16) << std::setfill('0') << (base + i) << ": ";
    }
    const auto v = soc.read_u8(base + i);
    if (!v.has_value()) {
      std::cerr << "?? ";
    } else {
      std::cerr << std::setw(2) << static_cast<unsigned>(*v) << ' ';
    }
    if ((i % 16) == 15) {
      std::cerr << '\n';
    }
  }
  if ((len % 16) != 0) {
    std::cerr << '\n';
  }
  std::cerr << std::setfill(' ') << std::dec;
}

void print_usage(const char* argv0) {
  std::cerr
      << "Usage: " << argv0 << " -bin <program.bin> "
      << "[-load <addr>] [-entry <pc>] [-steps <n>] [-sp <addr>] "
      << "[-dtb <file>] [-dtb-addr <addr>] [-segment <file@addr>]...\n";
}

std::optional<Options> parse_args(int argc, char** argv) {
  if (argc == 1) {
    return std::nullopt;
  }

  Options opt;
  bool has_entry = false;
  for (int i = 1; i < argc; ++i) {
    const std::string key = argv[i];
    if (key == "-h" || key == "--help") {
      print_usage(argv[0]);
      std::exit(0);
    }
    if (key.empty() || key[0] != '-') {
      std::cerr << "Invalid argument (expected -key): " << key << '\n';
      return std::nullopt;
    }
    if (i + 1 >= argc) {
      std::cerr << "Missing value for " << key << '\n';
      return std::nullopt;
    }
    const std::string val = argv[++i];

    if (key == "-bin") {
      opt.bin_path = val;
    } else if (key == "-load") {
      opt.load_addr = parse_u64(val);
    } else if (key == "-entry") {
      opt.entry_pc = parse_u64(val);
      has_entry = true;
    } else if (key == "-steps") {
      opt.max_steps = static_cast<std::size_t>(parse_u64(val));
    } else if (key == "-sp") {
      opt.init_sp = parse_u64(val);
    } else if (key == "-dtb") {
      opt.dtb_path = val;
    } else if (key == "-dtb-addr") {
      opt.dtb_addr = parse_u64(val);
    } else if (key == "-segment") {
      const std::size_t at = val.rfind('@');
      if (at == std::string::npos || at == 0 || at + 1 >= val.size()) {
        std::cerr << "Invalid -segment value (expected <file@addr>): " << val << '\n';
        return std::nullopt;
      }
      BinaryLoad seg{};
      seg.path = val.substr(0, at);
      seg.addr = parse_u64(val.substr(at + 1));
      opt.extra_bins.push_back(seg);
    } else {
      std::cerr << "Unknown option: " << key << '\n';
      return std::nullopt;
    }
  }

  if (opt.bin_path.empty()) {
    std::cerr << "Missing required -bin\n";
    return std::nullopt;
  }
  if (!has_entry) {
    opt.entry_pc = opt.load_addr;
  }
  return opt;
}

class RawStdinGuard {
public:
  RawStdinGuard() {
    if (!isatty(STDIN_FILENO)) {
      return;
    }
    if (tcgetattr(STDIN_FILENO, &old_) != 0) {
      return;
    }
    termios t = old_;
    t.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0) {
      return;
    }

    old_flags_ = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (old_flags_ >= 0) {
      fcntl(STDIN_FILENO, F_SETFL, old_flags_ | O_NONBLOCK);
    }
    enabled_ = true;
  }

  ~RawStdinGuard() {
    if (!enabled_) {
      return;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_);
    if (old_flags_ >= 0) {
      fcntl(STDIN_FILENO, F_SETFL, old_flags_);
    }
  }

private:
  bool enabled_ = false;
  int old_flags_ = -1;
  termios old_{};
};

void pump_stdin_to_uart(aarchvm::SoC& soc) {
  std::uint8_t buf[64];
  while (true) {
    const ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
      for (ssize_t i = 0; i < n; ++i) {
        soc.inject_uart_rx(buf[i]);
      }
      continue;
    }
    if (n == 0) {
      return;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    return;
  }
}

} // namespace

int main(int argc, char** argv) {
  auto parsed = parse_args(argc, argv);
  if (!parsed.has_value()) {
    if (argc == 1) {
      // Keep the embedded demo for quick smoke runs.
      aarchvm::SoC demo_soc;
      const std::vector<std::uint32_t> image = {
          0xD2800001u, 0xF2A12001u, 0xD2800900u, 0xF9000020u, 0xD2800D20u,
          0xF9000020u, 0xD2800140u, 0xF9000020u, 0xD4200000u,
      };
      if (!demo_soc.load_image(0x00000000, image)) {
        std::cerr << "Failed to load embedded demo image\n";
        return 1;
      }
      demo_soc.reset(0x00000000);
      if (!demo_soc.run(1000)) {
        std::cerr << "Simulation stopped unexpectedly\n";
        return 1;
      }
      return 0;
    }
    print_usage(argv[0]);
    return 1;
  }
  const Options& opt = *parsed;

  aarchvm::SoC soc;

  std::vector<std::uint8_t> program;
  if (!read_binary_file(opt.bin_path, program)) {
    std::cerr << "Failed to read program binary: " << opt.bin_path << '\n';
    return 1;
  }
  if (!soc.load_binary(opt.load_addr, program)) {
    std::cerr << "Failed to load binary at 0x" << std::hex << opt.load_addr << std::dec << '\n';
    return 1;
  }

  for (const auto& seg : opt.extra_bins) {
    std::vector<std::uint8_t> extra;
    if (!read_binary_file(seg.path, extra)) {
      std::cerr << "Failed to read extra binary: " << seg.path << '\n';
      return 1;
    }
    if (!soc.load_binary(seg.addr, extra)) {
      std::cerr << "Failed to load extra binary at 0x" << std::hex << seg.addr
                << std::dec << ": " << seg.path << '\n';
      return 1;
    }
  }

  bool dtb_loaded = false;
  if (opt.dtb_path.has_value() || std::getenv("AARCHVM_DTB_PATH") != nullptr) {
    const std::string dtb_path =
        opt.dtb_path.has_value() ? *opt.dtb_path : std::string(std::getenv("AARCHVM_DTB_PATH"));
    const std::uint64_t dtb_addr =
        opt.dtb_path.has_value()
            ? opt.dtb_addr
            : ((std::getenv("AARCHVM_DTB_ADDR") != nullptr)
                   ? parse_u64(std::string(std::getenv("AARCHVM_DTB_ADDR")))
                   : 0x40000000ull);

    std::vector<std::uint8_t> dtb;
    if (!read_binary_file(dtb_path, dtb)) {
      std::cerr << "Failed to read DTB binary: " << dtb_path << '\n';
      return 1;
    }
    if (!soc.load_binary(dtb_addr, dtb)) {
      std::cerr << "Failed to load DTB at 0x" << std::hex << dtb_addr << std::dec << '\n';
      return 1;
    }
    dtb_loaded = true;
    if (const char* dump_addr_env = std::getenv("AARCHVM_DUMP_ADDR"); dump_addr_env != nullptr) {
      const std::uint64_t dump_addr = parse_u64(std::string(dump_addr_env));
      const std::size_t dump_len =
          (std::getenv("AARCHVM_DUMP_LEN") != nullptr)
              ? static_cast<std::size_t>(parse_u64(std::string(std::getenv("AARCHVM_DUMP_LEN"))))
              : 64u;
      dump_bytes(soc, dump_addr, dump_len, "pre-reset");
    }
  }

  soc.reset(opt.entry_pc);
  if (opt.init_sp != 0) {
    soc.set_sp(opt.init_sp);
  }
  if (dtb_loaded) {
    const std::uint64_t dtb_addr =
        opt.dtb_path.has_value()
            ? opt.dtb_addr
            : ((std::getenv("AARCHVM_DTB_ADDR") != nullptr)
                   ? parse_u64(std::string(std::getenv("AARCHVM_DTB_ADDR")))
                   : 0x40000000ull);
    soc.set_x(0, dtb_addr);
  }

  RawStdinGuard stdin_guard;
  std::size_t remaining = opt.max_steps;
  constexpr std::size_t kRunChunk = 2000;
  while (remaining > 0) {
    pump_stdin_to_uart(soc);
    const std::size_t n = std::min(kRunChunk, remaining);
    if (!soc.run(n)) {
      std::cerr << "Simulation stopped unexpectedly\n";
      return 1;
    }
    remaining -= n;
  }

  if (const char* dump_post_addr_env = std::getenv("AARCHVM_DUMP_POST_ADDR"); dump_post_addr_env != nullptr) {
    const std::uint64_t dump_addr = parse_u64(std::string(dump_post_addr_env));
    const std::size_t dump_len =
        (std::getenv("AARCHVM_DUMP_POST_LEN") != nullptr)
            ? static_cast<std::size_t>(parse_u64(std::string(std::getenv("AARCHVM_DUMP_POST_LEN"))))
            : 64u;
    dump_bytes(soc, dump_addr, dump_len, "post-run");
  }
  if (std::getenv("AARCHVM_PRINT_SUMMARY") != nullptr) {
    std::cerr << "SUMMARY: steps=" << soc.steps()
              << " pc=0x" << std::hex << soc.pc() << std::dec << '\n';
  }

  return 0;
}

