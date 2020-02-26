cmake_minimum_required(VERSION 3.11)

set(SHADERC_SKIP_TESTS ON)
set(JSON_BuildTests OFF)
set(JSON_Install OFF)

execute_process(
    COMMAND ./utils/git-sync-deps
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/shaderc
    )

add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(shaderc)
add_subdirectory(json)
