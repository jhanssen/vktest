#ifndef RENDER_H
#define RENDER_H

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

    struct VertexBuffer
    {
        vk::UniqueBuffer buffer;
        vk::UniqueDeviceMemory memory;
    };

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
    std::shared_ptr<PipelineResult> makePipeline(const PipelineData& data);

    struct Node
    {
        struct Drawable
        {
            virtual ~Drawable() { }

            // shared data
            std::shared_ptr<PipelineResult> pipeline;
            std::vector<vk::UniqueCommandBuffer> commandBuffers;
            //vk::UniqueBuffer vertexBuffer;
            std::vector<vk::UniqueDeviceMemory> imagesMemory;
            std::vector<vk::UniqueImage> images;
            std::vector<vk::UniqueImageView> imageViews;
            std::vector<vk::UniqueSampler> imageSamplers;
            std::vector<vk::UniqueDeviceMemory> ubosMemory;
            std::vector<vk::UniqueBuffer> ubos;
            std::vector<vk::UniqueDescriptorSet> descriptorSets;

            virtual void update(const vk::UniqueDevice& device, uint32_t currentImage) = 0;
        };

        std::vector<std::shared_ptr<Drawable> > drawables;
        std::vector<std::shared_ptr<Node> > children;
    };

    struct RenderColor;
    struct RenderImage;

    void traverseSceneItem(const std::shared_ptr<Scene::Item>& sceneItem,
                           std::shared_ptr<Node>& renderNode);
    void makeRenderTree(const Scene& scene);

    void makeColorDrawableData();
    void makeImageDrawableData();
    void makeDrawableDatas();

    std::shared_ptr<Node::Drawable> makeColorDrawable(const Color& color, const Rect& geometry);
    std::shared_ptr<Node::Drawable> makeImageDrawable(const Scene::ImageData& image, const Rect& geometry);

    void renderNode(const std::shared_ptr<Node>& node, const vk::CommandBuffer& commandBuffer, uint32_t imageIndex);

    vk::CommandBuffer beginSingleCommand();
    void endSingleCommand(const vk::CommandBuffer& commandBuffer, bool enqueue = true);

    void transitionImageLayout(const vk::UniqueImage& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(const vk::UniqueBuffer& buffer, const vk::UniqueImage& image, uint32_t width, uint32_t height);

private:
    const Window& mWindow;
    vk::UniqueCommandPool mCommandPool;
    vk::UniqueDescriptorPool mDescriptorPool;

    struct DrawableData
    {
        std::shared_ptr<PipelineResult> pipeline;
    };
    enum DrawableType { DrawableColor, DrawableImage };
    std::vector<DrawableData> mDrawableData;

    std::shared_ptr<Node> mRoot;
};

#endif
