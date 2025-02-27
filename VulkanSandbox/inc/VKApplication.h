#pragma once

#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
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

	bool isComplete() const {
		return graphicsFamily.has_value();
	}
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

	//Main funcitions for Run()
	void initWindows();

	void initVulkan();

	void mainLoop();

	void cleanup();

	//Functions for initVulkan

	void createInstance();

	void pickPhysicalDevice();

	void createLogicalDevice();

	//Helper functions
	
	bool checkValidationLayerSupport();

	void printInstanceExtensionSupport();

	bool isDeviceSuitable(VkPhysicalDevice device);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
};