#ifndef TEXT_H
#define TEXT_H

#include <string>
#include <cstdint>
#include "Color.h"

class Text
{
public:
    std::string contents;
    uint32_t size;
    Color color;
    bool bold, italic;
};

#endif // TEXT_H
