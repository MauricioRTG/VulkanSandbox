#include "VKApplication.h"
#include <set>
#include <algorithm>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
//Load an image library
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

	window = glfwCreateWindow(WIDTH, HEIGHT, "VulkanSandbox Window", nullptr, nullptr);
	// GLFW does not know how to properly call a member function with the right this pointer to our VKApplication instance.
	//We have to set it mannually
	glfwSetWindowUserPointer(window, this);
	//Detect window resizes
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void VKApplication::initVulkan() {
	createInstance();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createTextureImage();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}

void VKApplication::mainLoop() {
	//Check for events like pressing the X button until the window has been closed by the user
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		drawFrame();
	}

	//Wait for the logical device to finish operations before exiting mainLoop and destroying the window
	//Becuase: drawing and presentation operations may still be going on. Cleaning up resources while that is happening is a bad idea.
	vkDeviceWaitIdle(logicalDevice);
}

void VKApplication::cleanup() {
	cleanupSwapChain();

	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	vkDestroyImage(logicalDevice, textureImage, nullptr);
	vkFreeMemory(logicalDevice, textureImageMemory, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);

	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

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

void VKApplication::createRenderPass(){
	//Attachment Description
	//In this case a single color buffer attachment represented by on of the images from the swp chain
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; //Not mulitsampling so just 1 sample
	//loadOp and storeOp determine what to do with the data in the attachment before rendering and after rendering. 
	//For loadOp:
	//VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment
	//VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start
	//VK_ATTACHMENT_LOAD_OP_DONT_CARE : Existing contents are undefined; we don't care about them
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	//For storeOp:
	//VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
	//VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering operation
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; //We're interested in seeing the rendered triangle on the screen, so we're going with the store operation
	//Our application won't do anything with the stencil buffer, so the results of loading and storing are irrelevant.
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//Textures and framebuffers in Vulkan are represented by VkImage objects with a certain pixel format, however the layout of the pixels in memory can change based on what you're trying to do with an image.
	//Some of the most common layouts are:
	//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Images used as color attachment
	//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Images to be presented in the swap chain
	//VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : Images to be used as destination for a memory copy operation
	//InitialLayout: specifies which layout the image will have before the render pass begins.
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //we don't care what previous layout the image was in. The caveat of this special value is that the contents of the image are not guaranteed to be preserved, but that doesn't matter since we're going to clear it anyway.
	//Finallayout: specifies the layout to automatically transition to when the render pass finishes.
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //We want the image to be ready for presentation using the swap chain after rendering

	//Subpasses and attachment references
	
	//A single redener pass can consist of multiple subpasses.
	//Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, for example a sequence of post-processing effects that are applied one after another.
	//If you group these rendering operations into one render pass, then Vulkan is able to reorder the operations and conserve memory bandwidth for possibly better performance.

	//Attachment references
	//Every subpass references one or more of the attachments that we've described using the structure in the previous sections. These references are themselves VkAttachmentReference structs that look like this:
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0; //specifies which attachment to reference by its index in the attachment descriptions array.
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //specifies which layout we would like the attachment to have during a subpass that uses this reference. Vulkan will automatically transition the attachment to this layout when the subpass is started. We intend to use the attachment to function as a color buffer 

	//Subpass description
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;//Vulkan may also support compute subpasses in the future, so we have to be explicit about this being a graphics subpass.
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; //The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!
	//The following other types of attachments can be referenced by a subpass:
	//pInputAttachments: Attachments that are read from a shader
	//pResolveAttachments: Attachments used for multisampling color attachments
	//pDepthStencilAttachment: Attachment for depth and stencil data
	//pPreserveAttachments: Attachments that are not used by this subpass, but for which the data must be preserved

	// Subpass dependencies

	//Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies, which specify memory and execution dependencies between subpasses

	// Why Do We Need an Explicit Subpass Dependency?
	// - The default implicit dependency assumes that the color attachment image is available at the very start of the pipeline. However, this is not always true because the swap chain may still be using it (e.g., for presenting the previous frame).
	// - The render pass needs to wait for the swap chain to release the image before it can start using it as a color attachment.
	// - By default, Vulkan does not enforce this waiting correctly, so we have to manually synchronize it with a subpass dependency. Vulkan doesn't automatically wait for the swap chain image to become available before the render pass starts.
	
	//We define a VkSubpassDependency struct to create an explicit dependency between the previous implicit stage (VK_SUBPASS_EXTERNAL) and the color attachment output stage (subpass 0 in our case).
	//A subpass dependency ensures that writing to the color attachment only happens after the swap chain releases the image.
	//This avoids race conditions and ensures proper image layout transitions.
	VkSubpassDependency dependency{};
	//The first two fields specify the indices of the dependency and the dependent subpass.
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL; //This refers to operations happening before the render pass starts (e.g., image acquisition by the swap chain).
	dependency.dstSubpass = 0; // This refers to the first (and only) subpass, where the color attachment is written.
	//The next two fields specify the operations to wait on and the stages in which these operations occur
	//We need to wait for the swap chain to finish reading from the image before we can access it.
	//This can be accomplished by waiting on the color attachment output stage itself.
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //This ensures the render pass waits until the swap chain finishes reading from the image.
	dependency.srcAccessMask = 0; // No memory access needed before this
	//The operations that should wait on this are in the color attachment stage and involve the writing of the color attachment. 
	//These settings will prevent the transition from happening until it's actually necessary (and allowed): when we want to start writing colors to it.
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; //The color attachment write operations will only happen after the image is available.
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Ensures we can write safely

	//Render Pass

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass!");
	}
}

