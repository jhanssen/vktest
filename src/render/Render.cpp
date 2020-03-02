#include "Render.h"
#include "RenderText.h"
#include <Buffer.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct RenderColorData
{
    glm::vec4 color;
    glm::vec4 geometry;
};

struct RenderImageData
{
    glm::vec4 geometry;
};

struct RenderTextData
{
    glm::vec4 color;
    glm::mat4 projection;
};

struct Render::RenderColorDrawable : public Render::Node::Drawable
{
public:
    virtual void update(const vk::UniqueDevice& device, uint32_t currentImage);

    std::vector<bool> changed;
    RenderColorData data;
};

void Render::RenderColorDrawable::update(const vk::UniqueDevice& device, uint32_t currentImage)
{
    if (!changed[currentImage])
        return;
    void* out = device->mapMemory(*ubosMemory[currentImage], 0, sizeof(data), {});
    memcpy(out, &data, sizeof(data));
    device->unmapMemory(*ubosMemory[currentImage]);
    changed[currentImage] = false;
}

struct Render::RenderImageDrawable : public Render::Node::Drawable
{
public:
    virtual void update(const vk::UniqueDevice& device, uint32_t currentImage);

    std::vector<bool> changed;
    RenderImageData data;
};

void Render::RenderImageDrawable::update(const vk::UniqueDevice& device, uint32_t currentImage)
{
    if (!changed[currentImage])
        return;
    void* out = device->mapMemory(*ubosMemory[currentImage], 0, sizeof(data), {});
    memcpy(out, &data, sizeof(data));
    device->unmapMemory(*ubosMemory[currentImage]);
    changed[currentImage] = false;
}

struct Render::RenderTextDrawable : public Render::Node::Drawable
{
public:
    virtual void update(const vk::UniqueDevice& device, uint32_t currentText);

    std::vector<bool> changed;
    RenderTextData data;
};

void Render::RenderTextDrawable::update(const vk::UniqueDevice& device, uint32_t currentImage)
{
    if (!changed[currentImage])
        return;
    void* out = device->mapMemory(*ubosMemory[currentImage], 0, sizeof(data), {});
    memcpy(out, &data, sizeof(data));
    device->unmapMemory(*ubosMemory[currentImage]);
    changed[currentImage] = false;
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

/*
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
*/

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

    std::array<vk::DescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0] = vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, swapChainFramebuffers.size());
    poolSizes[1] = vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, swapChainFramebuffers.size());
    // enough for 1000 'widgets'
    vk::DescriptorPoolCreateInfo descriptorPoolInfo({}, swapChainFramebuffers.size() * 1000, poolSizes.size(), poolSizes.data());
    mDescriptorPool = device->createDescriptorPoolUnique(descriptorPoolInfo);
    if (!mDescriptorPool) {
        printf("failed to create descriptor pool\n");
        return;
    }

    mRenderText = std::make_shared<RenderText>(*this);

    makeDrawableDatas();
    makeRenderTree(scene);
}

vk::CommandBuffer Render::beginSingleCommand() const
{
    const auto& device = mWindow.device();

    vk::CommandBufferAllocateInfo allocInfo(*mCommandPool, vk::CommandBufferLevel::ePrimary, 1);

    const std::vector<vk::CommandBuffer> commandBuffers = device->allocateCommandBuffers(allocInfo);
    if (commandBuffers.empty() || !commandBuffers[0]) {
        printf("failed to allocate command buffer for copy\n");
        return {};
    }
    const auto& commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Render::endSingleCommand(const vk::CommandBuffer& commandBuffer) const
{
    const auto& device = mWindow.device();

    commandBuffer.end();

    const auto& graphicsQueue = mWindow.graphicsQueue();

    vk::SubmitInfo submitInfo({}, {}, {}, 1, &commandBuffer);

    graphicsQueue.submit({ submitInfo }, {});
    graphicsQueue.waitIdle();

    device->freeCommandBuffers(*mCommandPool, { commandBuffer });
}

uint32_t Render::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
    const vk::PhysicalDevice& physicalDevice = mWindow.physicalDevice();
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return std::numeric_limits<uint32_t>::max();
}

