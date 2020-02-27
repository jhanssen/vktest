#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include "Buffer.h"

struct Image
{
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t bpl { 0 };
    uint8_t depth { 0 };
    bool alpha { false };
    Buffer data;
};

#endif // IMAGE_H
