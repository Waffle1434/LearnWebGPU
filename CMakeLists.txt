cmake_minimum_required(VERSION 3.0...3.25)
project(
    WebGPU
    VERSION 0.1.0
    LANGUAGES CXX C
)

add_executable(WebGPU_App main.cpp)
add_subdirectory(glfw) # Native Window (https://www.glfw.org/)
add_subdirectory(webgpu) # Case sensitive, can't be WebGPU
add_subdirectory(glfw3webgpu)
target_link_libraries(WebGPU_App PRIVATE glfw webgpu glfw3webgpu)
target_copy_webgpu_binaries(WebGPU_App)

set_target_properties(WebGPU_App PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    target_compile_options(WebGPU_App PRIVATE /W4)
else()
    target_compile_options(WebGPU_App PRIVATE -Wall -Wextra -pedantic)
endif()
