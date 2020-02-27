#ifndef RENDER_H
#define RENDER_H

#include <scene/Scene.h>
#include <Window.h>

class Render
{
public:
    Render(const Scene& scene, const Window& window);

    void render(const Window::RenderData& data);

private:
    vk::UniquePipeline mPipeline;
};

#endif
