#include "RenderText.h"
#include "Render.h"
#include <text/Layout.h>
#include <Utils.h>
#include <msdfgen.h>

constexpr uint32_t ImageWidth = 1024;
constexpr uint32_t ImageHeight = 1024;
constexpr vk::Format ImageFormat = vk::Format::eR8Snorm;

struct DrawUserData
{
    hb_position_t ascender;
    msdfgen::Shape* shape;
    msdfgen::Contour* contour;
    msdfgen::Point2 position;
};

struct Bounds
{
    double l, b, r, t;
};

static inline msdfgen::Point2 hbPoint2(hb_position_t x, hb_position_t y)
{
    return msdfgen::Point2((1 / 64.) * x, (1 / 64.) * y);
}

static inline bool render(msdfgen::Shape& shape, hb_font_t* font, hb_font_extents_t* fontExtents, hb_draw_funcs_t* funcs, hb_codepoint_t gid)
{
    hb_glyph_extents_t extents = { 0 };
    if (!hb_font_get_glyph_extents(font, gid, &extents)) {
        // no extents, bail out
        return false;
    }

    DrawUserData user = { fontExtents->ascender, &shape, nullptr, msdfgen::Point2() };

    hb_font_draw_glyph(font, gid, funcs, &user);

    if (!shape.contours.empty() && shape.contours.back().edges.empty())
        shape.contours.pop_back();

    return !shape.contours.empty();
}

static void moveTo(hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    if (!(data->contour && data->contour->edges.empty())) {
        data->contour = &data->shape->addContour();
    }
    data->position = hbPoint2(to_x, to_y);
}

static void lineTo(hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    const auto endpoint = hbPoint2(to_x, to_y);
    if (endpoint != data->position) {
        data->contour->addEdge(new msdfgen::LinearSegment(data->position, endpoint));
        data->position = endpoint;
    }
}

static void quadraticTo(hb_position_t control_x, hb_position_t control_y,
                        hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    data->contour->addEdge(new msdfgen::QuadraticSegment(data->position, hbPoint2(control_x, control_y), hbPoint2(to_x, to_y)));
    data->position = hbPoint2(to_x, to_y);
}

static void cubicTo(hb_position_t control1_x, hb_position_t control1_y,
                    hb_position_t control2_x, hb_position_t control2_y,
                    hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    data->contour->addEdge(new msdfgen::CubicSegment(data->position, hbPoint2(control1_x, control1_y), hbPoint2(control2_x, control2_y), hbPoint2(to_x, to_y)));
    data->position = hbPoint2(to_x, to_y);
}

RenderText::RenderText(const Render& render)
    : mRender(render)
{
    const auto& device = mRender.window().device();
    vk::ImageCreateInfo imageCreateInfo({}, vk::ImageType::e2D, ImageFormat, { ImageWidth, ImageHeight, 1 }, 1, 1);
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    vk::UniqueImage textureImage = device->createImageUnique(imageCreateInfo);
    if (!textureImage) {
        printf("failed to create texture image\n");
        return;
    }
    const vk::MemoryRequirements memRequirements = device->getImageMemoryRequirements(*textureImage);
    vk::MemoryAllocateInfo memoryAllocInfo(memRequirements.size, mRender.findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    vk::UniqueDeviceMemory textureImageMemory = device->allocateMemoryUnique(memoryAllocInfo);
    if (!textureImageMemory) {
        printf("failed to allocate texture image memory\n");
        return;
    }
    device->bindImageMemory(*textureImage, *textureImageMemory, 0);
    // ### maybe add a transition directly from eUndefined to eShaderReadOnlyOptimal
    mRender.transitionImageLayout(textureImage, ImageFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    mRender.transitionImageLayout(textureImage, ImageFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    mImage = std::move(textureImage);
    mImageMemory = std::move(textureImageMemory);

    vk::DeviceSize imageSize = ImageWidth * ImageHeight;
    auto staging = mRender.createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    mBuffer = std::move(staging.buffer);
    mBufferMemory = std::move(staging.memory);
}

vk::UniqueBuffer RenderText::renderText(const Text& text, const Rect& rect)
{
    Font fontf("./font.ttf", text.size);
    const auto hbfont = fontf.font();

    Layout layout(fontf);
    layout.setWidth(rect.width);
    layout.setText(utf8_to_utf16(text.contents));

    hb_font_extents_t fontExtents;
    hb_font_get_extents_for_direction(hbfont, HB_DIRECTION_LTR, &fontExtents);

    hb_draw_funcs_t* drawFuncs = hb_draw_funcs_create();
    hb_draw_funcs_set_move_to_func(drawFuncs, moveTo);
    hb_draw_funcs_set_line_to_func(drawFuncs, lineTo);
    hb_draw_funcs_set_quadratic_to_func(drawFuncs, quadraticTo);
    hb_draw_funcs_set_cubic_to_func(drawFuncs, cubicTo);

    const float range = 4.;
    const float range2 = std::max<float>(range, 2.) / 2.;
    const msdfgen::Vector2 scale = 1.;
    const msdfgen::Vector2 translate;
    const bool overlapSupport = true;
    const msdfgen::FillRule fillRule = msdfgen::FILL_NONZERO;
    const int width = 64;
    const int height = 64;

    const double large = 1e240;
    Bounds bounds;

    std::vector<RenderTextVertex> vertices;

    for (const auto& line : layout.lines) {
        for (const auto& run : line.runs) {
            const auto& buffer = run.buffer;
            assert(buffer != nullptr);

            const unsigned int len = hb_buffer_get_length(buffer);
            hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, nullptr);
            hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, nullptr);

            for (unsigned int i = 0; i < len; ++i) {
                //printf("got glyph idx %u\n", info[i].codepoint);
                msdfgen::Shape shape;
                if (!render(shape, hbfont, &fontExtents, drawFuncs, info[i].codepoint)) {
                    // nothing to render
                    continue;
                }
                if (!shape.validate()) {
                    printf("invalid shape\n");
                    continue;
                }
                shape.normalize();
                msdfgen::Bitmap<float, 1> sdf = msdfgen::Bitmap<float, 1>(width, height);
                msdfgen::generateSDF(sdf, shape, range, scale, translate, overlapSupport);
                msdfgen::distanceSignCorrection(sdf, shape, scale, translate, fillRule);

                bounds = { large, large, -large, -large };
                shape.bounds(bounds.l, bounds.b, bounds.r, bounds.t);

                const msdfgen::BitmapConstRef<float, 1>& ref = sdf;

                const int xmin = std::max<int>(0, floor(bounds.l) - range2);
                const int xmax = std::min<int>(ref.width, ceil(bounds.r) + range2);
                const int ymin = std::max<int>(0, floor(bounds.b) - range2);
                const int ymax = std::min<int>(ref.height, ceil(bounds.t) + range2);

                auto pixel = ref.pixels;
                pixel += ymin * ref.width;
                for (int row = ymin; row < ymax; ++row) {
                    pixel += xmin;
                    for (int col = xmin; col < xmax; ++col) {
                        float v = msdfgen::clamp(int((*pixel++)*0x100), 0xff) / 255.f;

                    }
                    pixel += ref.width - xmax;
                }
            }
        }
    }
}
