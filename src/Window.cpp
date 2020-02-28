#include "Window.h"
#include <unordered_set>
#include <optional>
#include <array>

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static bool enableValidationLayers = true;
#endif

const int MAX_FRAMES_IN_FLIGHT = 2;

static const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static bool checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

static QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& surface)
{
    QueueFamilyIndices indices;

    const auto queueFamilies = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (device.getSurfaceSupportKHR(i, *surface)) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

static bool checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    const auto availableExtensions = device.enumerateDeviceExtensionProperties();

    std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

static SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& surface)
{
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(*surface);
    details.formats = device.getSurfaceFormatsKHR(*surface);
    details.presentModes = device.getSurfacePresentModesKHR(*surface);

    return details;
}

static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = { width, height };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

static bool isDeviceSuitable(vk::PhysicalDevice device, const vk::UniqueSurfaceKHR& surface)
{
    QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool swapChainAdequate = false;
    const bool extensionsSupported = checkDeviceExtensionSupport(device);

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    printf("validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = vk::DebugUtilsMessengerCreateInfoEXT(
        {},
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        debugCallback
        );
}

Window::Window(uint32_t width, uint32_t height)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(width, height, "Vulkan window", nullptr, nullptr);
    mWindow.reset(window, [](GLFWwindow* w) {
        glfwDestroyWindow(w);
    });

#ifndef NDEBUG
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        printf("wanted validation layers but none available\n");
        enableValidationLayers = false;
    }
#endif

    auto getRequiredExtensions = [&]() -> std::vector<const char*> {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    };

    auto instanceExtensions = getRequiredExtensions();

    uint32_t layerCount = 0;
    const char* const* layerNames = nullptr;
    if (enableValidationLayers) {
        layerCount = static_cast<uint32_t>(validationLayers.size());
        layerNames = validationLayers.data();
    }

    vk::ApplicationInfo applicationInfo("Scenery", 1, "No Engine", 1, VK_API_VERSION_1_0);
    vk::InstanceCreateInfo instanceCreateInfo({} /* flags */, &applicationInfo,
                                              layerCount, layerNames,
                                              instanceExtensions.size(), instanceExtensions.data());

    vk::DebugUtilsMessengerCreateInfoEXT createDebugInfo;
    if (enableValidationLayers) {
        populateDebugMessengerCreateInfo(createDebugInfo);
        instanceCreateInfo.setPNext(&createDebugInfo);
    }

    mInstance = vk::createInstanceUnique(instanceCreateInfo);
    if (!mInstance) {
        printf("unable to create vk instance");
        return;
    }

    printf("available extensions:\n");
    const auto availableExts = vk::enumerateInstanceExtensionProperties();
    for (const auto& ext : availableExts) {
        printf("\t%s\n", ext.extensionName);
    }

    if (enableValidationLayers) {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(*mInstance, &static_cast<const VkDebugUtilsMessengerCreateInfoEXT&>(createInfo), nullptr, &mDebugMessenger) != VK_SUCCESS) {
            printf("failed to set up debug messenger!\n");
            return;
        }
    }

    const auto physicalDevices = mInstance->enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
        printf("no physical devices\n");
        return;
    }

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*mInstance, mWindow.get(), nullptr, &surface) != VK_SUCCESS) {
        printf("failed to create surface\n");
        return;
    }
    mSurface.reset(surface);

    for (const auto& device : physicalDevices) {
        if (isDeviceSuitable(device, mSurface)) {
            mPhysicalDevice = device;
            break;
        }
    }

    if (!mPhysicalDevice) {
        printf("failed to find suitable physical device\n");
        return;
    }

    QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice, mSurface);

    mPresentFamily = indices.presentFamily.value();
    mGraphicsFamily = indices.graphicsFamily.value();

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::unordered_set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1, &queuePriority);
        queueCreateInfos.push_back(queueCreateInfo);
    }

    uint32_t deviceLayerCount = 0;
    const char* const* deviceLayerNames = nullptr;
    if (enableValidationLayers) {
        deviceLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceLayerNames = validationLayers.data();
    }

    vk::DeviceCreateInfo createDeviceInfo({}, queueCreateInfos.size(), queueCreateInfos.data(),
                                          deviceLayerCount, deviceLayerNames,
                                          deviceExtensions.size(), deviceExtensions.data());

    mDevice = mPhysicalDevice.createDeviceUnique(createDeviceInfo);
    if (!mDevice) {
        printf("unable to create logical device\n");
        return;
    }

    mPresentQueue = mDevice->getQueue(indices.presentFamily.value(), 0);
    mGraphicsQueue = mDevice->getQueue(indices.graphicsFamily.value(), 0);
    if (!mPresentQueue || !mGraphicsQueue) {
        printf("unable to get device queues\n");
        return;
    }

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mPhysicalDevice, mSurface);
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    mExtent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createSwapchainInfo({}, *mSurface, imageCount, surfaceFormat.format, surfaceFormat.colorSpace, mExtent, 1, vk::ImageUsageFlagBits::eColorAttachment);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createSwapchainInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createSwapchainInfo.queueFamilyIndexCount = 2;
        createSwapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createSwapchainInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createSwapchainInfo.queueFamilyIndexCount = 0; // Optional
        createSwapchainInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createSwapchainInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createSwapchainInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createSwapchainInfo.presentMode = presentMode;
    createSwapchainInfo.clipped = VK_TRUE;

    mSwapChain = mDevice->createSwapchainKHRUnique(createSwapchainInfo);
    if (!mSwapChain) {
        printf("failed to create swap chain!\n");
        return;
    }

    mSwapChainImages = mDevice->getSwapchainImagesKHR(*mSwapChain);

    vk::Format swapChainImageFormat = surfaceFormat.format;

    mSwapChainImageViews.resize(mSwapChainImages.size());
    for (size_t i = 0; i < mSwapChainImages.size(); i++) {
        vk::ImageViewCreateInfo createImageViewInfo(
            {}, mSwapChainImages[i], vk::ImageViewType::e2D, swapChainImageFormat,
            {
                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity
            },
            {
                {}, 0, 1, 0, 1
            });
        mSwapChainImageViews[i] = mDevice->createImageView(createImageViewInfo);
        if (!mSwapChainImageViews[i]) {
            printf("failed to create image view %zu!\n", i);
            return;
        }
    }

    vk::AttachmentDescription colorAttachment({}, swapChainImageFormat, vk::SampleCountFlagBits::e1,
                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                                              vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                                              vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, 1, &colorAttachmentRef);

    vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass);

    mRenderPass = mDevice->createRenderPassUnique(renderPassInfo);
    if (!mRenderPass) {
        printf("failed to create render pass!\n");
        return;
    }

    mSwapChainFramebuffers.resize(mSwapChainImageViews.size());
    for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
        vk::ImageView attachments[] = {
            mSwapChainImageViews[i]
        };

        vk::FramebufferCreateInfo framebufferInfo({}, *mRenderPass, 1, attachments, mExtent.width, mExtent.height, 1);
        mSwapChainFramebuffers[i] = mDevice->createFramebuffer(framebufferInfo);
        if (!mSwapChainFramebuffers[i]) {
            printf("failed to create framebuffer!\n");
            return;
        }
    }
}

