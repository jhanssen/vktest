#include "RenderText.h"
#include "Render.h"
#include <text/Layout.h>
#include <Utils.h>
#include <msdfgen.h>
#include <glm/glm.hpp>

constexpr uint32_t RenderSize = 24;
constexpr uint32_t ImageWidth = 1024;
constexpr uint32_t ImageHeight = 1024;
constexpr vk::Format ImageFormat = vk::Format::eR8Unorm;

struct DrawUserData
{
    hb_position_t ascender;
    msdfgen::Shape* shape;
    msdfgen::Contour* contour;
    msdfgen::Point2 position;
    uint32_t descent;
};

struct Bounds
{
    double l, b, r, t;
};

static inline float mix(float coord, float limit, float min, float max)
{
    float r = coord / limit;
    return glm::mix(min, max, r);
}

static inline float mixScreen(float coord, float limit)
{
    return mix(coord, limit, -1.f, 1.f);
}

static inline float mixTexture(float coord, float limit)
{
    return mix(coord, limit, 0.f, 1.f);
}

static inline msdfgen::Point2 hbPoint2(hb_position_t x, hb_position_t y)
{
    return msdfgen::Point2((1 / 64.) * x, (1 / 64.) * y);
}

static inline bool render(msdfgen::Shape& shape, hb_font_t* font, hb_font_extents_t* fontExtents, hb_draw_funcs_t* funcs, hb_codepoint_t gid, uint32_t descent)
{
    DrawUserData user = { fontExtents->ascender, &shape, nullptr, msdfgen::Point2(), descent };

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
    data->position = hbPoint2(to_x, to_y + data->descent);
}

static void lineTo(hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    const auto endpoint = hbPoint2(to_x, to_y + data->descent);
    if (endpoint != data->position) {
        data->contour->addEdge(new msdfgen::LinearSegment(data->position, endpoint));
        data->position = endpoint;
    }
}

static void quadraticTo(hb_position_t control_x, hb_position_t control_y,
                        hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    data->contour->addEdge(new msdfgen::QuadraticSegment(data->position, hbPoint2(control_x, control_y + data->descent), hbPoint2(to_x, to_y + data->descent)));
    data->position = hbPoint2(to_x, to_y + data->descent);
}

static void cubicTo(hb_position_t control1_x, hb_position_t control1_y,
                    hb_position_t control2_x, hb_position_t control2_y,
                    hb_position_t to_x, hb_position_t to_y, void* user_data)
{
    DrawUserData* data = static_cast<DrawUserData*>(user_data);
    data->contour->addEdge(new msdfgen::CubicSegment(data->position, hbPoint2(control1_x, control1_y + data->descent), hbPoint2(control2_x, control2_y + data->descent), hbPoint2(to_x, to_y + data->descent)));
    data->position = hbPoint2(to_x, to_y + data->descent);
}

