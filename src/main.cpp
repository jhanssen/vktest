#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <stdio.h>

#include <optional>
#include <array>
#include <vector>
#include <string>
#include <cstring>
#include <unordered_set>

#include "Scene.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif

const int WIDTH = 1280;
const int HEIGHT = 720;
const int MAX_FRAMES_IN_FLIGHT = 2;

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Needs a json argument\n");
        return 1;
    }

#ifdef VULKAN_SDK
#ifdef __APPLE__
    if (!getenv("VK_ICD_FILENAMES")) {
        setenv("VK_ICD_FILENAMES", TOSTRING(VULKAN_SDK) "/etc/vulkan/icd.d/MoltenVK_icd.json", 0);
    }
#endif
#endif

    glfwInit();

    /*
    Decoder decoder(Decoder::Format_Auto);
    auto webp = decoder.decode("/Users/jhanssen/Downloads/1.webp");
    printf("got webp %u %u\n", webp.width, webp.height);
    auto png = decoder.decode("/Users/jhanssen/Downloads/pngquant2.png");
    printf("got png %u %u\n", png.width, png.height);
    */

    Scene scene = Scene::sceneFromJSON(argv[1]);

    return 0;
}