void VKApplication::createDescriptorSetLayout(){
	//Provide details about every descriptor binding used in the shaders for pipeline creation, just like we had to do for every vertex attribute and its location index

	// Descriptor Set Layout Bindings 

	//Binding for the model, view, proj uniform variable in shader
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;// It is possible for the shader variable to represent an array of uniform buffer objects, and descriptorCount specifies the number of values in the array.
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//Create Info for layout
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;//Number of bindings
	layoutInfo.pBindings = &uboLayoutBinding;

	if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor set layout!");
	}
}

void VKApplication::createGraphicsPipeline(){
	//Read shader SPIR-V Files
	auto vertShaderCode = readFile("shaders/vert.spv");
	auto fragShaderCode = readFile("shaders/frag.spv");

	//Create Shader modules
	VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
	VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

	//Shader stage creation

	//Vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main"; //Entrypoint of shader

	//Fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	//Vertex Input
	//Describes the format of the vertex data that will be passed to the vertex shader
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescription = Vertex::getAttributeDescription();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

	//Input Assembly
	//Describes what kind o feometry will be drawn form the vertices (points, lines, or triangles), and the topology type
	//VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
	//VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without reuse
	//VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : the end vertex of every line is used as start vertex for the next line
	//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : triangle from every 3 vertices without reuse
	//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : the second and third vertex of every triangle are used as first two vertices of the next triangle

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE; //If you set the primitiveRestartEnable member to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF in the vertex buffer.

	//Viewports and Sissors
	//Viewports (transformation) define the transformation from the image to the framebuffer
	//Sissor (filter) rectangles define in which regions pixels will actually be stored, any pixels outside the siccor rectangles will be discarded by rasterizer
	
	//Scissor that covers entire framebuffer (only needed to specify when we don use dynamic states)
	//VkRect2D scissor{};
	//scissor.offset = { 0, 0 };
	//scissor.extent = swapChainExtent;

	//Enable Dynamic states for scissor and viewport
	//Viewport(s) and scissor rectangle(s) can either be specified as a static part of the pipeline or as a dynamic state set in the command buffer. 
	
	//Viewport
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	//Rasterizer
	//Takes the geometry that is shaped by hte vertices from the vertex shader and turns it into fragments
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; //If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them. This is useful in some special cases like shadow maps.
	rasterizer.rasterizerDiscardEnable = VK_FALSE; //If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	//The polygonMode determines how fragments are generated for geometry. The following modes are available:
	//VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
	//VK_POLYGON_MODE_LINE : polygon edges are drawn as lines
	//VK_POLYGON_MODE_POINT : polygon vertices are drawn as points
	rasterizer.lineWidth = 1.0f;//escribes the thickness of lines in terms of number of fragments. The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
	//because of the Y-flip we did in the projection matrix, the vertices are now being drawn in counter-clockwise order instead of clockwise order. This causes backface culling to kick in and prevents any geometry from being drawn.
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;// The cullMode variable determines the type of face culling to use. You can disable culling, cull the front faces, cull the back faces or both.
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;// The frontFace variable specifies the vertex order for faces to be considered front-facing and can be clockwise or counterclockwise.
	rasterizer.depthBiasEnable = VK_FALSE;
	//The rasterizer can alter the depth values by adding a constant value or biasing them based on a fragment's slope. 
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	//Multisampling
	//One of the ways to perform anti-aliasing. It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel. This mainly occurs along edges, which is also where the most noticeable aliasing artifacts occur.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	//Color blending
	//After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer. This transformation is known as color blending and there are two ways to do it:
	//		Mix the old and new value to produce a final color
	//		Combine the old and new value using a bitwise operation

	//Color blend attachment: contains the configuration per attached framebuffer and the second

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	//Pseudocode that represent how previous struct uses the paramenters to calculate the blend
	/*if (blendEnable) {
		finalColor.rgb = (srcColorBlendFactor * newColor.rgb) < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
		finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp > (dstAlphaBlendFactor * oldColor.a);
	}
	else {
		finalColor = newColor;
	}

	finalColor = finalColor & colorWriteMask;
	
	//The parameters we passed are using alpha blending, where we want the new color to be blended with the old color based on opacity

	finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
	finalColor.a = newAlpha.a;
	*/

	//Color blending State: contains the global color blending settings. In our case we only have one framebuffer

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE; //If you want to use the second method of blending (bitwise combination), then you should set logicOpEnable to VK_TRUE
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // The bitwise operation can then be specified in the logicOp field. Note that this will automatically disable the first method, as if you had set blendEnable to VK_FALSE
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	//Source and destination values are combined according to the blend operation, quadruplets of source and destination weighting factors determined by the blend factors, and a blend constant, to obtain a new set of R, G, B, and A values
	colorBlending.blendConstants[0] = 0.0f; // Optional Rc
	colorBlending.blendConstants[1] = 0.0f; // Optional Gc
	colorBlending.blendConstants[2] = 0.0f; // Optional Bc
	colorBlending.blendConstants[3] = 0.0f; // Optional Ac

	//Dynamic States: scissor and viewport
	//While most of the pipeline state needs to be baked into the pipeline state, a limited amount of the state can actually be changed without recreating the pipeline at draw time
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	//Pipeline Layout creation
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	//Descriptor sets are used to bind resources(textures, buffers, etc.) to shaders
	pipelineLayoutInfo.setLayoutCount = 1; //means that no descriptor sets are used in this pipeline. 
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;//descriptor set layouts.
	//Push constants are small amounts of data that can be passed directly to shaders.
	pipelineLayoutInfo.pushConstantRangeCount = 0; //no push constants are used
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout!");
	}

	//Create Graphics Pipeline using:
	//Shader stages: the shader modules that define the functionality of the programmable stages of the graphics pipeline
	//Fixed-function state: all of the structures that define the fixed-function stages of the pipeline, like input assembly, rasterizer, viewport and color blending
	//Pipeline layout: the uniform and push values referenced by the shader that can be updated at draw time
	//Render pass: the attachments referenced by the pipeline stages and their usage

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	//Shader stages
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2; //Vertex shader and fragment shader
	pipelineInfo.pStages = shaderStages;
	//Fixed-functions states
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	//Pipeline layout
	pipelineInfo.layout = pipelineLayout;
	//Render pass
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;// index of the sub pass where this graphics pipeline will be used.
	//Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker. Using either the handle of an exisiting pipeline or reference another pipeline that is about to be created by index with 
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline!");
	}

	//Destroy shader modules
	vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
}