Window::~Window()
{
    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(*mInstance, mDebugMessenger, nullptr);
    }
}

void Window::exec()
{
    std::vector<vk::Semaphore> imageAvailableSemaphores;
    std::vector<vk::Semaphore> renderFinishedSemaphores;
    std::vector<vk::Fence> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(mSwapChainImages.size());

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphores[i] = mDevice->createSemaphore(semaphoreInfo);
        renderFinishedSemaphores[i] = mDevice->createSemaphore(semaphoreInfo);
        inFlightFences[i] = mDevice->createFence(fenceInfo);
        if (!imageAvailableSemaphores[i] || !renderFinishedSemaphores[i] || !inFlightFences[i]) {
            printf("failed to create sync objects for a frame!\n");
            return;
        }
    }

    uint32_t currentFrame = 0;
    auto drawFrame = [&]() -> void {
        if (!mRender)
            return;

        mDevice->waitForFences({ inFlightFences[currentFrame] }, VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = mDevice->acquireNextImageKHR(*mSwapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], {}).value;
        if (imagesInFlight[imageIndex]) {
            mDevice->waitForFences({ imagesInFlight[imageIndex] }, VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        mDevice->resetFences({ inFlightFences[currentFrame] });

        const RenderData renderData = {
            this,
            imageIndex, currentFrame,
            mSwapChainFramebuffers[imageIndex],
            imageAvailableSemaphores[currentFrame],
            renderFinishedSemaphores[currentFrame],
            inFlightFences[currentFrame],
        };
        mRender(renderData);

        vk::PresentInfoKHR presentInfo(1, &renderFinishedSemaphores[currentFrame], 1, &*mSwapChain, &imageIndex);
        mPresentQueue.presentKHR(presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    };

    while(!glfwWindowShouldClose(mWindow.get())) {
        glfwPollEvents();
        drawFrame();
    }
}
