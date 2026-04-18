cmake_minimum_required(VERSION 3.14)
project(NesEmulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Fetch dependencies
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

find_package(Vulkan REQUIRED)

include_directories(include)

add_executable(NesEmulator
    src/main.cpp
    src/CPU.cpp
    src/Bus.cpp
    src/PPU.cpp
    src/APU.cpp
    src/Renderer.cpp
    src/Cartridge.cpp
    src/Mapper.cpp
    src/Mapper_000.cpp
    src/Mapper_001.cpp
)

target_link_libraries(NesEmulator PRIVATE Vulkan::Vulkan glfw glm)

# Compile Shaders as a pre-build step
find_program(GLSLC glslc)
if(GLSLC)
    add_custom_command(
        TARGET NesEmulator PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        COMMAND ${GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.spv
        COMMAND ${GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.spv
        COMMENT "Compiling shaders"
    )
endif()

# Copy required assets (shaders and ROM) to the output directory post-build
add_custom_command(
    TARGET NesEmulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:NesEmulator>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/nestest.nes $<TARGET_FILE_DIR:NesEmulator>/nestest.nes
    COMMENT "Copying assets to executable directory"
)