Render::VertexBuffer Render::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) const
{
    const vk::Device& device = *mWindow.device();

    vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);

    Render::VertexBuffer buf;
    buf.buffer = device.createBufferUnique(bufferInfo);
    if (!buf.buffer) {
        return Render::VertexBuffer();
    }

    vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(*buf.buffer);
    vk::MemoryAllocateInfo allocInfo(memRequirements.size, findMemoryType(memRequirements.memoryTypeBits, properties));

    buf.memory = device.allocateMemoryUnique(allocInfo);
    if (!buf.memory) {
        return Render::VertexBuffer();
    }

    device.bindBufferMemory(*buf.buffer, *buf.memory, 0);
    return buf;
}

std::shared_ptr<Render::PipelineResult> Render::makePipeline(const PipelineData& data, vk::PrimitiveTopology topology)
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

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, topology, VK_FALSE);

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

    auto pipeline = makePipeline(createData, vk::PrimitiveTopology::eTriangleStrip);
    drawableData.pipeline = pipeline;

    assert(mDrawableData.size() == DrawableColor);
    mDrawableData.push_back(std::move(drawableData));
}

void Render::makeImageDrawableData()
{
    // make pipeline
    const PipelineData createData = {
        Buffer::readFile("./image-vert.spv"),
        Buffer::readFile("./image-frag.spv"),
        {}, {},
        [](const vk::UniqueDevice& device) -> vk::UniqueDescriptorSetLayout {
            vk::DescriptorSetLayoutBinding uboLayoutBindingVert(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
            vk::DescriptorSetLayoutBinding uboLayoutBindingFrag(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
            vk::DescriptorSetLayoutBinding uboLayoutBindings[] = { uboLayoutBindingVert, uboLayoutBindingFrag };
            vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 2, uboLayoutBindings);
            return device->createDescriptorSetLayoutUnique(layoutInfo);
        }
    };

    DrawableData drawableData;

    auto pipeline = makePipeline(createData, vk::PrimitiveTopology::eTriangleStrip);
    drawableData.pipeline = pipeline;

    assert(mDrawableData.size() == DrawableImage);
    mDrawableData.push_back(std::move(drawableData));
}

void Render::makeTextDrawableData()
{
    // make pipeline
    const PipelineData createData = {
        Buffer::readFile("./text-vert.spv"),
        Buffer::readFile("./text-frag.spv"),
        []() { return RenderTextVertex::getBindingDescription(); },
        []() { return RenderTextVertex::getAttributeDescriptions(); },
        [](const vk::UniqueDevice& device) -> vk::UniqueDescriptorSetLayout {
            vk::DescriptorSetLayoutBinding uboLayoutBindingVert(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
            vk::DescriptorSetLayoutBinding uboLayoutBindingFrag(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
            vk::DescriptorSetLayoutBinding uboLayoutBindings[] = { uboLayoutBindingVert, uboLayoutBindingFrag };
            vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 2, uboLayoutBindings);
            return device->createDescriptorSetLayoutUnique(layoutInfo);
        }
    };

    DrawableData drawableData;

    auto pipeline = makePipeline(createData, vk::PrimitiveTopology::eTriangleList);
    drawableData.pipeline = pipeline;

    assert(mDrawableData.size() == DrawableText);
    mDrawableData.push_back(std::move(drawableData));
}

void Render::makeDrawableDatas()
{
    makeColorDrawableData();
    makeImageDrawableData();
    makeTextDrawableData();
}

static inline float mix(float coord, float limit, float min, float max)
{
    float r = coord / limit;
    return glm::mix(min, max, r);
}

static inline float mixScreen(float coord, float limit)
{
    return mix(coord, limit, -1.f, 1.f);
}

std::shared_ptr<Render::Node::Drawable> Render::makeColorDrawable(const Color& color, const Rect& geom)
{
    assert(mDrawableData.size() > DrawableColor);
    const auto& drawableData = mDrawableData[DrawableColor];

    const auto& swapChainFramebuffers = mWindow.swapChainFramebuffers();
    const auto& device = mWindow.device();
    const auto& pipeline = drawableData.pipeline;

    auto colorDrawable = std::make_shared<RenderColorDrawable>();

    // make ubos
    vk::DeviceSize colorSize = sizeof(RenderColorData);
    colorDrawable->ubos.reserve(swapChainFramebuffers.size());
    colorDrawable->ubosMemory.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        auto ubo = createBuffer(colorSize,
                                vk::BufferUsageFlagBits::eUniformBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        colorDrawable->ubos.push_back(std::move(ubo.buffer));
        colorDrawable->ubosMemory.push_back(std::move(ubo.memory));
    }

    // allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(swapChainFramebuffers.size(), *pipeline->descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo(*mDescriptorPool, swapChainFramebuffers.size(), layouts.data());
    auto sets = device->allocateDescriptorSetsUnique(allocInfo);
    colorDrawable->descriptorSets.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        vk::DescriptorBufferInfo bufferInfo(*colorDrawable->ubos[i], 0, sizeof(RenderColorData));
        vk::WriteDescriptorSet descriptorWrite(*sets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &bufferInfo);
        device->updateDescriptorSets({ descriptorWrite }, {});

        colorDrawable->descriptorSets.push_back(std::move(sets[i]));
    }

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::eSecondary, swapChainFramebuffers.size());
    auto commandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (commandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        std::shared_ptr<Render::Node::Drawable>();
    }

    colorDrawable->commandBuffers.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo;
        commandBuffers[i]->begin(beginInfo);

        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
        commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->layout, 0, { *colorDrawable->descriptorSets[i] }, {});
        commandBuffers[i]->draw(4, 1, 0, 0);

        commandBuffers[i]->end();

        colorDrawable->commandBuffers.push_back(std::move(commandBuffers[i]));
    }

    colorDrawable->changed = std::vector<bool>(mWindow.swapChainFramebuffers().size(), true);

    const float width = static_cast<float>(mWindow.width());
    const float height = static_cast<float>(mWindow.height());

    colorDrawable->data.color = { color.r, color.g, color.b, color.a };
    colorDrawable->data.geometry = { mixScreen(geom.x, width), mixScreen(geom.y, height), mixScreen(geom.x + geom.width, width), mixScreen(geom.y + geom.height, height) };
    //printf("ball %f %f %f %f\n", colorDrawable->data.geometry[0], colorDrawable->data.geometry[1], colorDrawable->data.geometry[2], colorDrawable->data.geometry[3]);

    return colorDrawable;
}

