#include "VKApplication.h"
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <set>
#include <algorithm>
#include <fstream>

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
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createGraphicsPipeline();
}

void VKApplication::mainLoop() {
	//Check for events like pressing the X button until the window has been closed by the user
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void VKApplication::cleanup() {
	for (auto imageView : swapChainImageViews){
		vkDestroyImageView(logicalDevice, imageView, nullptr);
	}
	vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}

void VKApplication::createInstance(){
	//Check for validation layer support
	if (enableValidationLayers && !checkValidationLayerSupport()){
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

void VKApplication::createSurface(){
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}

	/*This is a glfw helper function that does the next steps:
	*
	* 1.Creating a VkWin32SurfaceCreateInfoKHR and filling it
	*
	* VkWin32SurfaceCreateInfoKHR createInfo{};
	* createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	* createInfo.hwnd = glfwGetWin32Window(window);
	* createInfo.hinstance = GetModuleHandle(nullptr);

	* 2. And then creating the WIN32 Surface
	* if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
	* 	throw std::runtime_error("failed to create window surface!");
	* }
	*/
}

void VKApplication::pickPhysicalDevice(){
	//Quary the number of physical devices
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0){
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

	//We need multiple VKDeviceQUeueCreateIngot stucts to create a queue form both families (graphics and presentation)
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	//Vulkan lets you assign priorities to queues to influence the scheduling of command buffer execution using floating point numbers between 0.0 and 1.0
	float queuePriority = 1.0f;
	//For each queue family one that support presentation and/or graphics (one queue family can have both, or they are supported by different queue families indicated by the Queue indice)
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		//Describes the number of queues we want for a single queue family (in this case qwe create a sinlge queue with graphics capabilities)
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	

	/*Specify the se of device feature that we'll be using. 
	* These are the features that we can queried support for with vkGetPhysicalDeviceFeatures, like geometry shaders. 
	* So from the available features from that physical device we select the ones we only want using this logical device (this way we can create multiple logical devices each with different features form the physical device)
	*/

	VkPhysicalDeviceFeatures deviceFeatures{}; //We leave everything to VK_FALSE

	/* Creating the logical device */

	//Here we add pointers to the queue creation info and device feature structs
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	//Specify extensions and validation layers (device specific)
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
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

	//Stores a handle to the presentation queue (created along with logical device)
	vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
}

void VKApplication::createSwapChain(){
	//Get physical device surface available supported capabilities, formats and present modes
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

	//From the available formats, present modes and capanilities choose the ones we want based on the physical device
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	//We should also make sure to not exceed the maximum number of images, also 0 in maxImageCount is a special value that means that there is no maximum
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	//Swapchain create info
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; //Amount of layers each image consists of, is 1 unless you are developing a steroscopic 3D application
	//specifies what kind of operations we'll use the images in the swap chain for. In this case we're going to render directly to them, which means that they're used as color attachment. It is also possible that you'll render images to a separate image first to perform operations like post-processing. In that case you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use a memory operation to transfer the rendered image to a swap chain image.
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	//Specify how to handle swap chain images that will be used across multiple queue families
	//That will be the case in our applicaiton if the graphics queue family is different from the presentation queue
	//We'll be drawing on the images in the swap chain from the graphics queue and then submitting them on the presentation queue.
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	//If the graphics queue family id different fromt eh presentation queue family
	if (indices.graphicsFamily != indices.presentFamily){
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Images can be used across multiple queue families without explicit ownership transfers.
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family. This option offers the best performance.
		createInfo.queueFamilyIndexCount = 0; //optional
		createInfo.pQueueFamilyIndices = nullptr;
	}

	//We can specify that a certain transform should be applied to images in the swap chain if it is supported (supportedTransforms in capabilities), like a 90 degree clockwise rotation or horizontal flip. To specify that you do not want any transformation, simply specify the current transformation.
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

	//The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system. You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE; //If the clipped member is set to VK_TRUE then that means that we don't care about the color of pixels that are obscured, for example because another window is in front of them. Unless you really need to be able to read these pixels back and get predictable results, you'll get the best performance by enabling clipping.
	createInfo.oldSwapchain = VK_NULL_HANDLE; //With Vulkan it's possible that your swap chain becomes invalid or unoptimized while your application is running, for example because the window was resized. In that case the swap chain actually needs to be recreated from scratch and a reference to the old one must be specified in this field. For now we assume that we'll only ever create onw swap chaim

	//Create Swapchain 
	if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create swap chain!");
	}

	//Get swap chain images
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());

	//Get images format and extent
	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;
}

void VKApplication::createImageViews()
{
	//To use any VkImage, including those in the swap chain, in the render pipeline we have to create a VkImageView object. An image view is quite literally a view into an image. It describes how to access the image and which part of the image to access, for example if it should be treated as a 2D texture depth texture without any mipmapping levels.

	//The amount of images views is the same amount of images
	swapChainImageViews.resize(swapChainImages.size());

	//Iterate over all the swap chain images and create the images views with a creatInfo struct
	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; //he viewType parameter allows you to treat images as 1D textures, 2D textures, 3D textures and cube maps.
		createInfo.format = swapChainImageFormat;
		//The components field allows you to swizzle (i.e., rearrange)  the color channels around of an image view. For example, you can map all of the channels to the red channel for a monochrome texture (imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_R;  // Map red to blue). You can also map constant values of 0 and 1 to a channel. In our case we'll stick to the default mapping.
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Keeps the original channel.
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		//The subresourceRange field describes what the image's purpose is and which part of the image should be accessed. Our images will be used as color targets without any mipmapping levels or multiple layers.
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		//Create Image View
		if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image view!");
		}
	}
}

