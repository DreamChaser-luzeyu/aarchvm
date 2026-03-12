#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  puts("FPSIMD_SELFTEST_INIT");
  fflush(stdout);

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return 127;
  }
  if (pid == 0) {
    execl("/bin/fpsimd_selftest", "fpsimd_selftest", (char*)NULL);
    perror("execl /bin/fpsimd_selftest");
    _exit(127);
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    perror("waitpid");
    return 127;
  }
  if (WIFEXITED(status)) {
    printf("FPSIMD_SELFTEST_RC=%d\n", WEXITSTATUS(status));
    fflush(stdout);
  } else if (WIFSIGNALED(status)) {
    printf("FPSIMD_SELFTEST_SIG=%d\n", WTERMSIG(status));
    fflush(stdout);
  } else {
    puts("FPSIMD_SELFTEST_RC=255");
    fflush(stdout);
  }

  for (;;) {
    sleep(1);
  }
}
