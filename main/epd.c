// Driver for the CrowPanel 4.2" e-paper screen (400 x 300, black and white).
//
// The numbers sent to the screen (the "registers") come from Elecrow's Arduino
// example for the board version with the green circular sticker on the back.
// Other board versions use a different screen chip and need different numbers.

#include "epd.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "font5x7.h"

// ---- Pins (GPIO numbers on the ESP32-S3) ----
#define PIN_SCK 12  // SPI clock
#define PIN_MOSI 11 // SPI data out
#define PIN_CS 45   // chip select
#define PIN_DC 46   // low = next byte is a command, high = next byte is data
#define PIN_RST 47  // reset
#define PIN_BUSY 48 // screen pulls this low while it is refreshing
#define PIN_POWER 7 // high = power the screen on

// The picture lives in memory as 1 bit per pixel: 8 pixels per byte.
// 1 = white, 0 = black. Row by row, left to right, top bit first.
#define BYTES_PER_ROW (EPD_W / 8)
#define BUFFER_SIZE (BYTES_PER_ROW * EPD_H)
static uint8_t buffer[BUFFER_SIZE];

static spi_device_handle_t spi;

// ---------------------------------------------------------------
// Talking to the screen
// ---------------------------------------------------------------

// Send bytes over SPI. is_data says whether they are data (1) or a command (0).
static void send(int is_data, const uint8_t *bytes, size_t length)
{
    gpio_set_level(PIN_DC, is_data);
    spi_transaction_t transaction = {
        .length = length * 8, // length is counted in bits
        .tx_buffer = bytes,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &transaction));
}

static void command(uint8_t value) { send(0, &value, 1); }
static void data(uint8_t value) { send(1, &value, 1); }

