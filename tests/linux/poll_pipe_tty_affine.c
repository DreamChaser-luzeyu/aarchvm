#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
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
    puts("PIPETTY-AFFINE-SKIP");
    return 0;
  }

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    perror("pipe");
    return 1;
  }

  pid_t child = fork();
  if (child < 0) {
    perror("fork");
    return 1;
  }
  if (child == 0) {
    close(pipefd[0]);
    if (pin_to_cpu(0) != 0) {
      perror("sched_setaffinity child");
      _exit(10);
    }
    char chunk[128];
    memset(chunk, 'A', sizeof(chunk));
    for (int i = 0; i < 400; ++i) {
      ssize_t n = write(pipefd[1], chunk, sizeof(chunk));
      if (n != (ssize_t)sizeof(chunk)) {
        perror("write pipe");
        _exit(11);
      }
      usleep(5000);
    }
    close(pipefd[1]);
    _exit(0);
  }

  close(pipefd[1]);
  if (pin_to_cpu(1) != 0) {
    perror("sched_setaffinity parent");
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 1;
  }

  struct termios orig, raw;
  if (tcgetattr(STDIN_FILENO, &orig) != 0) {
    perror("tcgetattr");
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 1;
  }
  raw = orig;
  raw.c_lflag &= ~(ICANON | ECHO);
  raw.c_iflag &= ~(ICRNL | INLCR | IGNCR);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
    perror("tcsetattr");
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    return 1;
  }

  puts("PIPETTY-AFFINE-BEGIN cpu=1");
  fflush(stdout);

  struct pollfd pfds[2];
  pfds[0].fd = pipefd[0];
  pfds[0].events = POLLIN;
  pfds[1].fd = STDIN_FILENO;
  pfds[1].events = POLLIN;

  char buf[256];
  size_t total_pipe = 0;
  int pipe_open = 1;
  for (;;) {
    pfds[0].fd = pipe_open ? pipefd[0] : -1;
    pfds[0].events = pipe_open ? POLLIN : 0;
    pfds[0].revents = 0;
    pfds[1].revents = 0;
    int rc = poll(pfds, 2, -1);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      kill(child, SIGKILL);
      waitpid(child, NULL, 0);
      return 2;
    }
    if (pipe_open && (pfds[0].revents & (POLLIN | POLLHUP)) != 0) {
      ssize_t n = read(pipefd[0], buf, sizeof(buf));
      if (n < 0) {
        if (errno != EINTR) {
          perror("read pipe");
          tcsetattr(STDIN_FILENO, TCSANOW, &orig);
          kill(child, SIGKILL);
          waitpid(child, NULL, 0);
          return 3;
        }
      } else if (n == 0) {
        pipe_open = 0;
        close(pipefd[0]);
        puts("PIPETTY-PIPE-EOF");
        fflush(stdout);
      } else {
        total_pipe += (size_t)n;
        if ((total_pipe % 4096u) == 0u) {
          printf("PIPETTY-PIPE-BYTES:%zu\n", total_pipe);
          fflush(stdout);
        }
      }
    }
    if ((pfds[1].revents & POLLIN) != 0) {
      unsigned char ch = 0;
      ssize_t n = read(STDIN_FILENO, &ch, 1);
      if (n == 1) {
        printf("PIPETTY-AFFINE-OK:0x%02x pipe=%zu\n", ch, total_pipe);
        fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
        waitpid(child, NULL, 0);
        return 0;
      }
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n < 0) {
        perror("read tty");
      } else {
        fprintf(stderr, "short tty read: %zd\n", n);
      }
      tcsetattr(STDIN_FILENO, TCSANOW, &orig);
      kill(child, SIGKILL);
      waitpid(child, NULL, 0);
      return 4;
    }
  }
}
