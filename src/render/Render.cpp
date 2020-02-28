#include "Render.h"
#include <Buffer.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

struct RenderColorData
{
    glm::vec4 color;
    glm::vec4 geometry;
};

struct Render::RenderColor : public Render::Node::Drawable
{
public:
    virtual void update(const vk::UniqueDevice& device, uint32_t currentImage);

    bool changed { true };
    RenderColorData data;
};

void Render::RenderColor::update(const vk::UniqueDevice& device, uint32_t currentImage)
{
    if (!changed)
        return;
    void* out = device->mapMemory(**ubosMemory[currentImage], 0, sizeof(data), {});
    memcpy(out, &data, sizeof(data));
    device->unmapMemory(**ubosMemory[currentImage]);
    changed = false;
}

static vk::UniqueShaderModule createShaderModule(const vk::UniqueDevice& device, const Buffer& buffer)
{
    if (buffer.empty()) {
        printf("unable to create shader module, file empty\n");
        return vk::UniqueShaderModule();
    }

    vk::ShaderModuleCreateInfo createInfo({}, buffer.size(), reinterpret_cast<const uint32_t*>(buffer.data()));
    return device->createShaderModuleUnique(createInfo);
}

static uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return std::numeric_limits<uint32_t>::max();
}

static Render::VertexBuffer createBuffer(vk::PhysicalDevice physicalDevice, vk::Device device, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);

    Render::VertexBuffer buf;
    buf.buffer = device.createBufferUnique(bufferInfo);
    if (!buf.buffer) {
        return Render::VertexBuffer();
    }

    vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*buf.buffer);
    vk::MemoryAllocateInfo allocInfo(memRequirements.size, findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties));

    buf.memory = device.allocateMemoryUnique(allocInfo);
    if (!buf.memory) {
        return Render::VertexBuffer();
    }

    device.bindBufferMemory(*buf.buffer, *buf.memory, 0);
    return buf;
}

static void copyBuffer(vk::Device device, vk::CommandPool commandPool, vk::Queue graphicsQueue,
                       vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);

    const std::vector<vk::UniqueCommandBuffer> commandBuffers = device.allocateCommandBuffersUnique(allocInfo);
    if (commandBuffers.empty() || !commandBuffers[0]) {
        printf("failed to allocate command buffer for copy\n");
        return;
    }
    const auto& commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer->begin(beginInfo);
    vk::BufferCopy copyRegion(0, 0, size);
    commandBuffer->copyBuffer(srcBuffer, dstBuffer, { copyRegion });
    commandBuffer->end();

    vk::SubmitInfo submitInfo({}, {}, {}, 1, &*commandBuffer);

    graphicsQueue.submit({ submitInfo }, {});
    graphicsQueue.waitIdle();
}

Render::Render(const Scene& scene, const Window& window)
    : mWindow(window)
{
    const auto& device = window.device();
    const uint32_t graphicsFamily = window.graphicsFamily();

    vk::CommandPoolCreateInfo poolInfo({}, graphicsFamily);
    mCommandPool = device->createCommandPoolUnique(poolInfo);
    if (!mCommandPool) {
        printf("failed to create command pool!\n");
        return;
    }

    const auto& swapChainFramebuffers = window.swapChainFramebuffers();

    vk::DescriptorPoolSize descriptorPoolSize(vk::DescriptorType::eUniformBuffer, swapChainFramebuffers.size());
    vk::DescriptorPoolCreateInfo descriptorPoolInfo({}, swapChainFramebuffers.size(), 1, &descriptorPoolSize);
    mDescriptorPool = device->createDescriptorPoolUnique(descriptorPoolInfo);
    if (!mDescriptorPool) {
        printf("failed to create descriptor pool\n");
        return;
    }

    makeDrawableDatas();
    makeRenderTree(scene);
}

