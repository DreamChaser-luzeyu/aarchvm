#include "aarchvm/soc.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace {

struct BinaryLoad {
  std::string path;
  std::uint64_t addr = 0;
};

struct ByteEvent {
  std::uint64_t step = 0;
  std::uint8_t byte = 0;
};

struct Options {
  std::string bin_path;
  std::uint64_t load_addr = 0;
  std::uint64_t entry_pc = 0;
  std::size_t max_steps = 1000000;
  std::size_t cpu_count = 1;
  aarchvm::SoC::SecondaryBootMode secondary_boot_mode = aarchvm::SoC::SecondaryBootMode::AllStart;
  std::uint64_t init_sp = 0;
  std::optional<std::string> dtb_path;
  std::uint64_t dtb_addr = 0x40000000ull;
  std::optional<std::string> snapshot_load_path;
  std::optional<std::string> snapshot_save_path;
  std::optional<std::string> drive_path;
  std::vector<BinaryLoad> extra_bins;
  std::optional<std::string> stop_on_uart_pattern;
  bool predecode_enabled = true;
  bool framebuffer_sdl = true;
  bool framebuffer_sdl_specified = false;
};

std::uint64_t parse_u64(const std::string& text) {
  return std::stoull(text, nullptr, 0);
}

bool env_enabled(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0' && value[0] != '0';
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

std::vector<ByteEvent> parse_byte_script_env(const char* env_name) {
  std::vector<ByteEvent> events;
  const char* script = std::getenv(env_name);
  if (script == nullptr || *script == '\0') {
    return events;
  }

  std::stringstream ss(script);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) {
      continue;
    }
    const std::size_t colon = item.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= item.size()) {
      std::cerr << "Invalid " << env_name << " item: " << item << '\n';
      continue;
    }
    events.push_back(ByteEvent{
        .step = parse_u64(item.substr(0, colon)),
        .byte = static_cast<std::uint8_t>(parse_u64(item.substr(colon + 1)) & 0xFFu),
    });
  }

  std::sort(events.begin(), events.end(), [](const ByteEvent& a, const ByteEvent& b) {
    return a.step < b.step;
  });
  return events;
}

void inject_scheduled_uart_rx(const std::vector<ByteEvent>& events,
                              std::size_t& next_event,
                              aarchvm::SoC& soc) {
  while (next_event < events.size() && events[next_event].step <= soc.steps()) {
    soc.inject_uart_rx(events[next_event].byte);
    ++next_event;
  }
}

