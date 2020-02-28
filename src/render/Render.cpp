#include "Render.h"
#include <Buffer.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

static vk::UniqueShaderModule createShaderModule(const vk::UniqueDevice& device, const Buffer& buffer)
{
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

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        vk::VertexInputBindingDescription bindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex);
        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions = {
            vk::VertexInputAttributeDescription { 0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos) },
            vk::VertexInputAttributeDescription { 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) }
        };
        return attributeDescriptions;
    }
};

Render::Render(const Scene& scene, const Window& window)
    : mWindow(window)
{
    const auto& device = window.device();
    const auto& physicalDevice = window.physicalDevice();
    const auto& extent = window.extent();
    auto vertShaderModule = createShaderModule(device, Buffer::readFile("vert.spv"));
    auto fragShaderModule = createShaderModule(device, Buffer::readFile("frag.spv"));
    if (!vertShaderModule || !fragShaderModule) {
        printf("couldn't create shader modules\n");
        return;
    }

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 1, &bindingDescription, static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data());

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

    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);
    if (!pipelineLayout) {
        printf("failed to create pipeline layout!\n");
        return;
    }

    const auto& renderPass = window.renderPass();

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr, &viewportState,
                                                &rasterizer, &multisampling, nullptr, &colorBlending, nullptr, *pipelineLayout,
                                                *renderPass, 0, {}, -1);

    mGraphicsPipelines = device->createGraphicsPipelinesUnique({}, { pipelineInfo });
    if (mGraphicsPipelines.empty() || !mGraphicsPipelines[0]) {
        printf("failed to create graphics pipeline!\n");
    }

    const auto& graphicsPipeline = mGraphicsPipelines[0];

    const uint32_t graphicsFamily = window.graphicsFamily();
    const auto& graphicsQueue = window.graphicsQueue();

    vk::CommandPoolCreateInfo poolInfo({}, graphicsFamily);
    mCommandPool = device->createCommandPoolUnique(poolInfo);
    if (!mCommandPool) {
        printf("failed to create command pool!\n");
        return;
    }

    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    const auto stagingBuffer = createBuffer(physicalDevice, *device, bufferSize,
                                            vk::BufferUsageFlagBits::eTransferSrc,
                                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = device->mapMemory(*stagingBuffer.memory, 0, bufferSize);
    memcpy(data, vertices.data(), (size_t) bufferSize);
    device->unmapMemory(*stagingBuffer.memory);

    mVertexBuffer = createBuffer(physicalDevice, *device, bufferSize,
                                 vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                                 vk::MemoryPropertyFlagBits::eDeviceLocal);

    copyBuffer(*device, *mCommandPool, graphicsQueue, *stagingBuffer.buffer, *mVertexBuffer.buffer, bufferSize);

    const auto& swapChainFramebuffers = window.swapChainFramebuffers();

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::ePrimary, swapChainFramebuffers.size());
    mCommandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (mCommandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        return;
    }

    for (size_t i = 0; i < mCommandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo;
        mCommandBuffers[i]->begin(beginInfo);

        vk::ClearValue clearValue = vk::ClearColorValue(std::array<float,4> { 0.0f, 0.0f, 0.0f, 1.0f });
        vk::RenderPassBeginInfo renderPassInfo(*renderPass, swapChainFramebuffers[i], { { 0, 0 }, extent }, 1, &clearValue);

        mCommandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        mCommandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        mCommandBuffers[i]->bindVertexBuffers(0, { *mVertexBuffer.buffer }, { 0 });
        mCommandBuffers[i]->draw(3, 1, 0, 0);

        mCommandBuffers[i]->end();
    }
}

void Render::render(const Window::RenderData& data)
{
    vk::Semaphore waitSemaphores[] = { data.wait };
    vk::Semaphore signalSemaphores[] = { data.signal };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    const auto& commandBuffer = mCommandBuffers[data.currentFrame];
    const auto& graphicsQueue = mWindow.graphicsQueue();
    vk::SubmitInfo submitInfo(1, waitSemaphores, waitStages, 1, &*commandBuffer, 1, signalSemaphores);

    try {
        graphicsQueue.submit({ submitInfo }, data.fence);
    } catch (const vk::Error& error) {
        printf("failed to submit draw command buffer\n");
    }
}