RenderText::RenderText(const Render& render)
    : mRender(render)
{
    mRectPacker.init(ImageWidth, ImageHeight);

    const auto& device = mRender.window().device();
    vk::ImageCreateInfo imageCreateInfo({}, vk::ImageType::e2D, ImageFormat, { ImageWidth, ImageHeight, 1 }, 1, 1);
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    vk::UniqueImage textureImage = device->createImageUnique(imageCreateInfo);
    if (!textureImage) {
        printf("failed to create text image\n");
        return;
    }
    const vk::MemoryRequirements memRequirements = device->getImageMemoryRequirements(*textureImage);
    vk::MemoryAllocateInfo memoryAllocInfo(memRequirements.size, mRender.findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    vk::UniqueDeviceMemory textureImageMemory = device->allocateMemoryUnique(memoryAllocInfo);
    if (!textureImageMemory) {
        printf("failed to allocate text image memory\n");
        return;
    }
    device->bindImageMemory(*textureImage, *textureImageMemory, 0);

    vk::DeviceSize imageSize = ImageWidth * ImageHeight;
    auto staging = mRender.createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // don't know if I need to initialize the buffer memory?
    void* data = device->mapMemory(*staging.memory, 0, imageSize, {});
    memset(data, 0x00, imageSize);
    device->unmapMemory(*staging.memory);

    // ### maybe add a transition directly from eUndefined to eShaderReadOnlyOptimal
    mRender.transitionImageLayout(textureImage, ImageFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    mRender.copyBufferToImage(staging.buffer, textureImage, ImageWidth, ImageHeight);
    mRender.transitionImageLayout(textureImage, ImageFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    mImage = std::move(textureImage);
    mImageMemory = std::move(textureImageMemory);

    mImageBuffer = std::move(staging.buffer);
    mImageBufferMemory = std::move(staging.memory);
}

uint32_t RenderText::renderSize() const
{
    return RenderSize;
}

vk::UniqueImageView RenderText::imageView()
{
    const auto& device = mRender.window().device();
    vk::ImageViewCreateInfo imageViewCreateInfo({}, *mImage, vk::ImageViewType::e2D, ImageFormat, {},
                                                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    return device->createImageViewUnique(imageViewCreateInfo);
}

RenderText::RenderTextResult RenderText::renderText(const Text& text, const Rect& rect, uint32_t& vertexCount)
{
    const std::string fontPath = "./font.ttf";

    FontContentsKey contentsKey { fontPath, text.size, text.contents, rect.x, rect.y };
    auto contentsCacheHit = mContentsCache.find(contentsKey);
    if (contentsCacheHit != mContentsCache.end()) {
        // hit!
        const auto& contents = contentsCacheHit->second;
        vertexCount = contents.numVertices;
        return { *contents.renderedBuffer, contents.geometry };
    }

    Font measureFont(fontPath, text.size);
    const auto measureHbFont = measureFont.font();

    Font renderFont(fontPath, RenderSize);
    const auto renderHbFont = renderFont.font();

    const float factor = RenderSize / static_cast<float>(text.size);

    Layout layout(measureFont);
    layout.setWidth(rect.width);
    layout.setText(utf8_to_utf16(text.contents));

    hb_font_extents_t measureFontExtents;
    hb_font_get_extents_for_direction(measureHbFont, HB_DIRECTION_LTR, &measureFontExtents);

    hb_font_extents_t renderFontExtents;
    hb_font_get_extents_for_direction(renderHbFont, HB_DIRECTION_LTR, &renderFontExtents);

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
    vertices.reserve(layout.glyphCount() * 6);

    const auto screenWidth = mRender.window().width();
    const auto screenHeight = mRender.window().height();
    auto makeVertex = [screenWidth, screenHeight](float srcX, float srcY, float dstX, float dstY) -> RenderTextVertex {
        // printf("making dst vert %f %f -> %f %f\n", dstX, dstY, mixScreen(dstX, screenWidth), mixScreen(dstY, screenHeight));
        return RenderTextVertex {
            { mixScreen(dstX, screenWidth), mixScreen(dstY, screenHeight) },
            { mixTexture(srcX, ImageWidth), mixTexture(srcY, ImageHeight) }
        };
    };

    const float measureAscender = measureFontExtents.ascender / 64.f;
    const float measureDescender = measureFontExtents.descender / 64.f;

    const float renderAscender = renderFontExtents.ascender / 64.f;

    const float lineHeight = measureAscender - measureDescender;
    //float dstX = rect.x, dstY = rect.y;
    float dstX = 0.f, dstY = 0.f;

    // printf("asc %f desc %f\n", ascender, descender);

    const auto& device = mRender.window().device();
    const vk::DeviceSize imageSize = ImageWidth * ImageHeight;

    Rect renderedGeometry;

    for (const auto& line : layout.lines()) {
        dstX = 0.f;//rect.x;
        for (const auto& run : line.runs) {
            const auto& buffer = run.buffer;
            assert(buffer != nullptr);

            const unsigned int len = hb_buffer_get_length(buffer);
            hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, nullptr);
            hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, nullptr);

            for (unsigned int i = 0; i < len; ++i) {
                //printf("got glyph idx %u\n", info[i].codepoint);
                FontGidKey cacheKey { renderFont.path(), renderFont.size(), info[i].codepoint };
                auto cacheHit = mGidCache.find(cacheKey);
                if (cacheHit != mGidCache.end()) {
                    const RectPacker::Rect& prect = cacheHit->second.rect->rect;
                    // top-left, bottom-right, bottom-left, top-left, top-right, bottom-right

                    const auto& ext = cacheHit->second.extents;
                    const float dstTop = dstY + (renderAscender - ext.ybearing);
                    const float dstBottom = dstTop + prect.height();

                    vertices.push_back(makeVertex(prect.x, prect.y, dstX, dstBottom));
                    vertices.push_back(makeVertex(prect.x, prect.bottom, dstX, dstTop));
                    vertices.push_back(makeVertex(prect.right, prect.bottom, dstX + prect.width(), dstTop));
                    vertices.push_back(makeVertex(prect.x, prect.y, dstX, dstBottom));
                    vertices.push_back(makeVertex(prect.right, prect.bottom, dstX + prect.width(), dstTop));
                    vertices.push_back(makeVertex(prect.right, prect.y, dstX + prect.width(), dstBottom));

                    // printf("extents %f %f %f %f\n", ext.xbearing, ext.ybearing, ext.width, ext.height);

                    dstX += (pos[i].x_advance / 64.f) * factor;
                    continue;
                }

                hb_glyph_extents_t extents;
                if (!hb_font_get_glyph_extents(renderHbFont, info[i].codepoint, &extents)) {
                    // no extents
                    dstX += (pos[i].x_advance / 64.f) * factor;
                    continue;
                }

                msdfgen::Shape shape;
                const uint32_t descent = abs(extents.y_bearing + extents.height);
                if (!render(shape, renderHbFont, &measureFontExtents, drawFuncs, info[i].codepoint, descent)) {
                    // nothing to render
                    dstX += (pos[i].x_advance / 64.f) * factor;
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

                auto node = mRectPacker.insert(xmax - xmin, ymax - ymin);
                const RectPacker::Rect& prect = node->rect;

                uint8_t* data = static_cast<uint8_t*>(device->mapMemory(*mImageBufferMemory, 0, imageSize, {}));
                data += prect.y * ImageWidth;

                auto pixel = ref.pixels;
                pixel += ymin * ref.width;
                for (int row = ymin; row < ymax; ++row) {
                    pixel += xmin;
                    data += prect.x;
                    for (int col = xmin; col < xmax; ++col) {
                        *data++ = msdfgen::clamp(int((*pixel++)*0x100), 0xff);
                    }
                    pixel += ref.width - xmax;
                    data += ImageWidth - (prect.x + (xmax - xmin));;
                }

                device->unmapMemory(*mImageBufferMemory);

                mGidCache[cacheKey] = { node, { extents.x_bearing / 64.f, extents.y_bearing / 64.f, extents.width / 64.f, extents.height / 64.f } };

                const float dstTop = dstY + (renderAscender - (extents.y_bearing / 64.f));
                const float dstBottom = dstTop + prect.height();

                // printf("extents2 %f %f %f %f\n", extents.x_bearing / 64.f, extents.y_bearing / 64.f, extents.width / 64.f, extents.height / 64.f);

                vertices.push_back(makeVertex(prect.x, prect.y, dstX, dstBottom));
                vertices.push_back(makeVertex(prect.x, prect.bottom, dstX, dstTop));
                vertices.push_back(makeVertex(prect.right, prect.bottom, dstX + prect.width(), dstTop));
                vertices.push_back(makeVertex(prect.x, prect.y, dstX, dstBottom));
                vertices.push_back(makeVertex(prect.right, prect.bottom, dstX + prect.width(), dstTop));
                vertices.push_back(makeVertex(prect.right, prect.y, dstX + prect.width(), dstBottom));

                dstX += (pos[i].x_advance / 64.f) * factor;
            }
        }
        dstY += lineHeight * factor;
        if (dstX > renderedGeometry.width)
            renderedGeometry.width = dstX;
    }
    renderedGeometry.height = dstY;

    mRender.transitionImageLayout(mImage, ImageFormat, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal);
    mRender.copyBufferToImage(mImageBuffer, mImage, ImageWidth, ImageHeight);
    mRender.transitionImageLayout(mImage, ImageFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    const vk::DeviceSize bufferSize = sizeof(RenderTextVertex) * vertices.size();
    auto buffer = mRender.createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    FontContentsData contentsData;
    contentsData.renderedBuffer = std::move(buffer.buffer);
    contentsData.renderedBufferMemory = std::move(buffer.memory);
    contentsData.numVertices = vertices.size();
    contentsData.geometry = renderedGeometry;

    void* out = device->mapMemory(*contentsData.renderedBufferMemory, 0, bufferSize, {});
    memcpy(out, vertices.data(), bufferSize);
    device->unmapMemory(*contentsData.renderedBufferMemory);

    auto renderedBuffer = *contentsData.renderedBuffer;

    mContentsCache[contentsKey] = std::move(contentsData);

    // printf("vertices %zu?\n", vertices.size());
    // for (const auto& v : vertices) {
    //     printf("%f %f -> %f %f\n", v.src.x, v.src.y, v.dst.x, v.dst.y);
    // }

    vertexCount = vertices.size();
    return { renderedBuffer, renderedGeometry };
}

// lifted from https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
inline void hash_combine(std::size_t& seed) { }

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    hash_combine(seed, rest...);
}

size_t RenderText::FontGidHasher::operator()(const FontGidKey& key) const noexcept
{
    size_t h = 0;
    hash_combine(h, key.path, key.size, key.gid);
    return h;
}

bool RenderText::FontGidKey::operator==(const FontGidKey& other) const
{
    return path == other.path && size == other.size && gid == other.gid;
}

size_t RenderText::FontContentsHasher::operator()(const FontContentsKey& key) const noexcept
{
    size_t h = 0;
    hash_combine(h, key.path, key.size, key.contents, key.x, key.y);
    return h;
}

bool RenderText::FontContentsKey::operator==(const FontContentsKey& other) const
{
    return path == other.path && size == other.size && contents == other.contents && x == other.x && y == other.y;
}
