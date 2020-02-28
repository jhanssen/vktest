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
            std::vector<std::shared_ptr<vk::UniqueCommandBuffer> > commandBuffers;
            //vk::UniqueBuffer vertexBuffer;
            std::vector<std::shared_ptr<vk::UniqueDeviceMemory> > ubosMemory;
            std::vector<std::shared_ptr<vk::UniqueBuffer > > ubos;
            std::vector<std::shared_ptr<vk::UniqueDescriptorSet > > descriptorSets;

            virtual void update(const vk::UniqueDevice& device, uint32_t currentImage) = 0;
        };

        std::vector<std::shared_ptr<Drawable> > drawables;
        std::vector<std::shared_ptr<Node> > children;
    };

    struct RenderColor;

    void traverseSceneItem(const std::shared_ptr<Scene::Item>& sceneItem,
                           std::shared_ptr<Node>& renderNode);
    void makeRenderTree(const Scene& scene);

    void makeColorDrawableData();
    void makeDrawableDatas();

    std::shared_ptr<Node::Drawable> makeColorDrawable(const Color& color, const Rect& geometry);

    void renderNode(const std::shared_ptr<Node>& node, const vk::UniqueCommandBuffer& commandBuffer, uint32_t imageIndex);

private:
    const Window& mWindow;
    vk::UniqueCommandPool mCommandPool;
    vk::UniqueDescriptorPool mDescriptorPool;

    struct DrawableData
    {
        std::shared_ptr<PipelineResult> pipeline;
        std::vector<std::shared_ptr<vk::UniqueCommandBuffer> > commandBuffers;

        std::vector<std::shared_ptr<vk::UniqueDeviceMemory> > ubosMemory;
        std::vector<std::shared_ptr<vk::UniqueBuffer > > ubos;
        std::vector<std::shared_ptr<vk::UniqueDescriptorSet > > descriptorSets;
    };
    enum DrawableType { DrawableColor };
    std::vector<DrawableData> mDrawableData;

    std::shared_ptr<Node> mRoot;
};

#endif
