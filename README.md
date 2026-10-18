# CrowPanel 4.2" E-paper demo (ESP-IDF)

> **Warning: this code is AI-generated.** It was written with Claude (Anthropic's AI
> assistant) and has only been tested on one board, the version with the green
> circular sticker described below. It may contain mistakes. Read and understand it
> before you rely on it, and check it against your own hardware.

A counter demo for the Elecrow CrowPanel ESP32 4.2" E-paper HMI display
(ESP32-S3, 400 x 300, black and white).

- **UP / DOWN** buttons change the count
- **OK** button resets it to zero
- The red LED is on while the program runs
- Updates use **partial refresh**, so the screen doesn't flash. Every 10th update is a
  full refresh (the screen flashes) to clear faint ghost images. Change
  `FULL_REFRESH_EVERY` in `main/main.c` to adjust this.

## Board version matters

This code is for the board version with the **green circular sticker on the back**
(PCBA V1.2A). Its screen uses a different controller from the SSD1683 that Elecrow's
main readme and most third-party drivers describe. With the wrong init sequence the
screen stays blank and the BUSY pin never changes.

The register values in `main/epd.c` come from Elecrow's Arduino example
`arduino_A_green_circular_sticker_on_the_back` in
[their repo](https://github.com/Elecrow-RD/CrowPanel-ESP32-4.2-E-paper-HMI-Display-with-400-300).
If you have a different version of the board, check the other example folders there.

## Pins

| Function | GPIO |
|---|---|
| SPI clock | 12 |
| SPI data (MOSI) | 11 |
| Chip select | 45 |
| Data/command | 46 |
| Reset | 47 |
| Busy | 48 (low while refreshing) |
| Screen power | 7 (high = on) |
| Red LED | 41 |
| Buttons (low = pressed) | UP 6, DOWN 4, OK 5, MENU 2, EXIT 1 |

## Files

| File | What it does |
|---|---|
| `main/main.c` | The demo: buttons, counter, drawing the screen |
| `main/epd.c`, `main/epd.h` | Screen driver: SPI, setup, drawing into a memory buffer, refresh |
| `main/font5x7.h` | 5x7 pixel font, capitals only |

Drawing calls (`epd_rect`, `epd_text`) only change a buffer in memory.
Nothing shows on the screen until `epd_refresh_full()` or `epd_refresh_partial()` is called.

## Build and flash

Requires ESP-IDF 6.0.x. Target is `esp32s3`.

```bash
source ~/.espressif/tools/activate_idf_v6.0.2.sh
python3 $IDF_PATH/tools/idf.py set-target esp32s3
python3 $IDF_PATH/tools/idf.py build
python3 $IDF_PATH/tools/idf.py -p /dev/ttyUSB0 -b 230400 flash monitor
```

Use a baud rate of 230400 or lower when flashing. A flash at 460800 once dropped
mid-write and left the board in a boot loop.
