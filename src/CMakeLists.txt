cmake_minimum_required(VERSION 3.11)
include_directories(${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    main.cpp
    Buffer.cpp
    Decoder.cpp
    Fetch.cpp
    Rect.cpp
    Window.cpp
    render/Render.cpp
    scene/Scene.cpp
    text/Font.cpp
    text/Layout.cpp
    )

add_executable(vk ${SOURCES})

if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

# fuckings google doing nothing right
include_directories(${THIRDPARTY_DIR}/libwebp/src)
# png not being better
include_directories(${THIRDPARTY_DIR}/libpng)
include_directories(${THIRDPARTY_BINARY_DIR}/libpng)

include_directories(${THIRDPARTY_DIR}/LUrlParser)

include_directories(${THIRDPARTY_DIR}/libjpeg-turbo)

include_directories(${THIRDPARTY_DIR}/harfbuzz/src)

include_directories(${THIRDPARTY_DIR}/msdfgen)

add_library(LUrlParser STATIC ${THIRDPARTY_DIR}/LUrlParser/LUrlParser.cpp)

find_package(OpenSSL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(ICU COMPONENTS uc i18n REQUIRED)
include_directories(${ICU_INCLUDE_DIRS})

target_link_libraries(vk glm::glm glfw ${GLFW_LIBRARIES} Vulkan::Vulkan
    httplib shaderc nlohmann_json::nlohmann_json png_static webpdecoder
    turbojpeg-static LUrlParser OpenSSL::SSL OpenSSL::Crypto lib_msdfgen
    harfbuzz ICU::uc ICU::i18n)
add_definitions(-DVULKAN_SDK=${VULKAN_SDK} -DCPPHTTPLIB_OPENSSL_SUPPORT)
