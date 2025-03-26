
# Vulkan Sandbox
Vulkan Sandbox is a vulkan application i use to learn the vulkan API. The purpose of this application is to be a base for a game engine and to test rendering techniques.

## TODO

* Multiple meshes
* Camera/handle user input
* Refactor code arquitecture
* Multiple textures
* Lighting 

## Building

### Windows

Prerequisites:

* [Git for Windows](https://github.com/git-for-windows/git/releases)
* [Vulkan SDK](https://vulkan.lunarg.com/) 
* A [Vulkan capable GPU](https://vulkan.gpuinfo.org/listdevices.php) with the appropriate drivers installed
* [GLM](https://github.com/g-truc/glm) a C++ mathematics library 
* [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader/tree/v1.0.6) to load 3D models
* [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h) to load textures

1. Open git bash
2. Git clone the project:

~~~
https://github.com/MauricioRTG/VulkanSandbox.git
~~~

### Visual Studio

1. Install [Visual Studio Community](https://www.visualstudio.com) with the Visual Studio C++ component.
2. Open the VulkanSandBox.sln
3. Select the desired configuration and platform (relased, x64)
4. Build project (press green play button)
