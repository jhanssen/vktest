#ifndef DECODER_H
#define DECODER_H

#include "Buffer.h"
#include "Image.h"
#include <string>
#include <memory>
#include <unordered_map>

class Decoder
{
public:
    enum Format { Format_Auto, Format_WEBP, Format_PNG, Format_Invalid};
    Decoder(Format format) : mFormat(format) { }

    std::shared_ptr<Image> decode(const std::string& path);

private:
    Format mFormat;
    std::unordered_map<std::string, std::shared_ptr<Image> > mCache;
};

#endif