void VKApplication::createFramebuffers(){
	//The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object. A framebuffer object references all of the VkImageView objects that represent the attachments
	//However, the image that we have to use for the attachment depends on which image the swap chain returns when we retrieve one for presentation. That means that we have to create a framebuffer for all of the images in the swap chain and use the one that corresponds to the retrieved image at drawing time.

	swapChainFramebuffers.resize(swapChainImageViews.size());

	//Iterate thorugh the image views and create framebuffers
	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		VkImageView attachments[] = {
			swapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass; //You can only use a framebuffer with the render passes that it is compatible with, which roughly means that they use the same number and type of attachments.
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments; //specify the VkImageView objects that should be bound to the respective attachment descriptions in the render pass pAttachment array
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1; //refers to the number of layers in image arrays. Our swap chain images are single images, so the number of layers is 1

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer!");
		}
	}
}

void VKApplication::createCommandPool(){
	QueueFamilyIndices queueFamiliyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	//There are two possible flags for command pools:
	//	VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often (may change memory allocation behavior)
	//	VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //We will be recording a command buffer every frame, so we want to be able to reset and rerecord over it
	//Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues we retrieved.
	//Each command pool can only allocate command buffers that are submitted on a single type of queue. 
	//We're going to record commands for drawing, which is why we've chosen the graphics queue family.
	poolInfo.queueFamilyIndex = queueFamiliyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool!");
	}
}

