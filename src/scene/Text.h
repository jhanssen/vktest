#ifndef TEXT_H
#define TEXT_H

#include <string>
#include <cstdint>
#include "Color.h"

class Text
{
public:
    std::string contents;
    Color color;
    uint32_t size { 0 };
    bool bold { false };
    bool italic { false };
};

#endif // TEXT_H
