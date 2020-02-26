#ifndef DECODER_H
#define DECODER_H

#include "Buffer.h"
#include <string>

class Decoder
{
public:
    enum Format { Format_Auto, Format_WEBP, Format_PNG, Format_Invalid};
    Decoder(Format format) : mFormat(format) { }

    struct Image
    {
        uint32_t width, height;
        uint32_t bpl;
        uint8_t depth;
        bool alpha;
        Buffer data;
    };

    Image decode(const std::string& path) const;

private:
    Format mFormat;
};

#endif
