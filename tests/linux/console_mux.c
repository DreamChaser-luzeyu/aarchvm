#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/vt.h>
#endif

static int set_raw_mode(int fd) {
  struct termios tio;
  if (tcgetattr(fd, &tio) != 0) {
    return -1;
  }
  cfmakeraw(&tio);
  tio.c_cflag |= CREAD | CLOCAL;
  tio.c_oflag |= OPOST | ONLCR;
  tio.c_lflag &= ~(ECHO | ICANON | ISIG);
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
  return tcsetattr(fd, TCSANOW, &tio);
}

static int write_all(int fd, const void* buf, size_t len) {
  const unsigned char* p = (const unsigned char*)buf;
  while (len > 0) {
    ssize_t n = write(fd, p, len);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    p += (size_t)n;
    len -= (size_t)n;
  }
  return 0;
}

static int open_fb_console(const char* path) {
  int fd = open(path, O_WRONLY | O_NOCTTY);
  if (fd < 0) {
    return -1;
  }
#ifdef __linux__
  int tty0 = open("/dev/tty0", O_RDWR | O_NOCTTY);
  if (tty0 >= 0) {
    const char* digits = path + strlen(path);
    while (digits > path && digits[-1] >= '0' && digits[-1] <= '9') {
      --digits;
    }
    if (*digits != '\0') {
      int vt = atoi(digits);
      if (vt > 0) {
        ioctl(tty0, VT_ACTIVATE, vt);
        ioctl(tty0, VT_WAITACTIVE, vt);
      }
    }
    close(tty0);
  }
#endif
  return fd;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s <serial-tty> <fb-tty> <command> [args...]\n", argv[0]);
    return 2;
  }

  const char* serial_path = argv[1];
  const char* fb_path = argv[2];

  int serial_fd = open(serial_path, O_RDWR | O_NOCTTY);
  if (serial_fd < 0) {
    perror("open serial tty");
    return 1;
  }
  if (set_raw_mode(serial_fd) != 0) {
    perror("set raw mode");
    close(serial_fd);
    return 1;
  }

  int fb_fd = open_fb_console(fb_path);
  if (fb_fd < 0) {
    fb_fd = open("/dev/tty0", O_WRONLY | O_NOCTTY);
  }

  int master_fd = -1;
  pid_t child = forkpty(&master_fd, NULL, NULL, NULL);
  if (child < 0) {
    perror("forkpty");
    close(serial_fd);
    if (fb_fd >= 0) {
      close(fb_fd);
    }
    return 1;
  }

  if (child == 0) {
    execvp(argv[3], &argv[3]);
    perror("execvp");
    _exit(127);
  }

  struct pollfd pfds[2];
  pfds[0].fd = serial_fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = master_fd;
  pfds[1].events = POLLIN;

  unsigned char buf[512];
  int status = 0;
  bool done = false;
  while (!done) {
    if (waitpid(child, &status, WNOHANG) == child) {
      done = true;
      continue;
    }

    int rc = poll(pfds, 2, 50);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      kill(child, SIGTERM);
      waitpid(child, &status, 0);
      break;
    }
    if (rc == 0) {
      continue;
    }

    if ((pfds[0].revents & POLLIN) != 0) {
      ssize_t n = read(serial_fd, buf, sizeof(buf));
      if (n > 0) {
        if (write_all(master_fd, buf, (size_t)n) != 0) {
          done = true;
        }
      } else if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) {
        done = true;
      }
    }

    if ((pfds[1].revents & POLLIN) != 0) {
      ssize_t n = read(master_fd, buf, sizeof(buf));
      if (n > 0) {
        if (write_all(serial_fd, buf, (size_t)n) != 0) {
          done = true;
        }
        if (fb_fd >= 0) {
          (void)write_all(fb_fd, buf, (size_t)n);
        }
      } else if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) {
        done = true;
      }
    }
  }

  close(master_fd);
  close(serial_fd);
  if (fb_fd >= 0) {
    close(fb_fd);
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return 0;
}
