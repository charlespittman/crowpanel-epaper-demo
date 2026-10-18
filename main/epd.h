#pragma once
#include <stdbool.h>

// Screen size in pixels
#define EPD_W 400
#define EPD_H 300

// Set up the pins, turn on the screen and wipe it white.
void epd_init(void);

// Everything below draws into a buffer in memory only.
// Nothing appears on the screen until you call a refresh function (see below).
void epd_clear(void);                                              // whole buffer white
void epd_rect(int x, int y, int w, int h, bool black);             // filled rectangle
void epd_text(int x, int y, const char *text, int scale, bool black);

// Send the buffer to the screen.
//   epd_refresh_full():    the screen flashes black and white, takes about a second,
//                          and clears any faint ghost of earlier pictures.
//   epd_refresh_partial(): quick and no flashing, but ghosts slowly build up, so
//                          do a full refresh now and then.
void epd_refresh_full(void);
void epd_refresh_partial(void);
