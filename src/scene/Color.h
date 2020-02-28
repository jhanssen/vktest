#ifndef COLOR_H
#define COLOR_H

class Color
{
public:
    float r { 0.f };
    float g { 0.f };
    float b { 0.f };
    float a { 0.f };

    bool isValid() const { return a != 0.f; }
};

#endif // COLOR_H
