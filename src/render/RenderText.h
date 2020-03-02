#ifndef RENDERTEXT_H
#define RENDERTEXT_H

#include "RectPacker.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <scene/Text.h>
#include <text/Font.h>
#include <Rect.h>
#include <unordered_map>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>

class Render;

class RenderText
{
public:
    RenderText(const Render& render);

    vk::UniqueImageView imageView();
    vk::Buffer renderText(const Text& text, const Rect& bounds, uint32_t& vertexCount);

private:
    struct FontGidKey
    {
        std::string path;
        uint32_t size;
        uint32_t gid;

        bool operator==(const FontGidKey&) const;
    };
    struct FontGidData
    {
        RectPacker::Node* rect;
    };
    struct FontGidHasher
    {
        size_t operator()(const FontGidKey&) const noexcept;
    };

    const Render& mRender;

    RectPacker mRectPacker;

    vk::UniqueImage mImage;
    vk::UniqueDeviceMemory mImageMemory;
    vk::UniqueBuffer mImageBuffer;
    vk::UniqueDeviceMemory mImageBufferMemory;

    vk::UniqueBuffer mRenderedBuffer;
    vk::UniqueDeviceMemory mRenderedBufferMemory;

    std::unordered_map<FontGidKey, FontGidData, FontGidHasher> mGidCache;
};

struct RenderTextVertex
{
    glm::vec2 dst;
    glm::vec2 src;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(RenderTextVertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;

        return bindingDescription;
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        attributeDescriptions.resize(2);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[0].offset = offsetof(RenderTextVertex, dst);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[1].offset = offsetof(RenderTextVertex, src);

        return attributeDescriptions;
    }
};

#endif // RENDERTEXT_H
