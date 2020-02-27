#include "Decoder.h"
#include "Fetch.h"
#include <webp/decode.h>
#include <png.h>
#include <turbojpeg.h>
#include <assert.h>

static inline Decoder::Format guessFormat(Decoder::Format from, const Buffer& data)
{
    if (from != Decoder::Format_Auto)
        return from;
    if (data.size() >= 4 && memcmp(data.data(), "\211PNG", 4) == 0)
        return Decoder::Format_PNG;
    if (data.size() >= 10 && memcmp(data.data() + 6, "JFIF", 4) == 0)
        return Decoder::Format_JPEG;
    if (data.size() >= 16 && memcmp(data.data() + 8, "WEBPVP8", 7) == 0)
        return Decoder::Format_WEBP;
    return Decoder::Format_Invalid;
}

static inline std::shared_ptr<Image> decodePNG(const Buffer& data)
{
    auto png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return std::shared_ptr<Image>();
    }
    auto info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        return std::shared_ptr<Image>();
    }

    auto img = std::make_shared<Image>();

    if (setjmp(png_jmpbuf(png_ptr))) {
        return std::shared_ptr<Image>();
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

    img->width = png_get_image_width(png_ptr, info_ptr);
    img->height = png_get_image_height(png_ptr, info_ptr);
    img->depth = png_get_bit_depth(png_ptr, info_ptr);
    switch (png_get_color_type(png_ptr, info_ptr)) {
    case 4:
    case 6:
        img->alpha = true;
        break;
    default:
        img->alpha = false;
        break;
    }

    png_read_update_info(png_ptr, info_ptr);
    if (setjmp(png_jmpbuf(png_ptr))) {
        return std::shared_ptr<Image>();
    }

    const auto rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    png_bytep* row_pointers = static_cast<png_bytep*>(png_malloc(png_ptr, img->height * sizeof(png_bytep)));
    for (int y = 0; y < img->height; ++y)
        row_pointers[y] = static_cast<png_bytep>(png_malloc(png_ptr, rowBytes));

    png_read_image(png_ptr, row_pointers);

    for (int y = 0; y < img->height; ++y)
        img->data.append(row_pointers[y], rowBytes);

    png_read_end(png_ptr, info_ptr);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    img->bpl = rowBytes;

    return img;
}

static inline std::shared_ptr<Image> decodeWEBP(const Buffer& data)
{
    WebPBitstreamFeatures features;
    if (WebPGetFeatures(data.data(), data.size(), &features) != VP8_STATUS_OK) {
        return std::shared_ptr<Image>();
    }
    auto img = std::make_shared<Image>();
    img->width = features.width;
    img->bpl = features.width * 4;
    img->height = features.height;
    img->alpha = features.has_alpha;
    img->depth = 32;

    img->data.resize(img->width * img->height * 4);
    auto out = WebPDecodeRGBAInto(data.data(), data.size(), img->data.data(), img->data.size(), img->width * 4);
    if (out == nullptr) {
        return std::shared_ptr<Image>();
    }

    return img;
}

static inline std::shared_ptr<Image> decodeJPEG(const Buffer& data)
{
    auto handle = tjInitDecompress();
    int width, height;
    auto bytes = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data()));
    if (tjDecompressHeader(handle, bytes, data.size(), &width, &height) != 0)
        return std::shared_ptr<Image>();

    auto img = std::make_shared<Image>();
    img->width = width;
    img->bpl = width * 4;
    img->height = height;
    img->alpha = false;
    img->depth = 32;

    img->data.resize(width * height * 4);

    if (tjDecompress2(handle, bytes, data.size(), img->data.data(), width, 0, height, TJPF_RGBA, TJFLAG_FASTDCT) != 0)
        return std::shared_ptr<Image>();

    tjDestroy(handle);

    return img;
}

std::shared_ptr<Image> Decoder::decode(const std::string& path)
{
    // first, check if this path is in the cache
    const auto it = mCache.find(path);
    if (it != mCache.end()) {
        return it->second;
    }

    const Buffer data = Fetch::fetch(path);
    if (data.empty())
        return std::shared_ptr<Image>();
    const auto format = guessFormat(mFormat, data);
    assert(format != Format_Auto);
    switch (format) {
    case Format_PNG: {
        auto img = decodePNG(data);
        mCache.insert(std::make_pair(path, img));
        return img; }
    case Format_WEBP: {
        auto img = decodeWEBP(data);
        mCache.insert(std::make_pair(path, img));
        return img; }
    case Format_JPEG: {
        auto img = decodeJPEG(data);
        mCache.insert(std::make_pair(path, img));
        return img; }
    default:
        break;
    }
    return std::shared_ptr<Image>();
}
