/**
 * ESP32-C3 Supermini - Blink LED + Serial Echo
 * - LED nhấp nháy
 * - Nhập text qua terminal → hiển thị "Rep: <text>"
 */

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO    GPIO_NUM_8   // Built-in LED on ESP32-C3 Supermini
#define BLINK_MS    500
#define INPUT_BUF   128

static void input_task(void *arg)
{
    char buf[INPUT_BUF];
    int i = 0;
    printf("\nNhap text va Enter de hien thi (Rep: ...)\n");

    while (1) {
        int c = getchar();
        if (c != EOF && c != -1) {
            if (c == '\n' || c == '\r') {
                buf[i] = '\0';
                if (i > 0) {
                    printf("Rep: %s\n", buf);
                }
                i = 0;
            } else if (i < (int)sizeof(buf) - 1 && c >= 32) {
                buf[i++] = (char)c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void blink_task(void *arg)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));

        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}

extern "C" void app_main(void)
{
    xTaskCreate(blink_task, "blink", 2048, NULL, 1, NULL);
    xTaskCreate(input_task, "input", 3072, NULL, 1, NULL);
}