void Render::transitionImageLayout(const vk::UniqueImage& image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const
{
    vk::PipelineStageFlags srcStage, dstStage;
    vk::ImageMemoryBarrier barrier({}, {}, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                                   *image, { {}, 0, 1, 0, 1 });
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        srcStage = vk::PipelineStageFlagBits::eFragmentShader;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    } else {
        printf("invalid image transition\n");
        return;
    }

    vk::CommandBuffer commandBuffer = beginSingleCommand();
    commandBuffer.pipelineBarrier(srcStage, dstStage, {}, {}, {}, { barrier });
    endSingleCommand(commandBuffer);
}

void Render::copyBufferToImage(const vk::UniqueBuffer& buffer, const vk::UniqueImage& image, uint32_t width, uint32_t height) const
{
    vk::CommandBuffer commandBuffer = beginSingleCommand();

    vk::BufferImageCopy region({}, 0, 0, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, {}, { width, height, 1 });
    commandBuffer.copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal, { region });

    endSingleCommand(commandBuffer);
}

void Render::copyBufferToImage(vk::DeviceSize bufferOffset, uint32_t bufferRowLength, uint32_t bufferImageHeight, const vk::UniqueBuffer& buffer,
                               const vk::UniqueImage& image, int32_t x, int32_t y, uint32_t width, uint32_t height) const
{
    vk::CommandBuffer commandBuffer = beginSingleCommand();

    vk::BufferImageCopy region(bufferOffset, bufferRowLength, bufferImageHeight, { vk::ImageAspectFlagBits::eColor, 0, 0, 1 }, { x, y, 0 }, { width, height, 1 });
    commandBuffer.copyBufferToImage(*buffer, *image, vk::ImageLayout::eTransferDstOptimal, { region });

    endSingleCommand(commandBuffer);
}

