#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef uint64_t (*guest_fn_t)(void);

static const char *resolve_busybox_path(void) {
  static const char *const candidates[] = {
      "/bin/busybox",
      "./out/initramfs-usertests-root/bin/busybox",
      "out/initramfs-usertests-root/bin/busybox",
  };
  for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
    if (access(candidates[i], X_OK) == 0) {
      return candidates[i];
    }
  }
  return "/bin/busybox";
}

static uint32_t encode_movz_x0(uint16_t imm16) {
  return 0xD2800000u | ((uint32_t)imm16 << 5);
}

static void emit_function(void *buf, uint16_t value) {
  uint32_t *insn = (uint32_t *)buf;
  insn[0] = encode_movz_x0(value);
  insn[1] = 0xD65F03C0u; // ret
}

int main(void) {
  static const uint16_t values[] = {0x11, 0x22, 0x33, 0x55, 0xAA, 0x1234};
  const long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
    fprintf(stderr, "invalid page size: %ld\n", page_size);
    return 1;
  }

  void *code = mmap(NULL,
                    (size_t)page_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0);
  if (code == MAP_FAILED) {
    perror("mmap");
    return 1;
  }

  guest_fn_t fn = (guest_fn_t)code;
  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
    if (mprotect(code, (size_t)page_size, PROT_READ | PROT_WRITE) != 0) {
      perror("mprotect rw");
      return 2;
    }
    emit_function(code, values[i]);
    __builtin___clear_cache((char *)code, (char *)code + 8);

    if (mprotect(code, (size_t)page_size, PROT_NONE) != 0) {
      perror("mprotect none");
      return 3;
    }
    if (mprotect(code, (size_t)page_size, PROT_READ | PROT_EXEC) != 0) {
      perror("mprotect rx");
      return 4;
    }
    __builtin___clear_cache((char *)code, (char *)code + 8);

    const uint64_t got = fn();
    if (got != values[i]) {
      fprintf(stderr,
              "exec mismatch at iter %zu: got=0x%llx expected=0x%x\n",
              i,
              (unsigned long long)got,
              values[i]);
      return 5;
    }
  }

  pid_t child = fork();
  if (child < 0) {
    perror("fork");
    return 6;
  }
  if (child == 0) {
    const char *busybox = resolve_busybox_path();
    execl(busybox, "busybox", "true", (char *)NULL);
    perror("execl busybox true");
    _exit(127);
  }

  int status = 0;
  if (waitpid(child, &status, 0) < 0) {
    perror("waitpid");
    return 7;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "busybox true failed: status=0x%x\n", status);
    return 8;
  }

  puts("EXECVE-OK");
  printf("MPROTECT-EXEC PASS iters=%zu\n", sizeof(values) / sizeof(values[0]));
  return 0;
}