void VKApplication::createTextureImage(){
	//Load an image and upload it into a Vulkan image object.

	// Load image
	int texWidth, texHeight, texChannels;
	//The STBI_rgb_alpha value forces the image to be loaded with an alpha channel, even if it doesn't have one
	stbi_uc* pixels = stbi_load("textures/lion-1.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	//The pixels are laid out row by row with 4 bytes per pixel in the case of STBI_rgb_alpha for a total of texWidth * texHeight * 4 values.
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("Failed to load texture image!");
	}

	// Staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	//The buffer should be in host visible memory so that we can map it and it should be usable as a transfer source so that we can copy it to an image later on:
	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	// Copy the pixel values that we got from the image loading library to the buffer:
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	//clean up the original pixel array
	stbi_image_free(pixels);

	//Create Image
	createImage(
		texWidth, 
		texHeight, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		textureImage, 
		textureImageMemory);

	//Copy the staging buffer to the texture image. Steps:
	//- Transition the texture image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	//- Execute the buffer to image copy operation

	//The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage. Remember that we can do this because we don't care about its contents before performing the copy operation.
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	//To be able to start sampling from the texture image in the shader, we need one last transition to prepare it for shader access:
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void VKApplication::createVertexBuffer(){

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	// Create the staging Buffer (Host-Visible Memory in RAM)
	//A staging buffer allows you to upload data in a single batch and then efficiently transfer it to device-local memory (VRAM in GPU), minimizing PCIe traffic.
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// Filling the staging buffer

	//It is now time to copy the vertex data to the buffer. This is done by mapping the buffer memory into CPU accessible memory with vkMapMemory.
	//This function allows us to access a region of the specified memory resource defined by an offset and size.
	//It is also possible to specify the special value VK_WHOLE_SIZE to map all of the memory.
	void* data; //CPU accessible memory (Host-visible memory in RAM)
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);

	//You can now simply memcpy the vertex data to the mapped memory
	memcpy(data, vertices.data(), (size_t)bufferSize);

	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	//Unfortunately the driver may not immediately copy the data into the buffer memory, for example because of caching. It is also possible that writes to the buffer are not visible in the mapped memory yet. 
	// - Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (we used this when finding a memory type)
	// - Call vkFlushMappedMemoryRanges after writing to the mapped memory, and call vkInvalidateMappedMemoryRanges before reading from the mapped memory

	//Flushing memory ranges or using a coherent memory heap means that the driver will be aware of our writes to the buffer, but it doesn't mean that they are actually visible on the GPU yet.
	//The transfer of data to the GPU is an operation that happens in the background and the specification simply tells us that it is guaranteed to be complete as of the next call to vkQueueSubmit.

	// Create vertex buffer using device local memory

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);


	//Copy Staging buffer [Host-Visible] content to Vertex buffer [device local]
	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	//After copying the data from the staging buffer to the device buffer, we clean up staging buffer and staging  buffer memory
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void VKApplication::createIndexBuffer(){

	//Same as creating a vertex buffer
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	//Create staging buffer (Host-visible) copy indices array into
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	//Copy indices array content into stagin buffer
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	//Create index buffer, destination where we transfer the content of the staging buffer
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	//Copy content from staging buffer (Host-visible in RAM) to indexBuffer in (device local VRAM in GPU)
	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	//Cleanup temp staging buffer used for transfer
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void VKApplication::createUniformBuffers(){
	//We're going to copy new data to the uniform buffer every frame, so it doesn't really make any sense to have a staging buffer. It would just add extra overhead .

	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	//We should have multiple buffers, because multiple frames may be in flight at the same time and we don't want to update the buffer in preparation of the next frame while a previous one is still reading from it!
	//We need to have as many uniform buffers as we have frames in flight, and write to a uniform buffer that is not currently being read by the GPU
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		//Create buffer
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);

		//Map pointer to access in CPU the uniform buffer memory 
		//We map the buffer right after creation using vkMapMemory to get a pointer to which we can write the data later on. 
		//The buffer stays mapped to this pointer for the application's whole lifetime. This technique is called "persistent mapping" and works on all Vulkan implementations.
		//Not having to map the buffer every time we need to update it increases performances, as mapping is not free.
		vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);

		//Instead of manually allocating memory for uniform buffers, many implementations use Vulkan descriptor sets to manage uniform buffer memory implicitly.
		//vkAllocateDescriptorSets() and vkUpdateDescriptorSets() handle memory assignment and binding
	}
}

void VKApplication::createDescriptorPool(){
	//Descriptor sets can't be created directly, they must be allocated from a pool like command buffers

	// Describe which descriptor types our descriptor sets are going to contain and how many of them
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);// We will allocate one of these descriptors for every frame.
	
	//Create info
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	//Aside from the maximum number of individual descriptors that are available, we also need to specify the maximum number of descriptor sets that may be allocated:
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	// Create Descriptor Pool
	if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

void VKApplication::createDescriptorSets(){
	// Create Info
	//You need to specify the descriptor pool to allocate from
	//The number of descriptor sets to allocate
	//The descriptor layout to base them on
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); //In our case we will create one descriptor set for each frame in flight, all with the same layout.
	allocInfo.pSetLayouts = layouts.data();

	//Allocate memory for descriptor set

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	//Note: You don't need to explicitly clean up descriptor sets, because they will be automatically freed when the descriptor pool is destroyed.

	//The descriptor sets have been allocated now, but the descriptors within still need to be configured. We'll now add a loop to populate every descriptor
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		// This structure specifies the buffer and the region within it that contains the data for the descriptor
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i]; //VkBuffer we want to bind to descriptor set
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		//The configuration of descriptors is updated using the vkUpdateDescriptorSets function, which takes an array of VkWriteDescriptorSet structs as parameter.
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0; //We gave our uniform buffer binding index 0
		descriptorWrite.dstArrayElement = 0;//Remember that descriptors can be arrays, so we also need to specify the first index in the array that we want to update. We're not using an array, so the index is simply 0.
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1; //The descriptorCount field specifies how many array elements you want to update.
		descriptorWrite.pBufferInfo = &bufferInfo; //Our descriptor is based on buffers, so we're using pBufferInfo.

		//t accepts two kinds of arrays as parameters: an array of VkWriteDescriptorSet and an array of VkCopyDescriptorSet. The latter can be used to copy descriptors to each other, as its name implies.
		vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
	}
}

void VKApplication::createCommandBuffers(){

	//Resize command buffers array to the desired in flight frames
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	//Specifies the command pool and number of buffers to allocate:
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	//The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
	//- VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
	//- VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

	if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate command buffers!");
	}
}

