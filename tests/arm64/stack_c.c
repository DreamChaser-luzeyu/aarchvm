#include <stdint.h>

static volatile uint32_t* const UART_DR = (volatile uint32_t*)0x09000000u;

static int add3(int a, int b, int c) {
  int t = a + b;
  return t + c;
}

int main(void) {
  int x = 1;
  int y = 2;
  int z = add3(x, y, 3);
  if (z == 6) {
    UART_DR[0] = 'S';
  } else {
    UART_DR[0] = 'F';
  }
  UART_DR[0] = '\n';
  return 0;
}
