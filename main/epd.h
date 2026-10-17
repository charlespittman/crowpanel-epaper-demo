#pragma once
#include <stdbool.h>

// Screen size in pixels
#define EPD_W 400
#define EPD_H 300

// Set up the pins, turn on the screen and wipe it white.
void epd_init(void);

// Everything below draws into a buffer in memory only.
// Nothing appears on the screen until you call epd_refresh().
void epd_clear(void);                                              // whole buffer white
void epd_rect(int x, int y, int w, int h, bool black);             // filled rectangle
void epd_text(int x, int y, const char *text, int scale, bool black);

// Send the buffer to the screen (takes about one second).
void epd_refresh(void);
