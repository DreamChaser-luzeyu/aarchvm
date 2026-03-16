#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
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
    puts("READTTY-AFFINE-SKIP");
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
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    perror("tcsetattr");
    return 1;
  }
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags >= 0) {
    (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  }
  puts("READTTY-AFFINE-BEGIN cpu=1");
  fflush(stdout);
  for (unsigned long i = 0; i < 50000000UL; ++i) {
    unsigned char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1) {
      printf("READTTY-AFFINE-OK:0x%02x iter=%lu\n", ch, i);
      fflush(stdout);
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      return 0;
    }
    if (n < 0 && errno != EAGAIN && errno != EINTR) {
      perror("read");
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      return 2;
    }
  }
  puts("READTTY-AFFINE-TIMEOUT");
  fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSANOW, &orig);
  return 3;
}
