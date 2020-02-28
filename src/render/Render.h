#ifndef RENDER_H
#define RENDER_H

#include <scene/Scene.h>
#include <Window.h>

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
    vk::UniquePipeline mPipeline;
    VertexBuffer mVertexBuffer;
};

#endif
