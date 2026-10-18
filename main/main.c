// Demo: a counter on the e-paper screen.
//   UP button   = count up
//   DOWN button = count down
//   OK button   = back to zero

#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd.h"

// Button pins. They read 0 while pressed and 1 while released.
#define BUTTON_UP   6
#define BUTTON_DOWN 4
#define BUTTON_OK   5

// The red LED on the board. High = on.
#define LED_PIN 41

// Do a full (flashing) refresh once every this many screen updates.
#define FULL_REFRESH_EVERY 10

static void button_init(int pin)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);   // reads 1 unless the button pulls it to 0
}

// Returns true once per press: waits for the button to be released first.
static bool button_pressed(int pin)
{
    if (gpio_get_level(pin) != 0) {
        return false;                            // not pressed
    }
    while (gpio_get_level(pin) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));           // wait until released (also debounces)
    }
    return true;
}

static void draw_screen(int count)
{
    char text[32];

    epd_clear();

    // Title bar: black rectangle with white text on top
    epd_rect(0, 0, EPD_W, 40, true);
    epd_text(12, 8, "CROWPANEL 4.2 EINK", 3, false);

    // Frame: a black rectangle with a slightly smaller white one inside
    epd_rect(10, 52, 380, 238, true);
    epd_rect(12, 54, 376, 234, false);

    // The counter, in big letters
    snprintf(text, sizeof text, "COUNT: %d", count);
    epd_text(24, 100, text, 4, true);

    epd_text(24, 160, "UP/DOWN: CHANGE COUNT", 2, true);
    epd_text(24, 184, "OK: RESET TO ZERO", 2, true);

    // Strip of black squares along the bottom
    for (int i = 0; i < 20; i += 2) {
        epd_rect(24 + i * 18, 250, 18, 18, true);
    }

    // Nothing above shows up until a refresh. Partial refreshes don't flash,
    // but faint ghosts of old pictures build up. So every few updates we do
    // a full refresh, which flashes the screen and wipes the ghosts away.
    static int updates = 0;                      // "static": keeps its value between calls
    if (updates % FULL_REFRESH_EVERY == 0) {
        epd_refresh_full();
    } else {
        epd_refresh_partial();
    }
    updates++;
}

void app_main(void)
{
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 1);                  // LED stays on while the program runs

    button_init(BUTTON_UP);
    button_init(BUTTON_DOWN);
    button_init(BUTTON_OK);

    epd_init();

    int count = 0;
    draw_screen(count);

    while (true) {
        if (button_pressed(BUTTON_UP)) {
            count++;
            draw_screen(count);
        }
        if (button_pressed(BUTTON_DOWN)) {
            count--;
            draw_screen(count);
        }
        if (button_pressed(BUTTON_OK)) {
            count = 0;
            draw_screen(count);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
