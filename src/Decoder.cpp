#include "Decoder.h"
#include <webp/decode.h>
#include <png.h>
#include <assert.h>

static inline Decoder::Format guessFormat(Decoder::Format from, const Buffer& data)
{
    if (from != Decoder::Format_Auto)
        return from;
    if (data.size() >= 4 && memcmp(data.data(), "\211PNG", 4) == 0)
        return Decoder::Format_PNG;
    if (data.size() >= 16 && memcmp(data.data() + 8, "WEBPVP8", 7) == 0)
        return Decoder::Format_WEBP;
    return Decoder::Format_Invalid;
}

static inline Decoder::Image decodePNG(const Buffer& data)
{
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return Decoder::Image();
    }
    auto info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        return Decoder::Image();
    }

    Decoder::Image img;

    if (setjmp(png_jmpbuf(png_ptr))) {
        return Decoder::Image();
    }
    struct PngData {
        const Buffer& data;
        size_t read;
    } pngData = { data, 0 };
    png_set_read_fn(png_ptr, &pngData, [](png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) -> void {
        png_voidp io_ptr = png_get_io_ptr(png_ptr);
        PngData* data = static_cast<PngData*>(io_ptr);
        // should handle out of bounds reads here
        memcpy(outBytes, data->data.data() + data->read, byteCountToRead);
        data->read += byteCountToRead;
    });
    png_set_sig_bytes(png_ptr, 0);
    png_read_info(png_ptr, info_ptr);

    img.width = png_get_image_width(png_ptr, info_ptr);
    img.height = png_get_image_height(png_ptr, info_ptr);
    img.depth = png_get_bit_depth(png_ptr, info_ptr);
    switch (png_get_color_type(png_ptr, info_ptr)) {
    case 4:
    case 6:
        img.alpha = true;
        break;
    default:
        img.alpha = false;
        break;
    }

    png_read_update_info(png_ptr, info_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        return Decoder::Image();
    }

    const auto rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    png_bytep* row_pointers = static_cast<png_bytep*>(png_malloc(png_ptr, img.height * sizeof(png_bytep)));
    for (int y = 0; y < img.height; ++y)
        row_pointers[y] = static_cast<png_bytep>(png_malloc(png_ptr, rowBytes));

    png_read_image(png_ptr, row_pointers);

    for (int y = 0; y < img.height; ++y)
        img.data.append(row_pointers[y], rowBytes);

    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    img.bpl = rowBytes;

    return img;
}

static inline Decoder::Image decodeWEBP(const Buffer& data)
{
    WebPBitstreamFeatures features;
    if (WebPGetFeatures(data.data(), data.size(), &features) != VP8_STATUS_OK) {
        return Decoder::Image();
    }
    Decoder::Image img;
    img.width = img.bpl = features.width;
    img.height = features.height;
    img.alpha = features.has_alpha;
    img.depth = 32;

    img.data.resize(img.width * img.height * 4);
    auto out = WebPDecodeRGBAInto(data.data(), data.size(), img.data.data(), img.data.size(), img.width * 4);
    if (out == nullptr) {
        return Decoder::Image();
    }

    return img;
}

Decoder::Image Decoder::decode(const std::string& path) const
{
    const Buffer data = Buffer::readFile(path);
    if (data.empty())
        return Image();
    const auto format = guessFormat(mFormat, data);
    assert(format != Format_Auto);
    switch (format) {
    case Format_PNG:
        return decodePNG(data);
    case Format_WEBP:
        return decodeWEBP(data);
    default:
        break;
    }
    return Image();
}