void inject_scheduled_ps2_rx(const std::vector<ByteEvent>& events,
                             std::size_t& next_event,
                             aarchvm::SoC& soc) {
  while (next_event < events.size() && events[next_event].step <= soc.steps()) {
    soc.inject_ps2_rx(events[next_event].byte);
    ++next_event;
  }
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
      << "[-load <addr>] [-entry <pc>] [-steps <n>] [-sp <addr>] [-smp <n>] [-smp-mode <all|psci>] "
      << "[-dtb <file>] [-dtb-addr <addr>] [-segment <file@addr>]... "
      << "[-snapshot-load <file>] [-snapshot-save <file>] [-drive <image.bin>] \n"
      << "[-stop-on-uart <text>] [-decode <fast|slow>] [-fb-sdl <on|off>]\n";
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
    } else if (key == "-smp") {
      opt.cpu_count = static_cast<std::size_t>(parse_u64(val));
      if (opt.cpu_count == 0) {
        std::cerr << "-smp must be >= 1\n";
        return std::nullopt;
      }
    } else if (key == "-smp-mode") {
      if (val == "all") {
        opt.secondary_boot_mode = aarchvm::SoC::SecondaryBootMode::AllStart;
      } else if (val == "psci") {
        opt.secondary_boot_mode = aarchvm::SoC::SecondaryBootMode::PsciOff;
      } else {
        std::cerr << "Invalid -smp-mode value (expected all or psci): " << val << '\n';
        return std::nullopt;
      }
    } else if (key == "-sp") {
      opt.init_sp = parse_u64(val);
    } else if (key == "-dtb") {
      opt.dtb_path = val;
    } else if (key == "-dtb-addr") {
      opt.dtb_addr = parse_u64(val);
    } else if (key == "-snapshot-load") {
      opt.snapshot_load_path = val;
    } else if (key == "-snapshot-save") {
      opt.snapshot_save_path = val;
    } else if (key == "-drive") {
      opt.drive_path = val;
    } else if (key == "-stop-on-uart") {
      opt.stop_on_uart_pattern = val;
    } else if (key == "-decode") {
      if (val == "fast") {
        opt.predecode_enabled = true;
      } else if (val == "slow") {
        opt.predecode_enabled = false;
      } else {
        std::cerr << "Invalid -decode value (expected fast or slow): " << val << '\n';
        return std::nullopt;
      }
    } else if (key == "-fb-sdl") {
      opt.framebuffer_sdl_specified = true;
      if (val == "on") {
        opt.framebuffer_sdl = true;
      } else if (val == "off") {
        opt.framebuffer_sdl = false;
      } else {
        std::cerr << "Invalid -fb-sdl value (expected on or off): " << val << '\n';
        return std::nullopt;
      }
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

  if (opt.bin_path.empty() && !opt.snapshot_load_path.has_value()) {
    std::cerr << "Missing required -bin or -snapshot-load\n";
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
    cfmakeraw(&t);
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

std::uint64_t piped_stdin_gap_steps() {
  const char* gap_env = std::getenv("AARCHVM_STDIN_RX_GAP");
  if (gap_env == nullptr || *gap_env == '\0') {
    return 2000u;
  }
  const auto gap = parse_u64(gap_env);
  return gap == 0 ? 1u : gap;
}

class SerialEscapeSequence {
public:
  [[nodiscard]] bool consume(std::uint8_t byte,
                             aarchvm::SoC& soc,
                             bool interactive_stdin,
                             std::deque<std::uint8_t>& buffered_bytes) {
    if (!interactive_stdin) {
      buffered_bytes.push_back(byte);
      return false;
    }

    if (!pending_ctrl_a_) {
      if (byte == kEscapePrefix) {
        pending_ctrl_a_ = true;
        return false;
      }
      soc.inject_uart_rx(byte);
      return false;
    }

    pending_ctrl_a_ = false;
    if (byte == 'x' || byte == 'X') {
      std::cerr << "Host serial escape Ctrl+A,x received; stopping simulation\n";
      return true;
    }
    if (byte == kEscapePrefix) {
      soc.inject_uart_rx(kEscapePrefix);
      return false;
    }

    soc.inject_uart_rx(kEscapePrefix);
    soc.inject_uart_rx(byte);
    return false;
  }

private:
  static constexpr std::uint8_t kEscapePrefix = 0x01u;
  bool pending_ctrl_a_ = false;
};

[[nodiscard]] bool pump_stdin_to_uart(aarchvm::SoC& soc,
                                      bool interactive_stdin,
                                      std::deque<std::uint8_t>& buffered_bytes,
                                      SerialEscapeSequence& escape_sequence) {
  std::uint8_t buf[64];
  while (true) {
    const ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
      for (ssize_t i = 0; i < n; ++i) {
        if (escape_sequence.consume(buf[i], soc, interactive_stdin, buffered_bytes)) {
          return true;
        }
      }
      continue;
    }
    if (n == 0) {
      return false;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    return false;
  }
}

void inject_buffered_stdin_to_uart(std::deque<std::uint8_t>& buffered_bytes,
                                   std::uint64_t gap_steps,
                                   std::uint64_t& next_inject_step,
                                   aarchvm::SoC& soc) {
  static constexpr std::size_t kMaxInflightRxBytes = 16u;
  while (!buffered_bytes.empty() &&
         next_inject_step <= soc.steps() &&
         soc.uart_rx_fifo_size() < kMaxInflightRxBytes) {
    soc.inject_uart_rx(buffered_bytes.front());
    buffered_bytes.pop_front();
    next_inject_step += gap_steps;
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

  aarchvm::SoC soc(opt.cpu_count);
  const bool debug_slow_mode = env_enabled("AARCHVM_DEBUG_SLOW");
  soc.set_secondary_boot_mode(opt.secondary_boot_mode);
  if (opt.framebuffer_sdl_specified) {
    soc.set_framebuffer_sdl_enabled(opt.framebuffer_sdl);
  }
  soc.set_predecode_enabled(debug_slow_mode ? false : opt.predecode_enabled);
  if (opt.stop_on_uart_pattern.has_value()) {
    soc.set_stop_on_uart_pattern(*opt.stop_on_uart_pattern);
  } else if (const char* stop_on_uart = std::getenv("AARCHVM_STOP_ON_UART"); stop_on_uart != nullptr) {
    soc.set_stop_on_uart_pattern(std::string(stop_on_uart));
  }

  if (opt.snapshot_load_path.has_value()) {
    if (!soc.load_snapshot(*opt.snapshot_load_path)) {
      std::cerr << "Failed to load snapshot: " << *opt.snapshot_load_path << '\n';
      return 1;
    }
  } else {
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
  }

  if (opt.drive_path.has_value()) {
    std::vector<std::uint8_t> drive;
    if (!read_binary_file(*opt.drive_path, drive)) {
      std::cerr << "Failed to read drive image: " << *opt.drive_path << '\n';
      return 1;
    }
    if (!soc.load_block_image(drive)) {
      std::cerr << "Failed to attach drive image: " << *opt.drive_path << '\n';
      return 1;
    }
  }

  const bool interactive_stdin = isatty(STDIN_FILENO);
  RawStdinGuard stdin_guard;
  const std::vector<ByteEvent> uart_rx_events = parse_byte_script_env("AARCHVM_UART_RX_SCRIPT");
  const std::vector<ByteEvent> ps2_rx_events = parse_byte_script_env("AARCHVM_PS2_RX_SCRIPT");
  const std::uint64_t stdin_gap_steps = piped_stdin_gap_steps();
  std::deque<std::uint8_t> buffered_stdin_uart;
  SerialEscapeSequence serial_escape_sequence;
  std::uint64_t next_stdin_inject_step = soc.steps();
  std::size_t next_uart_rx_event = 0;
  std::size_t next_ps2_rx_event = 0;
  std::size_t remaining = opt.max_steps;
  while (remaining > 0) {
    if (pump_stdin_to_uart(soc, interactive_stdin, buffered_stdin_uart, serial_escape_sequence)) {
      break;
    }
    if (!interactive_stdin) {
      inject_buffered_stdin_to_uart(buffered_stdin_uart, stdin_gap_steps, next_stdin_inject_step, soc);
    }
    inject_scheduled_uart_rx(uart_rx_events, next_uart_rx_event, soc);
    inject_scheduled_ps2_rx(ps2_rx_events, next_ps2_rx_event, soc);

    std::size_t run_chunk = 200000u;

    if (!interactive_stdin && !buffered_stdin_uart.empty() && next_stdin_inject_step > soc.steps()) {
      const std::uint64_t until_inject = next_stdin_inject_step - soc.steps();
      run_chunk = std::min<std::size_t>(run_chunk, static_cast<std::size_t>(until_inject));
    }
    if (next_uart_rx_event < uart_rx_events.size() && uart_rx_events[next_uart_rx_event].step > soc.steps()) {
      const std::uint64_t until_inject = uart_rx_events[next_uart_rx_event].step - soc.steps();
      run_chunk = std::min<std::size_t>(run_chunk, static_cast<std::size_t>(until_inject));
    }
    if (next_ps2_rx_event < ps2_rx_events.size() && ps2_rx_events[next_ps2_rx_event].step > soc.steps()) {
      const std::uint64_t until_inject = ps2_rx_events[next_ps2_rx_event].step - soc.steps();
      run_chunk = std::min<std::size_t>(run_chunk, static_cast<std::size_t>(until_inject));
    }
    if (run_chunk == 0u) {
      continue;
    }
    const std::size_t n = std::min(run_chunk, remaining);
    if (!soc.run(n)) {
      std::cerr << "Simulation stopped unexpectedly\n";
      return 1;
    }
    remaining -= n;
    if (soc.stop_requested()) {
      break;
    }
  }

  if (const char* dump_post_addr_env = std::getenv("AARCHVM_DUMP_POST_ADDR"); dump_post_addr_env != nullptr) {
    const std::uint64_t dump_addr = parse_u64(std::string(dump_post_addr_env));
    const std::size_t dump_len =
        (std::getenv("AARCHVM_DUMP_POST_LEN") != nullptr)
            ? static_cast<std::size_t>(parse_u64(std::string(std::getenv("AARCHVM_DUMP_POST_LEN"))))
            : 64u;
    dump_bytes(soc, dump_addr, dump_len, "post-run");
  }
  if (opt.snapshot_save_path.has_value() && !soc.save_snapshot(*opt.snapshot_save_path)) {
    std::cerr << "Failed to save snapshot: " << *opt.snapshot_save_path << "\n";
    return 1;
  }
  if (std::getenv("AARCHVM_PRINT_SUMMARY") != nullptr) {
    std::cerr << "SUMMARY: steps=" << soc.steps()
              << " pc=0x" << std::hex << soc.pc() << std::dec << '\n';
  }
  if (std::getenv("AARCHVM_PRINT_TIMER_SUMMARY") != nullptr) {
    std::cerr << "TIMER-SUMMARY counter=0x" << std::hex << soc.timer_counter()
              << " cntv_ctl=0x" << soc.timer_cntv_ctl()
              << " cntv_cval=0x" << soc.timer_cntv_cval()
              << " cntv_tval=0x" << soc.timer_cntv_tval()
              << " cntp_ctl=0x" << soc.timer_cntp_ctl()
              << " cntp_cval=0x" << soc.timer_cntp_cval()
              << " cntp_tval=0x" << soc.timer_cntp_tval()
              << std::dec << '\n';
  }
  if (std::getenv("AARCHVM_PRINT_IRQ_SUMMARY") != nullptr) {
    std::cerr << "IRQ-SUMMARY masked=" << (soc.irq_masked() ? 1 : 0)
              << " gic27(p/e)=" << (soc.gic_pending(27) ? 1 : 0) << '/' << (soc.gic_enabled(27) ? 1 : 0)
              << " gic30(p/e)=" << (soc.gic_pending(30) ? 1 : 0) << '/' << (soc.gic_enabled(30) ? 1 : 0)
              << " gic33(p/e)=" << (soc.gic_pending(33) ? 1 : 0) << '/' << (soc.gic_enabled(33) ? 1 : 0)
              << " depth=" << soc.exception_depth()
              << " wfi=" << (soc.cpu_waiting_for_interrupt() ? 1 : 0)
              << std::dec << '\n';
  }
  if (std::getenv("AARCHVM_PRINT_UART_SUMMARY") != nullptr) {
    std::cerr << "UART-SUMMARY tx=" << soc.uart_tx_count()
              << " reads=" << soc.uart_mmio_reads()
              << " writes=" << soc.uart_mmio_writes()
              << " config_writes=" << soc.uart_config_writes()
              << " id_reads=" << soc.uart_id_reads()
              << " rx_injected=" << soc.uart_rx_injected_count()
              << " fifo=" << soc.uart_rx_fifo_size()
              << " cr=0x" << std::hex << soc.uart_cr()
              << " imsc=0x" << soc.uart_imsc()
              << " ris=0x" << soc.uart_ris()
              << " irq=" << std::dec << ((soc.uart_ris() & soc.uart_imsc()) != 0 ? 1 : 0)
              << " pstate=0x" << std::hex << soc.pstate_bits()
              << " irq_masked=" << std::dec << soc.irq_masked()
              << " exc_depth=" << soc.exception_depth()
              << " wfi=" << soc.cpu_waiting_for_interrupt()
              << " wfe=" << soc.cpu_waiting_for_event()
              << " icc_igrpen1=" << soc.icc_igrpen1_el1()
              << " vbar=0x" << std::hex << soc.vbar_el1()
              << " gicd_ctlr=0x" << soc.gicd_ctlr()
              << " gic33_p=" << std::dec << soc.gic_pending(33)
              << " gic33_e=" << soc.gic_enabled(33)
              << "\n";
  }
  if (std::getenv("AARCHVM_PRINT_REGS") != nullptr) {
    std::cerr << std::hex << std::setfill('0');
    for (std::uint32_t i = 0; i < 31; ++i) {
      std::cerr << "REG x" << std::dec << i << std::hex
                << "=0x" << std::setw(16) << soc.x(i) << '\n';
    }
    std::cerr << "REG sp=0x" << std::setw(16) << soc.sp() << '\n';
    std::cerr << std::dec << std::setfill(' ');
  }

  return 0;
}
