#ifndef RECT_H
#define RECT_H

#include <cstdint>
#include <hb.h>

class Rect
{
public:
    float x { 0.f };
    float y { 0.f };
    float width { 0.f };
    float height { 0.f };

    bool isValid() const { return width > 0.f && height > 0.f; }

    Rect integralized() const;
    void unite(const Rect& other);

    static Rect fromPoint(hb_position_t x, hb_position_t y);
    static Rect fromRect(hb_position_t x, hb_position_t y, hb_position_t w, hb_position_t h);
};

#endif // RECT_H