void VKApplication::createGraphicsPipeline(){
	//Read shader SPIR-V Files
	auto vertShaderCode = readFile("VulkanSandbox/shaders/vert.spv");
	auto fragShaderCode = readFile("VulkanSandbox/shaders/frag.spv");

	//Create Shader modules
	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	//Shader stage creation

	//Vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main"; //Entypoint of shader

	//Fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	//Destroy shader modules
	vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
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

	//Select based on having the queue families tyes that we want (that support Preentation and Graphics)
	QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);
	//Check for adequate swapChain support in surface with correct formats and presentation modes
	bool swapChainAdequate = false;
	if (extensionsSupported) {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

QueueFamilyIndices VKApplication::findQueueFamilies(VkPhysicalDevice device) const {
	QueueFamilyIndices indices;
	//Assign index to queue families that could be found
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies){
		//Masks 32 bits, using VK_QUEUE_GRAPHICS_BIT position and if after the result is 1 this means the queue family has graphics capabilities
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT){
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		//Exit early if all required indices where found
		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

bool VKApplication::checkDeviceExtensionSupport(VkPhysicalDevice device){
	//Get available extensions
	uint32_t extensionCount; 
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	//Loop available extensions and remove them from requiredExtenisons if requiredExtensions is empty then it has all required extensions
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

SwapChainSupportDetails VKApplication::querySwapChainSupport(VkPhysicalDevice device) const{
	SwapChainSupportDetails details;

	//Get Surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	//Get Surface formats
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	//Get Surface Present Modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR VKApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
	//VK_FORMAT_B8G8R8A8_SRGB means that we store the B, G, R and alpha channels in that order with an 8 bit unsigned integer
	//for a total of 32 bits per pixel. The colorSpace member indicates if the SRGB color space is supported or not using the VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	//If it fails to find the format we want, just pick the first one
	return availableFormats[0];
}

VkPresentModeKHR VKApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes){
	//VK_PRESENT_MODE_MAILBOX_KHR is a very nice trade-off if energy usage is not a concern. It allows us to avoid tearing 
	// while still maintaining a fairly low latency by rendering new images that are as up-to-date as possible right until the vertical blank. 
	// On mobile devices, where energy usage is more important, you will probably want to use VK_PRESENT_MODE_FIFO_KHR instead.
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;

	/*
	* VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away without wating for vertical blank or storing the image in a queue, which may result in tearing.
	* VK_PRESENT_MODE_FIFO_KHR(avoids tearing): The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
	* VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
	* VK_PRESENT_MODE_MAILBOX_KHR(avoids tearing and provides low letancy, but has energy cost becasue of discard): This is another variation of the second mode that waits for the vertical blank (vertical sync signal). Instead of blocking the application when the queue is full, the image that is already in the single-queue are simply replaced with the newer one. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
	*/
}

VkExtent2D VKApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities){
	//Check if Vulkan has already specified a fixed extent (resolution)
	//if 'curentExtent.width' is not 'std::numeric_limits<uint32_t>::max()', it means Vulkan has already set the resolution for us,
	//and we should use the value directly
	//'std::numeric_limits<uint32_t>::max()' returns the maximum possible value that can be stored in a uint32_t (which is an unsigned 32-bit integer).
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		//If vulkan gives us a max uint32_t as the width, it means we are allowd to pick our resolution

		int width, height;

		//Query the actual framebuffer size from GLFW
		//GLFW window sizes are in screen coorinates, but Vulkan requires the swap chain in pixels
		//This funcion retrives the size in pixels, accounting for high-DPI displays (like Apple's Retina display)
		glfwGetFramebufferSize(window, &width, &height);

		//Create a VkExtent2D struct and set its width & height based on the framebuffer size
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};
		
		//Clamp the width and height so they stay within Vulkan's allowed min/max image extent range
		//This ensures that we do not select a resolution that is unsupported
		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

std::vector<char> VKApplication::readFile(const std::string& filename){
	//ate: Start reading at end of the file (so that we can use the read postion to deremine the size of the file). Opens the file and moves the read position to the end immediately
	//binary: read the file as a binary file (avoid text transformation)

	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file!");
	}

	//Initialize buffer with the size of the file
	size_t fileSize = (size_t) file.tellg();//Return the current postion of the read pointer which is at the end of the file
	std::vector<char> buffer(fileSize);
	
	//Seek back to the beginning of the file and read all of the bytes at once:
	file.seekg(0); //Moves the read position back to the beginning of the file.
	file.read(buffer.data(), fileSize);// Reads fileSize bytes into the buffer.
	file.close();

	return buffer;
}

VkShaderModule VKApplication::createShaderModule(const std::vector<char>& code) const
{
	//Struct that provide info for creating shader module
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //reinterpret_cast<const uint32_t*> is used to reinterpret std::vector<char> data as uint32_t* because Vulkan expects the shader in 32-bit words (fixed-size unit of data SPIR-V defines a word as 32 bits (4 bytes)).

	//Create Shader Module
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
}
