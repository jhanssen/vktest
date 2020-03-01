#ifndef RENDERTEXT_H
#define RENDERTEXT_H

#include <vulkan/vulkan.hpp>
#include <finders_interface.h> // our rectpacker, bad name for a header file
#include <vector>
#include <scene/Text.h>
#include <Rect.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>

class Render;

class RenderText
{
public:
    RenderText(const Render& render);

    const vk::UniqueImageView& imageView() const { return mImageView; }
    vk::UniqueBuffer renderText(const Text& text, const Rect& bounds);

private:
    const Render& mRender;

    typedef rectpack2D::empty_spaces<false, rectpack2D::default_empty_spaces> PackerSpaces;
    typedef rectpack2D::output_rect_t<PackerSpaces> PackerRect;

    std::vector<PackerRect> mRects;

    vk::UniqueImage mImage;
    vk::UniqueDeviceMemory mImageMemory;
    vk::UniqueBuffer mBuffer;
    vk::UniqueDeviceMemory mBufferMemory;
    vk::UniqueImageView mImageView;
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
