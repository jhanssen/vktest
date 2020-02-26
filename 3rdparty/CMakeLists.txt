cmake_minimum_required(VERSION 3.11)

set(SHADERC_SKIP_TESTS ON)

execute_process(
    COMMAND ./utils/git-sync-deps
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/shaderc
    )

add_subdirectory(glfw ${CMAKE_CURRENT_BINARY_DIR}/glfw)
add_subdirectory(glm ${CMAKE_CURRENT_BINARY_DIR}/glm)
add_subdirectory(shaderc ${CMAKE_CURRENT_BINARY_DIR}/shaderc)