void VKApplication::createSyncObjects(){
	//Resize syncronization vectors for desired in flight frames
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	//inFlightFence is only signaled after a frame has finished rendering, yet since this is the first frame, there are no previous frames in which to signal the fence! Thus vkWaitForFences() blocks indefinitely, waiting on something which will never happen.
	//reate the fence in the signaled state, so that the first call to vkWaitForFences() returns immediately since the fence is already signaled.
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		//Create semaphores and fence
		if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
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
		std::cout << "Failed to open file!" << std::endl;
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

VkShaderModule VKApplication::createShaderModule(const std::vector<char>& code) const{
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

void VKApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex){
	// Begin Command buffer recording

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//The flags parameter specifies how we're going to use the command buffer:
	//	VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
	//	VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
	//	VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution.
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr; //Only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to being recoring command buffer!");
	}

	// Starting render pass

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	// We created a framebuffer for each swap chain image where it is specified as a color attachment.
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex]; // we need to bind the framebuffer for the swapchain image we want to draw to. Using the imageIndex parameter which was passed in, we can pick the right framebuffer for the current swapchain image.
	//Define the size fo the render area
	//he render area defines where shader loads and stores will take place. 
	// The pixels outside this region will have undefined values. It should match the size of the attachments for best performance.
	renderPassInfo.renderArea.offset = { 0, 0 }; 
	renderPassInfo.renderArea.extent = swapChainExtent;
	// define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used as load operation for the color attachment.
	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} }; //Black with 100% opacity.
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	// Command to begin render pass
	
	//VkSubpassContents controls how the drawing commands within the render pass will be provided:
	//	VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
	//	VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Bind Pipeline
	// controls how the drawing commands within the render pass will be provided.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	//With this we have told Vulkan which operations to execute in the graphics pipeline and which attachment to use in the fagment shader

	//Set viewport and scissior
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Bind vertex buffer to command buffer
	VkBuffer vertexBuffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	// Bind index buffer to command buffer
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

	//Bind the right descriptor set for each frame to the descriptors in the shader
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

	//Draw Indexed command
	//vertexCount: size of vertexBuffer
	//instanceCount: Used for instanced rendering, use 1 if you're not doing that.
	//firstIndex : Used as an offset into the index buffer, defines the lowest value of gl_VertexIndex.
	//vertexOffset :offset to add to the indices in the index buffer.
	//firstInstance : Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	// End render pass

	vkCmdEndRenderPass(commandBuffer);

	//End Command buffer

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record command buffer!");
	}
}

void VKApplication::drawFrame(){
	// We were required to wait on the previous frame to finish before we can start submitting the next which results in unnecessary idling of the host.
	//The way to fix this is to allow multiple frames to be in-flight at once, that is to say, allow the rendering of one frame to not interfere with the recording of the next. 
	//How do we do this? Any resource that is accessed and modified during rendering must be duplicated (command buffers, semaphores, and fences)
	// Benefints: The CPU can continue recording and submitting new frames without waiting for the GPU to finish executing previous ones.
	// Does It Matter If the CPU Submits a New Frame Before the Previous One Finishes? No, it doesn’t matter, because:
	// - When the CPU submits a command buffer using vkQueueSubmit(), it doesn’t execute immediately. Instead, it gets queued in the GPU's command queue (FIFO).
	// - Even if the CPU submits a new frame's command buffer quickly, the GPU won’t execute it until previous submissions are finished
	// - Synchronization objects (semaphores, fences) control execution order and prevent resource conflicts.
	//What this allows is to reduce the time the CPU is idle by briging some of the work foward to the GPU, previously we had to wait for the GPU to finish excuting before starting submitting a new command buffer to the queue 
	
	//Takes an array of fences and waits on the host for either any or all of the fences to be signaled before returning. 
	// - The VK_TRUE we pass here indicates that we want to wait for all fences, but in the case of a single one it doesn't matter.
	// - This function also has a timeout parameter that we set to the maximum value of a 64 bit unsigned integer, UINT64_MAX, which effectively disables the timeout.
	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	// Acquiring an image for the swap chain

	uint32_t imageIndex;
	//The third parameter specifies a timeout in nanoseconds for an image to become available. Using the maximum value of a 64 bit unsigned integer means we effectively disable the timeout.
	// The next two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image. That's the point in time where we can start drawing to it. 
	//The last parameter specifies a variable to output the index of the swap chain image that has become available.
	//The index refers to the VkImage in our swapChainImages array. We're going to use that index to pick the VkFrameBuffer.
	VkResult result = vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	//Recreate Swap chain if current is no longer compatible
	// VK_ERROR_OUT_OF_DATE_KHR: The swap chain has become incompatible with the surface and can no longer be used for rendering. Usually happens after a window resize.
	// VK_SUBOPTIMAL_KHR: The swap chain can still be used to successfully present to the surface, but the surface properties are no longer matched exactly.
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain(); // recreate the swap chain and try again in the next drawFrame
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swap chain image!");
	}

	//Generate a new transformation every frame to make the geometry spin around
	updateUniformBuffer(currentFrame);

	//After waiting, we need to manually reset the fence to the unsignaled state
	//Delay resetting the fence until after we know for sure we will be submitting work with it. Thus, if we return early, the fence is still signaled and vkWaitForFences wont deadlock the next time we use the same fence object.
	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

	// Recording the command buffer

	//Makes sure the command buffer is able to be recorded
	//The second parameter of vkResetCommandBuffer is a VkCommandBufferResetFlagBits flag. Since we don't want to do anything special, we leave it as 0.
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	
	//Record commands to command buffer
	recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

	// Submitting the command buffer in queue
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	//The first three parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait. 
	//We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline that writes to the color attachment. 
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	//The next two parameters specify which command buffers to actually submit for execution.
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
	// The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the command buffer(s) have finished execution. 
	//In our case we're using the renderFinishedSemaphore for that purpose.
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	//Submit the command buffer to the graphics queue using vkQueueSubmit
	//The last parameter references an optional fence that will be signaled when the command buffers finish execution.
	// This allows us to know when it is safe for the command buffer to be reused
	// Now on the next frame, the CPU will wait for this command buffer to finish executing before it records new commands into it.
	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	// Presentation

	//The last step of drawing a frame is submitting the result back to the swap chain to have it eventually show up on the screen.
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	//The first two parameters specify which semaphores to wait on before presentation can happen
	//Since we want to wait on the command buffer to finish execution, thus our triangle being drawn, we take the semaphores which will be signalled and wait on them
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	//The next two parameters specify the swap chains to present images to and the index of the image for each swap chain. 
	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	// It allows you to specify an array of VkResult values to check for every individual swap chain if presentation was successful. It's not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.
	presentInfo.pResults = nullptr; // Optional

	//Submit the request to present an image to the swap chain
	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	//Recreate Swap chain if current is no longer compatible
	//It is important to check framebufferResized this after vkQueuePresentKHR to ensure that the semaphores are in a consistent state, otherwise a signaled semaphore may never be properly waited upon.
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
		framebufferResized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	//Advance to the next frame every time
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; //By using the modulo (%) operator, we ensure that the frame index loops around after every MAX_FRAMES_IN_FLIGHT enqueued frames.
}