// Wait for a refresh to finish (gives up after 15 seconds).
static void wait_until_ready(void)
{
    vTaskDelay(pdMS_TO_TICKS(20)); // let the screen start and pull BUSY low
    for (int i = 0; i < 1500 && gpio_get_level(PIN_BUSY) == 0; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Briefly pull the reset pin low to restart the screen chip.
static void reset_screen(void)
{
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Basic screen settings, sent once after a reset.
static void configure_screen(void)
{
    command(0x00);
    data(0x3F);
    data(0x4D);
    command(0x01);
    data(0x03);
    data(0x10);
    data(0x3F);
    data(0x3F);
    data(0x03);
    command(0x06);
    data(0x96);
    data(0x96);
    data(0x29);
    command(0x30);
    data(0x09);
    command(0x61);
    data(0x01);
    data(0x90);
    data(0x01);
    data(0x2C); // size: 0x190 x 0x12C = 400 x 300
    command(0x82);
    data(0x05);
    command(0x50);
    data(0x97);
    command(0x60);
    data(0x22);
    command(0xE3);
    data(0x88);
}

// A "waveform" tells the screen how to drive its black and white particles.
// There are 5 tables of 42 bytes (sent to commands 0x20 to 0x24).
// Only the first 7 bytes of each table are non-zero, so that is all we store.

// Full refresh: flashes black and white to wipe away old ghost images. Slow.
static const uint8_t waveform_full[5][7] = {
    {0x01, 0x14, 0x0A, 0x14, 0x00, 0x01, 0x01},
    {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01},
    {0x01, 0x54, 0x0A, 0x94, 0x00, 0x01, 0x01},
    {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01},
    {0x01, 0x94, 0x0A, 0x54, 0x00, 0x01, 0x01},
};

// Partial refresh: only changes the pixels that are different. No flashing.
static const uint8_t waveform_partial[5][7] = {
    {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00},
    {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00},
    {0x01, 0x94, 0x00, 0x00, 0x00, 0x01, 0x00},
    {0x01, 0x54, 0x00, 0x00, 0x00, 0x01, 0x00},
    {0x01, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00},
};

static void send_waveform(const uint8_t waveform[5][7])
{
    for (int i = 0; i < 5; i++)
    {
        uint8_t table[42] = {0};         // starts out all zeros
        memcpy(table, waveform[i], 7);   // fill in the first 7 bytes
        command(0x20 + i);
        send(1, table, sizeof table);
    }
}

// Send the picture in the buffer to the screen and start a refresh.
// The waveform decides whether it is a full or a partial refresh.
static void refresh(const uint8_t waveform[5][7])
{
    command(0x50);
    data(0xD7);
    command(0x13);
    send(1, buffer, BUFFER_SIZE); // the new picture
    send_waveform(waveform);
    command(0x17);
    data(0xA5); // start the refresh
    wait_until_ready();

    // Tell the screen what is showing now (command 0x10 = "previous picture").
    // A partial refresh compares the previous and the new picture, and only
    // changes the pixels that are different.
    command(0x10);
    send(1, buffer, BUFFER_SIZE);
}

// ---------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------

void epd_init(void)
{
    // Output pins
    int outputs[] = {PIN_DC, PIN_RST, PIN_POWER};
    for (int i = 0; i < 3; i++)
    {
        gpio_reset_pin(outputs[i]);
        gpio_set_direction(outputs[i], GPIO_MODE_OUTPUT);
    }
    gpio_reset_pin(PIN_BUSY);
    gpio_set_direction(PIN_BUSY, GPIO_MODE_INPUT);

    // Power the screen on and give it time to settle
    gpio_set_level(PIN_POWER, 1);
    vTaskDelay(pdMS_TO_TICKS(200));

    // Set up the SPI bus (MOSI + clock only, the screen never sends data back)
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUFFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t device = {
        .clock_speed_hz = 4 * 1000 * 1000, // 4 MHz
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &device, &spi));

    reset_screen();
    configure_screen();

    // Start with a white screen. Command 0x10 holds the "previous picture"
    // (white), and the refresh then sends the new picture.
    epd_clear();
    command(0x10);
    send(1, buffer, BUFFER_SIZE);
    epd_refresh_full();
}

void epd_clear(void)
{
    memset(buffer, 0xFF, BUFFER_SIZE); // all bits 1 = all white
}

// Set one pixel in the buffer.
static void set_pixel(int x, int y, bool black)
{
    if (x < 0 || y < 0 || x >= EPD_W || y >= EPD_H)
    {
        return; // off the screen, ignore
    }
    uint8_t *byte = &buffer[y * BYTES_PER_ROW + x / 8]; // the byte holding this pixel
    uint8_t bit = 0x80 >> (x % 8);                      // which bit in that byte
    if (black)
    {
        *byte &= ~bit; // black = 0
    }
    else
    {
        *byte |= bit; // white = 1
    }
}

void epd_rect(int x, int y, int w, int h, bool black)
{
    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            set_pixel(x + col, y + row, black);
        }
    }
}

// Draw text with a 5x7 pixel font. scale = 2 makes each font pixel 2x2, and so on.
void epd_text(int x, int y, const char *text, int scale, bool black)
{
    // After each letter, move right by 5 pixels (letter width) + 1 pixel (gap).
    for (; *text != '\0'; text++, x += 6 * scale)
    {
        char c = *text;
        if (c >= 'a' && c <= 'z')
        {
            c -= 32; // the font only has capitals
        }
        if (c < ' ' || c > 'Z')
        {
            c = '?'; // characters we don't have
        }

        const uint8_t *glyph = font5x7[c - ' ']; // 5 bytes, one per column
        for (int col = 0; col < 5; col++)
        {
            for (int row = 0; row < 7; row++)
            {
                // Is this pixel of the letter switched on?
                if (glyph[col] & (1 << row))
                {
                    epd_rect(x + col * scale, y + row * scale, scale, scale, black);
                }
            }
        }
    }
}

void epd_refresh_full(void)
{
    refresh(waveform_full);
}

void epd_refresh_partial(void)
{
    refresh(waveform_partial);
}
