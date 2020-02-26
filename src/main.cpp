#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

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

static std::vector<uint8_t> readFile(const std::string& filename)
{
    FILE* f = fopen(filename.c_str(), "r");
    if (!f)
        return std::vector<uint8_t>();
    fseek(f, 0, SEEK_END);
    const auto sz = ftell(f);
    if (sz == 0) {
        fclose(f);
        return std::vector<uint8_t>();
    }
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data;
    data.resize(sz);
    fread(data.data(), 1, sz, f);
    fclose(f);
    return data;

}

int main(int argc, char** argv)
{
#ifdef VULKAN_SDK
#ifdef __APPLE__
    if (!getenv("VK_ICD_FILENAMES")) {
        setenv("VK_ICD_FILENAMES", TOSTRING(VULKAN_SDK) "/etc/vulkan/icd.d/MoltenVK_icd.json", 0);
    }
#endif
#endif

    glfwInit();

    return 0;
}
