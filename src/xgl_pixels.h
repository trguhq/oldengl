/* MIT licensed. Private, portable RGBA-to-XGL memory-raster conversion. */
#ifndef XGLMESA_PIXELS_H
#define XGLMESA_PIXELS_H

#include <stddef.h>

/* unsigned int must have exactly 32 bits; destination is XGL-owned storage. */
int xglmesa_pack_rgba(void *destination, const unsigned char *rgba,
                     size_t width, size_t height,
                     unsigned int red_shift, unsigned int green_shift,
                     unsigned int blue_shift);

#endif
