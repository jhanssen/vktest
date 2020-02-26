cmake_minimum_required(VERSION 3.11)
include_directories(${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(vk main.cpp)

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
target_link_libraries(vk glm::glm glfw ${GLFW_LIBRARIES} Vulkan::Vulkan shaderc nlohmann_json::nlohmann_json)
add_definitions(-DVULKAN_SDK=${VULKAN_SDK})
