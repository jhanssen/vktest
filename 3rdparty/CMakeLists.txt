cmake_minimum_required(VERSION 3.11)

set(SHADERC_SKIP_TESTS ON)
set(JSON_BuildTests OFF)
set(JSON_Install OFF)
set(PNG_TESTS OFF)
set(PNG_SHARED OFF)
set(HB_BUILD_TESTS OFF)
set(HB_BUILD_UTILS OFF)

set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS 1)
set(CMAKE_INSTALL_DOCDIR "share/doc")

execute_process(
    COMMAND ./utils/git-sync-deps
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/shaderc
    )

add_subdirectory(cpp-httplib)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(shaderc)
add_subdirectory(json)
add_subdirectory(libpng)
add_subdirectory(libwebp)
add_subdirectory(libjpeg-turbo)
add_subdirectory(harfbuzz)
add_subdirectory(msdfgen)
