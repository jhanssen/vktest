#ifndef WINDOW_H
#define WINDOW_H

#include <memory>
#include <functional>
#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window
{
public:
    Window(uint32_t width, uint32_t height);
    ~Window();

    void exec();

    // vulkan accessors
    const vk::UniqueInstance& instance() const { return mInstance; }
    const vk::PhysicalDevice& physicalDevice() const { return mPhysicalDevice; }
    const vk::UniqueDevice& device() const { return mDevice; }
    const vk::UniqueSurfaceKHR& surface() const { return mSurface; }
    const vk::Queue& presentQueue() const { return mPresentQueue; }
    const vk::Queue& graphicsQueue() const { return mGraphicsQueue; }
    const vk::Extent2D& extent() const { return mExtent; }
    const vk::UniqueSwapchainKHR& swapChain() const { return mSwapChain; }
    const std::vector<vk::Image>& swapChainImages() const { return mSwapChainImages; }
    const std::vector<vk::ImageView>& swapChainImageViews() const { return mSwapChainImageViews; }
    const std::vector<vk::Framebuffer>& swapChainFramebuffers() const { return mSwapChainFramebuffers; }
    const vk::UniqueRenderPass& renderPass() const { return mRenderPass; }
    uint32_t presentFamily() const { return mPresentFamily; }
    uint32_t graphicsFamily() const { return mGraphicsFamily; }
    const std::shared_ptr<GLFWwindow>& window() const { return mWindow; }

    struct RenderData
    {
        const Window* window;
        uint32_t imageIndex, currentFrame;
        vk::Framebuffer fb;
        vk::Semaphore wait;
        vk::Semaphore signal;
        vk::Fence fence;
    };
    void registerRender(std::function<void(const RenderData&)>&& render)
    {
        mRender = std::move(render);
    }

private:
    vk::UniqueInstance mInstance;
    VkDebugUtilsMessengerEXT mDebugMessenger;
    vk::PhysicalDevice mPhysicalDevice;
    vk::UniqueDevice mDevice;
    vk::UniqueSurfaceKHR mSurface;
    vk::Queue mPresentQueue, mGraphicsQueue;
    vk::Extent2D mExtent;
    vk::UniqueSwapchainKHR mSwapChain;
    std::vector<vk::Image> mSwapChainImages;
    std::vector<vk::ImageView> mSwapChainImageViews;
    vk::UniqueRenderPass mRenderPass;
    std::vector<vk::Framebuffer> mSwapChainFramebuffers;
    uint32_t mPresentFamily, mGraphicsFamily;
    std::shared_ptr<GLFWwindow> mWindow;

    std::function<void(const RenderData&)> mRender;
};

#endif
