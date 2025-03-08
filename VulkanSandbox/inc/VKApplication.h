#pragma once

#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
//For Surface
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//For Surface
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#define VK_USE_PLATFORM_WIN32_KHR

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//Stores indices for different queue types
struct QueueFamilyIndices {
	/*
	* std::optional is a wrapper that contains no value until you assign something to it. Allowing to distinguish between the case of a value existing or not with has_value()
	* We use optional to indicating whether a particular queue family was found
	*/
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VKApplication {
public:
	void run();
private:
	class GLFWwindow* window;
	
	/*  Vulkan handles */

	//Vulkan on your system, we configure vulkan
	VkInstance instance;

	//Handle of hardware device (GPU)
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	//Main interface to a physical device (active configuration for features we want to use from the physical device) 
	VkDevice logicalDevice;

	//Receive commands to be executed on physical device (queues are automatically created along with the logical device)
	//We can use the vkGetDeviceQueue function to retrieve queue handles for each queue family. 
	VkQueue graphicsQueue;

	//Queue that supports presentation
	VkQueue presentQueue;

	//Window of you OS, we conect wulkan and the window system with a extension from glfw
	VkSurfaceKHR surface;

	//Send image (presents) to a monitor, provides image to render into
	VkSwapchainKHR swapChain;

	//Images from swap chain
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	//Image views: describes how to access the image and which part of the image to access, this ares used by the render pipline to access the images
	std::vector<VkImageView> swapChainImageViews;

	//Render pass: Specifies how many color and depth buffers there will be, how many samples to use for each of them and how their contents should be handled throughout the rendering operations. 
	VkRenderPass renderPass;

	//Pipeline layout
	VkPipelineLayout pipelineLayout;

	//Graphics pipeline
	VkPipeline graphicsPipeline;

	//Holds frambuffers: references all of the VkImage view objects that represent the attachments. Create a frame buffer for all the images in the swap chain and use the one that corresponds to the retrieved image at drawing time
	std::vector<VkFramebuffer> swapChainFramebuffers;

	//Main funcitions for Run()
	void initWindows();

	void initVulkan();

	void mainLoop();

	void cleanup();

	//Functions for initVulkan

	void createInstance();

	void createSurface();

	void pickPhysicalDevice();

	void createLogicalDevice();

	void createSwapChain();

	void createImageViews();
 
	void createRenderPass();

	void createGraphicsPipeline();

	void createFramebuffers();

	//Helper functions
	
	bool checkValidationLayerSupport();

	void printInstanceExtensionSupport();

	bool isDeviceSuitable(VkPhysicalDevice device);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

	//SwapChain
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	//Get surface supported capabilities, formats, and presnet modes
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

	//Choosing swap chain settings
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	//Shader modules

	//To create the SPIR-V bytcode files the command is glslc shader.vert -o vert.spv glslc shader.frag -o frag.spv

	//The readFile function will read all of the bytes from the specified file and return them in a byte array managed by std::vector (for the SPIR-V file)
	static std::vector<char> readFile(const std::string& filename);

	VkShaderModule createShaderModule(const std::vector<char>& code) const;
};