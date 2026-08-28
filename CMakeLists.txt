cmake_minimum_required(VERSION 3.24)
project(Voxis LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

include(CompilerWarnings)
include(Dependencies)

file(GLOB_RECURSE VOXIS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_executable(voxis ${VOXIS_SOURCES})

target_include_directories(voxis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(voxis SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})

target_link_libraries(voxis PRIVATE
    volk
    SDL2::SDL2-static
    glm::glm
    EnTT::EnTT
    VulkanMemoryAllocator
    spdlog::spdlog
    Vulkan::shaderc_combined
)

set_project_warnings(voxis)

add_custom_command(TARGET voxis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
        $<TARGET_FILE_DIR:voxis>/src/shaders
    COMMENT "Copying shaders to build output directory"
)