void VKApplication::recreateSwapChain(){
	//Handling minimization
	//This case is special because it will result in a frame buffer size of 0. 
	// We will handle that by pausing until the window is in the foreground again by extending the recreateSwapChain function
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}
	
	//We first call vkDeviceWaitIdle, because we shouldn't touch resources that may still be in use
	vkDeviceWaitIdle(logicalDevice);

	//Clean old swapchain
	cleanupSwapChain();

	createSwapChain();
	createImageViews();//The image views need to be recreated because they are based directly on the swap chain images
	createFramebuffers();//the framebuffers directly depend on the swap chain images
}

void VKApplication::cleanupSwapChain(){
	for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(logicalDevice, swapChainFramebuffers[i], nullptr);
	}

	for (size_t i = 0; i < swapChainImageViews.size(); i++) {
		vkDestroyImageView(logicalDevice, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
}

void VKApplication::framebufferResizeCallback(GLFWwindow* window, int width, int height){
	//Get pointer for vulkan application instance
	auto app = reinterpret_cast<VKApplication*>(glfwGetWindowUserPointer(window));
	//Set flag
	app->framebufferResized = true;
}

uint32_t VKApplication::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const{
	//Query info about the available types of memory in physical device
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	//Find memory type that is suitable for the buffer 
	//We can find the index of a suitable memory type by simply iterating over them and checking if the corresponding bit is set to 1.
	//The properties define special features of the memory, like being able to map it so we can write to it from the CPU. 
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		//Check for memory type support and if the memoery type support the desired properties
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	
	throw std::runtime_error("Failed to find suitable memory type!");
}

void VKApplication::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory){
	// Create vertex Buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;//Specifies the size of the buffer in bytes
	bufferInfo.usage = usage;//Indicates for which purposes the data in the buffer is going to be used. It is possible to specify multiple purposes using a bitwise or.
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//Just like the images in the swap chain, buffers can also be owned by a specific queue family or be shared between multiple at the same time. The buffer will only be used from the graphics queue, so we can stick to exclusive access.

	if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	//Buffer has been created, but it doesn't actually have any memory assigned to it yet. First step of allocating memory:

	// Query Memory Requirements

	//The VkMemoryRequirements struct has three fields:
	//size: The size of the required amount of memory in bytes, may differ from bufferInfo.size.
	//alignment: The offset in bytes where the buffer begins in the allocated region of memory, depends on bufferInfo.usage and bufferInfo.flags.
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

	// Allocate Vertex Buffer Memory

	//Graphics cards can offer different types of memory to allocate from. Each type of memory varies in terms of allowed operations and performance characteristics. We need to combine the requirements of the buffer and our own application requirements to find the right type of memory to use
	//Memory allocation is now as simple as specifying the size and type, both of which are derived from the memory requirements of the vertex buffer and the desired property.
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	// Associate this memory with the buffer
	//Since this memory is allocated specifically for this the vertex buffer, the offset is simply 0. If the offset is non-zero, then it is required to be divisible by memRequirements.alignment.
	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

void VKApplication::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size){
	// Allocate temporary commadn buffer to execute memory transfer operations
	
	//TODO Optimization: You may wish to create a separate command pool for these kinds of short-lived buffers, because the implementation may be able to apply memory allocation optimizations. You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool generation in that case.

	//Begin command buffer recording
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	// Transfer content of src buffer to dst buffer command

	//It is not possible to specify VK_WHOLE_SIZE here, unlike the vkMapMemory command.
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	//End command buffer recording
	endSingleTimeCommands(commandBuffer);
}

