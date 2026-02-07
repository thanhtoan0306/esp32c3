/**
 * ESP32-C3 Supermini - Blink Built-in LED
 *
 * Built-in LED is connected to GPIO8 on this board.
 */

#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO    GPIO_NUM_8   // Built-in LED on ESP32-C3 Supermini
#define BLINK_MS    500

extern "C" void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        printf("LED ON\n");
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));

        gpio_set_level(LED_GPIO, 0);
        printf("LED OFF\n");
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}