std::shared_ptr<Render::PipelineResult> Render::makePipeline(const PipelineData& data)
{
    const auto& device = mWindow.device();
    const auto& extent = mWindow.extent();
    auto vertShaderModule = createShaderModule(device, data.vertexShader);
    auto fragShaderModule = createShaderModule(device, data.fragmentShader);
    if (!vertShaderModule || !fragShaderModule) {
        printf("couldn't create shader modules\n");
        return std::shared_ptr<PipelineResult>();
    }

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::VertexInputBindingDescription bindingDescription;
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    if (data.vertexBinding && data.vertexAttributes) {
        bindingDescription = data.vertexBinding();
        attributeDescriptions = data.vertexAttributes();

        vertexInputInfo = vk::PipelineVertexInputStateCreateInfo({}, 1, &bindingDescription, static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data());
    }

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f);

    vk::Rect2D scissor = { { 0, 0 }, extent };

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
                                                        vk::FrontFace::eClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha,
                                                               vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
                                                               vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    vk::DynamicState dynamicStates[] = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eLineWidth
    };

    vk::PipelineDynamicStateCreateInfo dynamicState({}, 2, dynamicStates);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    vk::UniqueDescriptorSetLayout descriptorSetLayout;
    if (data.descriptorSetLayout) {
        descriptorSetLayout = data.descriptorSetLayout(device);
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;
    }

    auto pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);
    if (!pipelineLayout) {
        printf("failed to create pipeline layout!\n");
        return std::shared_ptr<PipelineResult>();
    }

    const auto& renderPass = mWindow.renderPass();

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr, &viewportState,
                                                &rasterizer, &multisampling, nullptr, &colorBlending, nullptr, *pipelineLayout,
                                                *renderPass, 0, {}, -1);

    auto graphicsPipelines = device->createGraphicsPipelinesUnique({}, { pipelineInfo });
    if (graphicsPipelines.empty() || !graphicsPipelines[0]) {
        printf("failed to create graphics pipeline!\n");
        return std::shared_ptr<PipelineResult>();
    }

    return std::make_shared<PipelineResult>(std::move(graphicsPipelines[0]), std::move(pipelineLayout), std::move(descriptorSetLayout));
}

void Render::makeColorDrawableData()
{
    // make pipeline
    const PipelineData createData = {
        Buffer::readFile("./color-vert.spv"),
        Buffer::readFile("./color-frag.spv"),
        {}, {},
        [](const vk::UniqueDevice& device) -> vk::UniqueDescriptorSetLayout {
            vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
            vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
            return device->createDescriptorSetLayoutUnique(layoutInfo);
        }
    };

    DrawableData drawableData;

    auto pipeline = makePipeline(createData);
    drawableData.pipeline = pipeline;

    const auto& swapChainFramebuffers = mWindow.swapChainFramebuffers();
    const auto& device = mWindow.device();
    const auto& physicalDevice = mWindow.physicalDevice();

    // make ubos
    vk::DeviceSize colorSize = sizeof(RenderColorData);
    drawableData.ubos.reserve(swapChainFramebuffers.size());
    drawableData.ubosMemory.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        auto ubo = createBuffer(physicalDevice, *device, colorSize,
                                vk::BufferUsageFlagBits::eUniformBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        drawableData.ubos.push_back(std::make_shared<vk::UniqueBuffer>(std::move(ubo.buffer)));
        drawableData.ubosMemory.push_back(std::make_shared<vk::UniqueDeviceMemory>(std::move(ubo.memory)));
    }

    // allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(swapChainFramebuffers.size(), *pipeline->descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo(*mDescriptorPool, swapChainFramebuffers.size(), layouts.data());
    auto sets = device->allocateDescriptorSetsUnique(allocInfo);
    drawableData.descriptorSets.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        vk::DescriptorBufferInfo bufferInfo(**drawableData.ubos[i], 0, sizeof(RenderColorData));
        vk::WriteDescriptorSet descriptorWrite(*sets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &bufferInfo);
        device->updateDescriptorSets({ descriptorWrite }, {});

        drawableData.descriptorSets.push_back(std::make_shared<vk::UniqueDescriptorSet>(std::move(sets[i])));
    }

    // make command buffers
    const auto& extent = mWindow.extent();
    const auto& renderPass = mWindow.renderPass();

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::ePrimary, swapChainFramebuffers.size());
    auto commandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (commandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        return;
    }

    drawableData.commandBuffers.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo;
        commandBuffers[i]->begin(beginInfo);

        vk::ClearValue clearValue = vk::ClearColorValue(std::array<float,4> { 0.0f, 0.0f, 0.0f, 1.0f });
        vk::RenderPassBeginInfo renderPassInfo(*renderPass, swapChainFramebuffers[i], { { 0, 0 }, extent }, 1, &clearValue);

        commandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
        commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->layout, 0, { **drawableData.descriptorSets[i] }, {});
        commandBuffers[i]->draw(3, 1, 0, 0);
        commandBuffers[i]->endRenderPass();

        commandBuffers[i]->end();

        drawableData.commandBuffers.push_back(std::make_shared<vk::UniqueCommandBuffer>(std::move(commandBuffers[i])));
    }

    assert(mDrawableData.size() == DrawableColor);
    mDrawableData.push_back(std::move(drawableData));
}

