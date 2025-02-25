cmake_minimum_required(VERSION 3.10)
project(VulkanSandbox)

# Find Vulkan
find_package(Vulkan CONFIG REQUIRED)

# Find GLFW
find_package(glfw3 CONFIG REQUIRED)

# Find GLM (Header-only)
find_package(glm CONFIG REQUIRED)

# Add source files
add_executable(VulkanSandbox 
    VulkanSandbox/src/main.cpp
    VulkanSandbox/src/VKApplication.cpp
)

# Include Directories (GLM is header-only, Vulkan and GLFW provide includes)
target_include_directories(VulkanSandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/VulkanSandbox/inc
)

# Link libraries
target_link_libraries(VulkanSandbox PRIVATE Vulkan::Vulkan glfw)