void VKApplication::updateUniformBuffer(uint32_t currentImage){
	// This make sure that the geometry rotates 90 degrees per second regardless of frame rate
	static auto startTime = std::chrono::high_resolution_clock::now(); //It remains the same across multiple function calls due to the static keyword
	
	auto currentTime = std::chrono::high_resolution_clock::now();
	//currentTime - startTime:  produces a duration object representing the time difference.
	//std::chrono::duration<float, std::chrono::seconds::period>: converts the duration into seconds as a float.
	//.count(): extracts the actual numerical value.
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	//Update model view and proj
	UniformBufferObject ubo{};

	//The model rotation will be a simple rotation around the Z-axis using the time variable:

	// Model: Transform object coordinates from local to world space
	//The glm::rotate function takes an existing transformation, rotation angle and rotation axis as parameters.
	//The glm::mat4(1.0f) constructor returns an identity matrix. 
	//Using a rotation angle of time * glm::radians(90.0f) accomplishes the purpose of rotation 90 degrees per second.
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	
	// View: from world space to view space (camera view)
	//View/Camera looks at at the geometry from above at a 45 degree angle
	// First parameter: positoin of the camera in world space
	// Second parameter: Target position, the positon the camera is looking (in this case the world origin)
	// Third parameter: Up vector, defines which direction is condered "up" in the world. In this case, (0,0,1) means the positive Z-axis is up.
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	// Projection: from view space to clip-space
	//The projection matrix then converts coordinates within this specified range to normalized device coordinates (-1.0, 1.0) (not directly, a step called Perspective Division sits in between). 
	//All coordinates outside this range will not be mapped between -1.0 and 1.0 and therefore be clipped.
	//The projection matrix to transform view coordinates to clip coordinates usually takes two different forms, where each form defines its own unique frustum:
	// - Orthographic: defines a cube-like frustum box that defines the clipping space where each vertex outside this box is clipped
	// - Perspective: objects that are farther away appear much smaller
	//First parameter: defines the fov value, that stands for field of view and sets how large the viewspace is (usually set to 45 degrees)
	//Second parameter: sets the aspect ratio which is calculated by dividing the swapChainExtent's width by its height.
	//Third and fourth parameter: sets the near and far plane of the frustum (All the vertices between the near and far plane and inside the frustum will be rendered0
	//We usually set the near distance to 0.1 and the far distance to 100.0. 
	//The rectangle has changed into a square because the projection matrix now corrects for aspect ratio takes care of screen resizing.
	ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

	//GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted. The easiest way to compensate for that is to flip the sign on the scaling factor of the Y axis in the projection matrix.
	ubo.proj[1][1] = ubo.proj[1][1] * -1;

	//Copy the data in the uniform buffer object to the current uniform buffer.
	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void VKApplication::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory){
	//Create Info for Image we are going to feel with data from the staging buffer
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	//Image type, ells Vulkan with what kind of coordinate system the texels in the image are going to be addressed. It is possible to create 1D, 2D and 3D images.
	//1D: One dimensional images can be used to store an array of data or gradient
	//2D: mainly used for textures
	//3D: can be used to store voxel volumes
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	//The extent field specifies the dimensions of the image, basically how many texels there are on each axis
	//That's why depth must be 1 instead of 0
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1; //Means there is no depth component 
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;//use the same format for the texels as the pixels in the buffer
	//VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
	//VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
	//Unlike the layout of an image, the tiling mode cannot be changed at a later time. If you want to be able to directly access texels in the memory of the image, then you must use VK_IMAGE_TILING_LINEAR. We will be using a staging buffer instead of a staging image, so this won't be necessary. We will be using VK_IMAGE_TILING_OPTIMAL for efficient access from the shader.
	imageInfo.tiling = tiling;
	//VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
	//VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.
	//There are few situations where it is necessary for the texels to be preserved during the first transition. One example, however, would be if you wanted to use an image as a staging image in combination with the VK_IMAGE_TILING_LINEAR layout. In that case, you'd want to upload the texel data to it and then transition the image to be a transfer source without losing the data.
	//In our case, however, we're first going to transition the image to be a transfer destination and then copy texel data to it from a buffer object, so we don't need this property and can safely use VK_IMAGE_LAYOUT_UNDEFINED.
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination
	//We also want to be able to access the image from the shader to color our mesh, so the usage should include VK_IMAGE_USAGE_SAMPLED_BIT
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
	//The samples flag is related to multisampling. This is only relevant for images that will be used as attachments
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // Optional: Sparse images are images where only certain regions are actually backed by memory. If you were using a 3D texture for a voxel terrain, for example, then you could use this to avoid allocating memory to store large volumes of "air" values. 

	//Create Image
	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create image!");
	}

	//Get memory requrments for image
	//Allocating memory for an image works in exactly the same way as allocating memory for a buffer. Use vkGetImageMemoryRequirements instead of vkGetBufferMemoryRequirements
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

	//Allocation info
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	//Allocate memory for image
	if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate image memory!");
	}

	//Bind image memory with textureImage
	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