void Render::makeDrawableDatas()
{
    makeColorDrawableData();
}

std::shared_ptr<Render::Node::Drawable> Render::makeColorDrawable(const Color& color, const Rect& geom)
{
    assert(mDrawableData.size() > DrawableColor);
    const auto& drawableData = mDrawableData[DrawableColor];

    auto colorDrawable = std::make_shared<RenderColor>();
    colorDrawable->pipeline = drawableData.pipeline;
    colorDrawable->commandBuffers = drawableData.commandBuffers;
    colorDrawable->ubosMemory = drawableData.ubosMemory;
    colorDrawable->ubos = drawableData.ubos;
    colorDrawable->descriptorSets = drawableData.descriptorSets;

    colorDrawable->data.color = { color.r, color.g, color.b, color.a };
    colorDrawable->data.geometry = { geom.x, geom.y, geom.x + geom.width, geom.y + geom.height };

    return colorDrawable;
}

void Render::traverseSceneItem(const std::shared_ptr<Scene::Item>& sceneItem,
                               std::shared_ptr<Node>& renderNode)
{
    if (!sceneItem)
        return;
    assert(!renderNode);
    renderNode = std::make_shared<Node>();
    if (sceneItem->geometry.isValid()) {
        if (sceneItem->color.isValid()) {
            renderNode->drawables.push_back(makeColorDrawable(sceneItem->color, sceneItem->geometry));
        }
    }

    if (!sceneItem->children.empty()) {
        renderNode->children.resize(sceneItem->children.size());
        for (size_t i = 0; i < sceneItem->children.size(); ++i) {
            traverseSceneItem(sceneItem->children[i], renderNode->children[i]);
        }
    }
}

void Render::makeRenderTree(const Scene& scene)
{
    traverseSceneItem(scene.root, mRoot);
}

void Render::renderNode(const std::shared_ptr<Node>& node, const vk::UniqueCommandBuffer& commandBuffer, uint32_t imageIndex)
{
    for (const auto& drawable : node->drawables) {
        drawable->update(mWindow.device(), imageIndex);
        commandBuffer->executeCommands({ **drawable->commandBuffers[imageIndex] });
    }

    for (const auto& child : node->children) {
        renderNode(child, commandBuffer, imageIndex);
    }
}

void Render::render(const Window::RenderData& data)
{
    const auto& device = mWindow.device();

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::ePrimary, 1);
    auto commandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (commandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        return;
    }

    const auto& commandBuffer = commandBuffers[0];
    vk::CommandBufferBeginInfo beginInfo;
    commandBuffer->begin(beginInfo);
    renderNode(mRoot, commandBuffer, data.imageIndex);
    commandBuffer->end();

    vk::Semaphore waitSemaphores[] = { data.wait };
    vk::Semaphore signalSemaphores[] = { data.signal };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    const auto& graphicsQueue = mWindow.graphicsQueue();
    vk::SubmitInfo submitInfo(1, waitSemaphores, waitStages, 1, &*commandBuffer, 1, signalSemaphores);

    try {
        graphicsQueue.submit({ submitInfo }, data.fence);
    } catch (const vk::Error& error) {
        printf("failed to submit draw command buffer\n");
    }
}
