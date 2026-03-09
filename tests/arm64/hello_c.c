#include <stdint.h>

static volatile uint32_t* const UART_DR = (volatile uint32_t*)0x09000000u;

int main(void) {
  UART_DR[0] = 'C';
  UART_DR[0] = '\n';
  return 0;
}
