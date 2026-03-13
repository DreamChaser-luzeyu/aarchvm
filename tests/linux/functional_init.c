#include <stdio.h>
#include <unistd.h>

int main(void) {
  char *const argv[] = {"sh", "/bin/run_functional_suite", NULL};
  execv("/bin/sh", argv);
  perror("execv /bin/sh /bin/run_functional_suite");
  return 127;
}
