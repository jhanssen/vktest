#ifndef RECT_H
#define RECT_H

#include <cstdint>

class Rect
{
public:
    float x { 0.f };
    float y { 0.f };
    float width { 0.f };
    float height { 0.f };

    bool isValid() const { return width > 0.f && height > 0.f; }
};

#endif // RECT_H
