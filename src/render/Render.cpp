#include "Render.h"
#include <Buffer.h>

static vk::UniqueShaderModule createShaderModule(const vk::UniqueDevice& device, const Buffer& buffer)
{
    vk::ShaderModuleCreateInfo createInfo({}, buffer.size(), reinterpret_cast<const uint32_t*>(buffer.data()));
    return device->createShaderModuleUnique(createInfo);
}

Render::Render(const Scene& scene, const Window& window)
{
    const auto& device = window.device();
    auto vertShaderModule = createShaderModule(device, Buffer::readFile("vert.spv"));
    auto fragShaderModule = createShaderModule(device, Buffer::readFile("frag.spv"));
    if (!vertShaderModule || !fragShaderModule) {
        printf("couldn't create shader modules\n");
        return;
    }
}

void Render::render(const Window::RenderData& data)
{
}
