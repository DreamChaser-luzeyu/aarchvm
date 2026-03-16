#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int pin_to_cpu(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}

int main(void) {
  long nproc = sysconf(_SC_NPROCESSORS_ONLN);
  if (nproc < 2) {
    puts("POLLTTY-AFFINE-SKIP");
    return 0;
  }
  if (pin_to_cpu(1) != 0) {
    perror("sched_setaffinity");
    return 1;
  }

  struct termios orig, raw;
  if (tcgetattr(STDIN_FILENO, &orig) != 0) {
    perror("tcgetattr");
    return 1;
  }
  raw = orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_iflag &= ~(ICRNL | INLCR | IGNCR);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    perror("tcsetattr");
    return 1;
  }

  puts("POLLTTY-AFFINE-BEGIN cpu=1");
  fflush(stdout);

  struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
  for (;;) {
    int rc = poll(&pfd, 1, -1);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      return 2;
    }
    if ((pfd.revents & POLLIN) != 0) {
      unsigned char ch = 0;
      ssize_t n = read(STDIN_FILENO, &ch, 1);
      if (n == 1) {
        printf("POLLTTY-AFFINE-OK:0x%02x\n", ch);
        fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return 0;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n < 0) {
        perror("read");
      } else {
        fprintf(stderr, "short read: %zd\n", n);
      }
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      return 3;
    }
  }
}
