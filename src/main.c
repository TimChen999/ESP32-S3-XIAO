#include <stdio.h>

void app_main(void) {
  while (1) {
    printf("ESP32 test firmware running\n");
    for (volatile unsigned long i = 0; i < 8000000UL; i++) {
    }
  }
}