VkCommandBuffer VKApplication::beginSingleTimeCommands(){
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	// Start Recording buffer

	//We're only going to use the command buffer once and wait with returning from the function until the copy operation has finished executing
	// It's good practice to tell the driver about our intent using VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT.
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VKApplication::endSingleTimeCommands(VkCommandBuffer commandBuffer){
	//End Recording Buffer
	vkEndCommandBuffer(commandBuffer);

	// Submit command buffer for exceution
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// The buffer copy command requires a queue family that supports transfer operations, which is indicated using VK_QUEUE_TRANSFER_BIT
	//The good news is that any queue family with VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT capabilities already implicitly support VK_QUEUE_TRANSFER_BIT operations.
	//he implementation is not required to explicitly list it in queueFlags in those cases.
	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	//Unlike the draw commands, there are no events we need to wait on this time. We just want to execute the transfer on the buffers immediately. 
	// There are again two possible ways to wait on this transfer to complete:
	// - We could use a fence and wait with vkWaitForFences (A fence would allow you to schedule multiple transfers simultaneously and wait for all of them complete, instead of executing one at a time.)
	// - Simply wait for the transfer queue to become idle with vkQueueWaitIdle
	vkQueueWaitIdle(graphicsQueue);

	// Clean up the command buffer used for the transfer operation.
	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void VKApplication::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout){
	// Begin command buffer recording
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	// One of the most common ways to perform layout transitions is using an image memory barrier.
	//A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write to a buffer completes before reading from it, but it can also be used to transition image layouts and transfer queue family ownership 
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout; //It is possible to use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout if you don't care about the existing contents of the image.
	barrier.newLayout = newLayout;
	//If you are using the barrier to transfer queue family ownership
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; //Ignore
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	//The image and subresourceRange specify the image that is affected and the specific part of the image.
	//Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	//Barriers are primarily used for synchronization purposes, so you must specify which types of operations that involve the resource must happen before the barrier, and which operations that involve the resource must wait on the barrier
	//We need to do that despite already using vkQueueWaitIdle to manually synchronize
	//There are two transitions we need to handle:
	//- Undefined to transfer destination: transfer writes that don't need to wait on anything
	//- Transfer destination to shader reading: shader reads should wait on transfer writes, specifically the shader reads in the fragment shader, because that's where we're going to use the texture
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0; //Undefined doesn't matter (Don't need to wait on anything). set srcAccessMask to 0 if you ever needed a VK_ACCESS_HOST_WRITE_BIT dependency in a layout transition. Command buffer submission results in implicit VK_ACCESS_HOST_WRITE_BIT synchronization at the beginning. 
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;// Since the writes don't have to wait on anything, you may specify an empty access mask and the earliest possible pipeline stage VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;//transfer writes must occur in the pipeline transfer stage (is not a real stage within the graphics and compute pipelines. It is more of a pseudo-stage where transfers happen)
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		//The image will be written in the same pipeline stage (transfer stage) and subsequently read by the fragment shader, which is why we specify shader reading access in the fragment shader pipeline stage.
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; //Wait for transfer write
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Read in shader texture image, after waiting fo the transfer write operation to complete

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; 
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	//All types of pipeline barriers are submitted using the same function
	//The first parameter after the command buffer specifies in which pipeline stage the operations occur that should happen before the barrier.
	//The second parameter specifies the pipeline stage in which operations will wait on the barrier. 
	//    The pipeline stages that you are allowed to specify before and after the barrier depend on how you use the resource before and after the barrier. 
	//    if you're going to read from a uniform after the barrier: VK_ACCESS_UNIFORM_READ_BIT
	//    and the earliest shader that will read from the uniform as pipeline stage: VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT 
	//    It would not make sense to specify a non-shader pipeline stage for this type of usage and the validation layers will warn you when you specify a pipeline stage that does not match the type of usage.
	//The third parameter is either 0 or VK_DEPENDENCY_BY_REGION_BIT
	//    The latter turns the barrier into a per-region condition. 
	//    That means that the implementation is allowed to already begin reading from the parts of a resource that were written so far
	//The last three pairs of parameters reference arrays of pipeline barriers of the three available types: memory barriers, buffer memory barriers, and image memory barriers
	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
	
	// End command buffer recoding and submit
	endSingleTimeCommands(commandBuffer);
}

void VKApplication::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height){
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	//Specify which part of the buffer is going to be copied to which part of the image
	VkBufferImageCopy region{};
	region.bufferOffset = 0;// specifies the byte offset in the buffer at which the pixel values start
	//he bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory. For example, you could have some padding bytes between rows of the image.
	//Specifying 0 for both indicates that the pixels are simply tightly packed like they are in our case.
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	// The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels.
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	//Copy buffer to image command
	//I'm assuming here that the image has already been transitioned to the layout that is optimal for copying pixels to. Right now we're only copying one chunk of pixels to the whole image, but it's possible to specify an array of VkBufferImageCopy to perform many different copies from this buffer to the image in one operation.
	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region
	);

	endSingleTimeCommands(commandBuffer);
}
