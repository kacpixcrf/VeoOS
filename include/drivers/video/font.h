#ifndef VEOOS_DRIVERS_VIDEO_FONT_H
#define VEOOS_DRIVERS_VIDEO_FONT_H

#include "util/types.h"

#define FONT_GLYPH_WIDTH 8
#define FONT_GLYPH_HEIGHT 16

const uint8_t* font_get_glyph(unsigned char character);

#endif
