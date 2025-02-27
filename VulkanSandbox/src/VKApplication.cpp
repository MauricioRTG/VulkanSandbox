#include "VKApplication.h"
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

void VKApplication::run() {
	initWindows();
	initVulkan();
	mainLoop();
	cleanup();
}

void VKApplication::initWindows() {
	glfwInit();
	
	//Do not create an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//Disable handling resized windows
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "VulkanSandbox Window", nullptr, nullptr);

}

void VKApplication::initVulkan() {
	createInstance();
	pickPhysicalDevice();
	createLogicalDevice();
}

void VKApplication::mainLoop() {
	//Check for events like pressing the X button until the window has been closed by the user
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void VKApplication::cleanup() {
	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void VKApplication::createInstance(){
	//Check for validation layer support
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	//Provides information about the application we are developing
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Sandbox";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_4;

	//Funciton to return the GLFW extensions it needs
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//Tells vulkan driver which global extensions and validation layers we want to use. Global means they apply to entire program not a specific device
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;

	//Include validation layer names if they are enabled
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} 
	else {
		createInfo.enabledLayerCount = 0;
	}
	
	//Create vulkan instance
	if (vkCreateInstance(&createInfo, nullptr, &instance)) {
		throw std::runtime_error("Failed to create vulkan instence!");
	}

	//printInstanceExtensionSupport();
}

void VKApplication::pickPhysicalDevice(){
	//Quary the number of physical devices
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support!");
	}

	//Get all physical device handles
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	//Select a physical device
	for (const auto& device : physicalDevices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}

}

void VKApplication::createLogicalDevice(){
	/*
	* The currently available drivers will only allow you to create a small number of queues for each queue family 
	* and you don't really need more than one. That's because you can create all of the command buffers on multiple threads 
	* and then submit them all at once on the main thread with a single low-overhead call.
	*/

	//find all the queue families of the physical device, we are only interested in creating queue with graphics capabilities
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	//Vulkan lets you assign priorities to queues to influence the scheduling of command buffer execution using floating point numbers between 0.0 and 1.0
	float queuePriority = 1.0f;

	//Describes the number of queues we want for a single queue family (in this case qwe create a sinlge queue with graphics capabilities)
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	/*Specify the se of device feature that we'll be using. 
	* These are the features that we can queried support for with vkGetPhysicalDeviceFeatures, like geometry shaders. 
	* So from the available features from that physical device we select the ones we only want using this logical device (this way we can create multiple logical devices each with different features form the physical device)
	*/

	VkPhysicalDeviceFeatures deviceFeatures{}; //We leave everything to VK_FALSE

	/* Creating the logical device */

	//Here we add pointers to the queue creation info and device feature structs
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pEnabledFeatures = &deviceFeatures;
	//Specify extensions and validation layers (device specific)
	createInfo.enabledExtensionCount = 0;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	//Intantiate logical device
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device!");
	}

	//Stores a handle to the graphics queue (created along with logical device)
	vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);

}

bool VKApplication::checkValidationLayerSupport(){
	uint32_t layerCount; 
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//Check if wanted validation layers are in the available layers
	for (const char* layerName : validationLayers){
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers){
			//Compare layer name
			if (strcmp(layerName, layerProperties.layerName) == 0){
				layerFound = true;
				break;
			}
		}

		//Exit if checking current layer is not found
		if (!layerFound){
			return false;
		}
	}

	//all layers where checked and all where found so we return true
	return true;
}

void VKApplication::printInstanceExtensionSupport(){
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	std::cout << "Available instance extensions: \n";

	for (const auto& extension : extensions) {
		std::cout << '\t' << extension.extensionName << '\n';
	}
}

bool VKApplication::isDeviceSuitable(VkPhysicalDevice device){
	/*Check for features and properties you want in your physical device
	VkPhysicalDeviceProperties physicalDeviceProperties;
	VkPhysicalDeviceFeatures physicalDeviceFeatures;
	vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(device, &physicalDeviceFeatures);

	return physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		physicalDeviceFeatures.geometryShader;*/

	//Other alternative is to rateDeviceSuitability with a score that increases based on the features we want selecting the best

	//Select based on having the queue families tyes that we want
	QueueFamilyIndices indices = findQueueFamilies(device);

	return indices.isComplete();
}

QueueFamilyIndices VKApplication::findQueueFamilies(VkPhysicalDevice device) const {
	QueueFamilyIndices indices;
	//Assign index to queue families that could be found
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies){
		//Masks 32 bits, using VK_QUEUE_GRAPHICS_BIT position and if after the result is 1 this means the queue family has graphics capabilities
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
			indices.graphicsFamily = i;
		}

		//Exit early if all required indices where found
		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}
