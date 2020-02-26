#ifndef DECODER_H
#define DECODER_H

#include "Buffer.h"
#include <string>

class Decoder
{
public:
    enum Format { Format_Auto, Format_WEBP, Format_PNG };
    Decoder(Format format);

    Buffer decode(const std::string& path) const;
};

#endif