std::shared_ptr<Render::Node::Drawable> Render::makeImageDrawable(const Scene::ImageData& image, const Rect& geom)
{
    const auto& device = mWindow.device();
    const auto& swapChainFramebuffers = mWindow.swapChainFramebuffers();

    auto imageDrawable = std::make_shared<RenderImageDrawable>();

    vk::Format vkFormat = vk::Format::eUndefined;
    int bpp = 0;
    switch (image.image->depth) {
    case 8:
        vkFormat = vk::Format::eR8Uint;
        bpp = 1;
        break;
    case 32:
        vkFormat = vk::Format::eR8G8B8A8Srgb;
        bpp = 4;
        break;
    }
    if (vkFormat == vk::Format::eUndefined) {
        printf("unknown depth? %d\n", image.image->depth);
        return {};
    }

    // allocate image
    vk::ImageCreateInfo imageCreateInfo({}, vk::ImageType::e2D, vkFormat, { image.image->width, image.image->height, 1 }, 1, 1);
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    vk::UniqueImage textureImage = device->createImageUnique(imageCreateInfo);
    if (!textureImage) {
        printf("failed to create texture image\n");
        return {};
    }
    const vk::MemoryRequirements memRequirements = device->getImageMemoryRequirements(*textureImage);
    vk::MemoryAllocateInfo memoryAllocInfo(memRequirements.size, findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    vk::UniqueDeviceMemory textureImageMemory = device->allocateMemoryUnique(memoryAllocInfo);
    if (!textureImageMemory) {
        printf("failed to allocate texture image memory\n");
        return {};
    }
    device->bindImageMemory(*textureImage, *textureImageMemory, 0);

    vk::DeviceSize imageSize = image.image->width * image.image->height * bpp;
    assert(imageSize == image.image->data.size());
    auto staging = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    void* data = device->mapMemory(*staging.memory, 0, imageSize, {});
    memcpy(data, image.image->data.data(), imageSize);
    device->unmapMemory(*staging.memory);

    transitionImageLayout(textureImage, vkFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(staging.buffer, textureImage, image.image->width, image.image->height);
    transitionImageLayout(textureImage, vkFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::ImageViewCreateInfo imageViewCreateInfo({}, *textureImage, vk::ImageViewType::e2D, vkFormat, {},
                                                { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::UniqueImageView textureImageView = device->createImageViewUnique(imageViewCreateInfo);
    if (!textureImageView) {
        printf("failed to create texture image view\n");
        return {};
    }

    vk::SamplerCreateInfo samplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear);
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16.f;
    samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = vk::CompareOp::eAlways;
    samplerCreateInfo.mipLodBias = 0.f;
    samplerCreateInfo.minLod = 0.f;
    samplerCreateInfo.maxLod = 0.f;
    vk::UniqueSampler textureImageSampler = device->createSamplerUnique(samplerCreateInfo);
    if (!textureImageSampler) {
        printf("failed to create texture image sampler\n");
        return {};
    }

    imageDrawable->imageMemory = std::move(textureImageMemory);
    imageDrawable->image = std::move(textureImage);
    imageDrawable->imageView = std::move(textureImageView);
    imageDrawable->imageSampler = std::move(textureImageSampler);

    assert(mDrawableData.size() > DrawableImage);
    const auto& drawableData = mDrawableData[DrawableImage];

    const auto& pipeline = drawableData.pipeline;

    // make ubos
    vk::DeviceSize colorSize = sizeof(RenderColorData);
    imageDrawable->ubos.reserve(swapChainFramebuffers.size());
    imageDrawable->ubosMemory.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        auto ubo = createBuffer(colorSize,
                                vk::BufferUsageFlagBits::eUniformBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        imageDrawable->ubos.push_back(std::move(ubo.buffer));
        imageDrawable->ubosMemory.push_back(std::move(ubo.memory));
    }

    // allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(swapChainFramebuffers.size(), *pipeline->descriptorSetLayout);
    vk::DescriptorSetAllocateInfo descriptorAllocInfo(*mDescriptorPool, swapChainFramebuffers.size(), layouts.data());
    auto sets = device->allocateDescriptorSetsUnique(descriptorAllocInfo);
    imageDrawable->descriptorSets.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        vk::DescriptorBufferInfo bufferInfo(*imageDrawable->ubos[i], 0, sizeof(RenderColorData));
        vk::DescriptorImageInfo imageInfo(*imageDrawable->imageSampler, *imageDrawable->imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet bufferDescriptorWrite(*sets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &bufferInfo);
        vk::WriteDescriptorSet imageDescriptorWrite(*sets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, {});
        device->updateDescriptorSets({ bufferDescriptorWrite, imageDescriptorWrite }, {});

        imageDrawable->descriptorSets.push_back(std::move(sets[i]));
    }

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::eSecondary, swapChainFramebuffers.size());
    auto commandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (commandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        std::shared_ptr<Render::Node::Drawable>();
    }

    imageDrawable->commandBuffers.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo;
        commandBuffers[i]->begin(beginInfo);

        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
        commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->layout, 0, { *imageDrawable->descriptorSets[i] }, {});
        commandBuffers[i]->draw(4, 1, 0, 0);

        commandBuffers[i]->end();

        imageDrawable->commandBuffers.push_back(std::move(commandBuffers[i]));
    }

    imageDrawable->changed = std::vector<bool>(mWindow.swapChainFramebuffers().size(), true);

    const float width = static_cast<float>(mWindow.width());
    const float height = static_cast<float>(mWindow.height());

    imageDrawable->data.geometry = { mixScreen(geom.x, width), mixScreen(geom.y, height), mixScreen(geom.x + geom.width, width), mixScreen(geom.y + geom.height, height) };

    return imageDrawable;
}

std::shared_ptr<Render::Node::Drawable> Render::makeTextDrawable(const Text& text, const Rect& geometry)
{
    const auto& device = mWindow.device();
    const auto& swapChainFramebuffers = mWindow.swapChainFramebuffers();

    auto textDrawable = std::make_shared<RenderTextDrawable>();

    uint32_t vertexCount;
    const auto renderData = mRenderText->renderText(text, geometry, vertexCount);
    const vk::UniqueImageView& imageView = mRenderText->imageView();
    if (!imageView) {
        printf("failed to get image view\n");
        return {};
    }

    vk::SamplerCreateInfo samplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear);
    samplerCreateInfo.anisotropyEnable = VK_TRUE;
    samplerCreateInfo.maxAnisotropy = 16.f;
    samplerCreateInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = vk::CompareOp::eAlways;
    samplerCreateInfo.mipLodBias = 0.f;
    samplerCreateInfo.minLod = 0.f;
    samplerCreateInfo.maxLod = 0.f;
    vk::UniqueSampler imageSampler = device->createSamplerUnique(samplerCreateInfo);
    if (!imageSampler) {
        printf("failed to create texture image sampler\n");
        return {};
    }

    textDrawable->imageSampler = std::move(imageSampler);
    textDrawable->imageView = mRenderText->imageView();

    assert(mDrawableData.size() > DrawableText);
    const auto& drawableData = mDrawableData[DrawableText];

    const auto& pipeline = drawableData.pipeline;

    // make ubos
    vk::DeviceSize colorSize = sizeof(RenderColorData);
    textDrawable->ubos.reserve(swapChainFramebuffers.size());
    textDrawable->ubosMemory.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        auto ubo = createBuffer(colorSize,
                                vk::BufferUsageFlagBits::eUniformBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        textDrawable->ubos.push_back(std::move(ubo.buffer));
        textDrawable->ubosMemory.push_back(std::move(ubo.memory));
    }

    // allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(swapChainFramebuffers.size(), *pipeline->descriptorSetLayout);
    vk::DescriptorSetAllocateInfo descriptorAllocInfo(*mDescriptorPool, swapChainFramebuffers.size(), layouts.data());
    auto sets = device->allocateDescriptorSetsUnique(descriptorAllocInfo);
    textDrawable->descriptorSets.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); ++i) {
        vk::DescriptorBufferInfo bufferInfo(*textDrawable->ubos[i], 0, sizeof(RenderColorData));
        vk::DescriptorImageInfo imageInfo(*textDrawable->imageSampler, *textDrawable->imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::WriteDescriptorSet bufferDescriptorWrite(*sets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &bufferInfo);
        vk::WriteDescriptorSet imageDescriptorWrite(*sets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, {});
        device->updateDescriptorSets({ bufferDescriptorWrite, imageDescriptorWrite }, {});

        textDrawable->descriptorSets.push_back(std::move(sets[i]));
    }

    vk::CommandBufferAllocateInfo allocCommandBufferInfo(*mCommandPool, vk::CommandBufferLevel::eSecondary, swapChainFramebuffers.size());
    auto commandBuffers = device->allocateCommandBuffersUnique(allocCommandBufferInfo);
    if (commandBuffers.empty()) {
        printf("failed to allocate command buffers!\n");
        std::shared_ptr<Render::Node::Drawable>();
    }

    textDrawable->commandBuffers.reserve(swapChainFramebuffers.size());
    for (size_t i = 0; i < commandBuffers.size(); i++) {
        vk::CommandBufferBeginInfo beginInfo;
        commandBuffers[i]->begin(beginInfo);

        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline->pipeline);
        commandBuffers[i]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipeline->layout, 0, { *textDrawable->descriptorSets[i] }, {});
        commandBuffers[i]->bindVertexBuffers(0, { renderData.buffer }, { 0 });
        commandBuffers[i]->draw(vertexCount, 1, 0, 0);
        //commandBuffers[i]->draw(6, 1, 0, 0);

        commandBuffers[i]->end();

        textDrawable->commandBuffers.push_back(std::move(commandBuffers[i]));
    }

    textDrawable->changed = std::vector<bool>(mWindow.swapChainFramebuffers().size(), true);

    const float factor = text.size / static_cast<float>(mRenderText->renderSize());
    const float tx = mix(geometry.x, mWindow.width(), -1.0f, 1.0f);
    const float ty = mix(geometry.y, mWindow.height(), -1.0f, 1.0f);

    glm::mat4 projection = glm::mat4(1.0f);
    projection = glm::scale(projection, glm::vec3(factor, factor, 1.0f));
    projection = glm::translate(projection, glm::vec3(1.0f + (tx / factor), 1.0f + (ty / factor), 0.0f));

    textDrawable->data.projection = projection;
    textDrawable->data.color = { text.color.r, text.color.g, text.color.b, text.color.a };

    return textDrawable;
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
        if (sceneItem->image.image) {
            renderNode->drawables.push_back(makeImageDrawable(sceneItem->image, sceneItem->geometry));
        }
        if (!sceneItem->text.contents.empty() && sceneItem->text.size > 0) {
            renderNode->drawables.push_back(makeTextDrawable(sceneItem->text, sceneItem->geometry));
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

void Render::renderNode(const std::shared_ptr<Node>& node, const vk::CommandBuffer& commandBuffer, uint32_t imageIndex)
{
    if (!node)
        return;

    for (const auto& drawable : node->drawables) {
        drawable->update(mWindow.device(), imageIndex);
        commandBuffer.executeCommands({ *drawable->commandBuffers[imageIndex] });
    }

    for (const auto& child : node->children) {
        renderNode(child, commandBuffer, imageIndex);
    }
}

void Render::render(const Window::RenderData& data)
{
    const auto& extent = mWindow.extent();
    const auto& renderPass = mWindow.renderPass();
    const auto& swapChainFramebuffers = mWindow.swapChainFramebuffers();
    const auto& device = mWindow.device();

    const auto& commandBuffer = beginSingleCommand();

    vk::ClearValue clearValue = vk::ClearColorValue(std::array<float,4> { 0.0f, 0.0f, 0.0f, 1.0f });
    vk::RenderPassBeginInfo renderPassInfo(*renderPass, swapChainFramebuffers[data.imageIndex], { { 0, 0 }, extent }, 1, &clearValue);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    renderNode(mRoot, commandBuffer, data.imageIndex);

    commandBuffer.endRenderPass();
    commandBuffer.end();

    vk::Semaphore waitSemaphores[] = { data.wait };
    vk::Semaphore signalSemaphores[] = { data.signal };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    const auto& graphicsQueue = mWindow.graphicsQueue();
    vk::SubmitInfo submitInfo(1, waitSemaphores, waitStages, 1, &commandBuffer, 1, signalSemaphores);

    try {
        graphicsQueue.submit({ submitInfo }, data.fence);
        graphicsQueue.waitIdle();
    } catch (const vk::Error& error) {
        printf("failed to submit draw command buffer\n");
    }

    device->freeCommandBuffers(*mCommandPool, { commandBuffer });
}
