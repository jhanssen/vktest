#include "Rect.h"
#include <math.h>

Rect Rect::fromPoint(hb_position_t x, hb_position_t y)
{
    return {
        (1 / 64.f) * x,
        (1 / 64.f) * y,
        0.f, 0.f
    };
}

Rect Rect::fromRect(hb_position_t x, hb_position_t y, hb_position_t w, hb_position_t h)
{
    return {
        (1 / 64.f) * x,
        (1 / 64.f) * y,
        (1 / 64.f) * w,
        (1 / 64.f) * h
    };
}

Rect Rect::integralized() const
{
    return {
        floorf(x),
        floorf(y),
        ceilf(width),
        ceilf(height)
    };
}

void Rect::unite(const Rect& other)
{
    if (other.x < x)
        x = other.x;
    if (other.y < y)
        y = other.y;
    if (other.x + other.width > x + width)
        width += (other.x + other.width) - (x + width);
    if (other.y + other.height > y + height)
        height += (other.y + other.height) - (y + height);
}
