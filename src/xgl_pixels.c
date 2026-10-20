/* MIT licensed. See LICENSE. */
#include "xgl_pixels.h"
#include <limits.h>
#include <string.h>

int
xglmesa_pack_rgba(void *destination, const unsigned char *rgba,
                  size_t width, size_t height,
                  unsigned int red_shift, unsigned int green_shift,
                  unsigned int blue_shift)
{
    size_t x, y;
    const unsigned char *source;
    unsigned char *output = (unsigned char *)destination;
    unsigned int word;

    if (UINT_MAX != 0xffffffffU || !destination || !rgba ||
        width == 0 || height == 0 || width > ((size_t)-1) / height / 4 ||
        red_shift > 24 || green_shift > 24 || blue_shift > 24 ||
        red_shift % 8 || green_shift % 8 || blue_shift % 8 ||
        red_shift == green_shift || red_shift == blue_shift ||
        green_shift == blue_shift)
        return 0;

    /* OpenGL's origin is lower-left; XGL memory rasters begin at upper-left. */
    for (y = 0; y < height; ++y) {
        source = rgba + (height - y - 1) * width * 4;
        for (x = 0; x < width; ++x) {
            word = ((unsigned int)source[0] << red_shift) |
                   ((unsigned int)source[1] << green_shift) |
                   ((unsigned int)source[2] << blue_shift);
            /* Xgl_usgn32 may be unsigned long on 32-bit Solaris. */
            memcpy(output, &word, sizeof(word));
            output += sizeof(word);
            source += 4;
        }
    }
    return 1;
}
