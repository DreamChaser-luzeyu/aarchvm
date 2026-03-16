#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(void) {
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

  puts("POLLTTY-BEGIN");
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
        printf("POLLTTY-OK:0x%02x\n", ch);
        fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        return 0;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      perror("read");
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      return 3;
    }
  }
}
