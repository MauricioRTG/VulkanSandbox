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
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
}

void VKApplication::mainLoop() {
	//Check for events like pressing the X button until the window has been closed by the user
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void VKApplication::cleanup() {
	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

	for (auto framebuffer : swapChainFramebuffers) {
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
	}

	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

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

	
	//Render Pass

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create render pass!");
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

	//Vertex Input
	//Describes the format of the vertex data that will be passed to the vertex shader
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;// The cullMode variable determines the type of face culling to use. You can disable culling, cull the front faces, cull the back faces or both.
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;// The frontFace variable specifies the vertex order for faces to be considered front-facing and can be clockwise or counterclockwise.
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
	pipelineLayoutInfo.setLayoutCount = 0; //means that no descriptor sets are used in this pipeline. 
	pipelineLayoutInfo.pSetLayouts = nullptr;// means there's no descriptor set layout.
	//Push constants are small amounts of data that can be passed directly to shaders.
	pipelineLayoutInfo.pushConstantRangeCount = 0; //no push constants are used
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to createpipeline layout!");
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

void VKApplication::createCommandBuffer(){

	//Specifies the command pool and number of buffers to allocate:
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	//The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
	//- VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
	//- VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers.
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
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
