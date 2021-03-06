#ifndef RENDER_H
#define RENDER_H

#include "RenderText.h"
#include <scene/Scene.h>
#include <Window.h>
#include <Buffer.h>
#include <Rect.h>
#include <memory>
#include <vector>
#include <functional>

class Render
{
public:
    Render(const Scene& scene, const Window& window);

    void render(const Window::RenderData& data);

    const Window& window() const { return mWindow; }

    struct VertexBuffer
    {
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
    };

    void transitionImageLayout(const vk::UniqueImage& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;
    void copyBufferToImage(const vk::UniqueBuffer& buffer, const vk::UniqueImage& image, uint32_t width, uint32_t height) const;
    void copyBufferToImage(vk::DeviceSize bufferOffset, uint32_t bufferRowLength, uint32_t bufferImageHeight, const vk::UniqueBuffer& buffer,
                           const vk::UniqueImage& image, int32_t x, int32_t y, uint32_t width, uint32_t height) const;
    VertexBuffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const;
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

private:
    struct PipelineData
    {
        Buffer vertexShader, fragmentShader;
        std::function<vk::VertexInputBindingDescription()> vertexBinding;
        std::function<std::vector<vk::VertexInputAttributeDescription>()> vertexAttributes;
        std::function<vk::UniqueDescriptorSetLayout(const vk::UniqueDevice&)> descriptorSetLayout;
    };

    struct PipelineResult
    {
        PipelineResult(vk::UniquePipeline&& p,
                       vk::UniquePipelineLayout&& l,
                       vk::UniqueDescriptorSetLayout&& d)
            : pipeline(std::move(p)), layout(std::move(l)), descriptorSetLayout(std::move(d))
        {
        }

        vk::UniquePipeline pipeline;
        vk::UniquePipelineLayout layout;
        vk::UniqueDescriptorSetLayout descriptorSetLayout;
    };
    std::shared_ptr<PipelineResult> makePipeline(const PipelineData& data, vk::PrimitiveTopology topology);

    struct Node
    {
        struct Drawable
        {
            virtual ~Drawable() { }

            // shared data
            std::shared_ptr<PipelineResult> pipeline;
            std::vector<vk::UniqueCommandBuffer> commandBuffers;
            vk::UniqueBuffer vertexBuffer;
            vk::UniqueDeviceMemory imageMemory;
            vk::UniqueImage image;
            vk::UniqueImageView imageView;
            vk::UniqueSampler imageSampler;
            std::vector<vk::UniqueDeviceMemory> ubosMemory;
            std::vector<vk::UniqueBuffer> ubos;
            std::vector<vk::UniqueDescriptorSet> descriptorSets;

            virtual void update(const vk::UniqueDevice& device, uint32_t currentImage) = 0;
        };

        std::vector<std::shared_ptr<Drawable> > drawables;
        std::vector<std::shared_ptr<Node> > children;
    };

    struct RenderColorDrawable;
    struct RenderImageDrawable;
    struct RenderTextDrawable;

    void traverseSceneItem(const std::shared_ptr<Scene::Item>& sceneItem,
                           std::shared_ptr<Node>& renderNode);
    void makeRenderTree(const Scene& scene);

    void makeColorDrawableData();
    void makeImageDrawableData();
    void makeTextDrawableData();
    void makeDrawableDatas();

    std::shared_ptr<Node::Drawable> makeColorDrawable(const Color& color, const Rect& geometry);
    std::shared_ptr<Node::Drawable> makeImageDrawable(const Scene::ImageData& image, const Rect& geometry);
    std::shared_ptr<Node::Drawable> makeTextDrawable(const Text& image, const Rect& geometry);

    void renderNode(const std::shared_ptr<Node>& node, const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);

    vk::CommandBuffer beginSingleCommand() const;
    void endSingleCommand(const vk::CommandBuffer& commandBuffer) const;

private:
    const Window& mWindow;
    vk::UniqueCommandPool mCommandPool;
    vk::UniqueDescriptorPool mDescriptorPool;

    struct DrawableData
    {
        std::shared_ptr<PipelineResult> pipeline;
    };
    enum DrawableType { DrawableColor, DrawableImage, DrawableText };
    std::vector<DrawableData> mDrawableData;

    std::shared_ptr<Node> mRoot;
    std::shared_ptr<RenderText> mRenderText;
};

#endif
