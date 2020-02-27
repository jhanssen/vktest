#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include "Buffer.h"

struct Image
{
    uint32_t width, height;
    uint32_t bpl;
    uint8_t depth;
    bool alpha;
    Buffer data;
};

#endif // IMAGE_H
