#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_RED   GPIO_NUM_3   // D2
#define LED_GREEN GPIO_NUM_4   // D3
#define LED_BLUE  GPIO_NUM_5   // D4

static void configure_leds(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_RED) | (1ULL << LED_GREEN) | (1ULL << LED_BLUE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

void app_main(void)
{
    configure_leds();

    printf("\nHello, XIAO ESP32-S3!\n");
    printf("Welcome to Wokwi :-)\n");

    while (1) {
        printf("Red\n");
        gpio_set_level(LED_RED, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_RED, 0);

        printf("Green\n");
        gpio_set_level(LED_GREEN, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_GREEN, 0);

        printf("Blue\n");
        gpio_set_level(LED_BLUE, 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_BLUE, 0);
    }
}