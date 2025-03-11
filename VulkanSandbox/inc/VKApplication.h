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
	//- We can use the vkGetDeviceQueue function to retrieve queue handles for each queue family. 
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

	//We have to create command pool before we can create command buffers. They manage the memory that is used to store the buffers and command buffers are allocated from them
	VkCommandPool commandPool;

	//Command Buffer: Commands in Vulkan, like drawing operations and memory transfers, are not executed directly using function calls. You have to record all of the operations you want to perform in command buffer objects. 
	//- The advantage of this is that when we are ready to tell the Vulkan what we want to do, all of the commands are submitted together and Vulkan can more efficiently process the commands since all of them are available together.
	//- In addition, this allows command recording to happen in multiple threads if so desired.
	//- Command buffers will be automatically freed when their command pool is destroyed, so we don't need explicit cleanup.
	VkCommandBuffer commandBuffer;

	//Semaphore to signal that an image has been acquired from the swapchain and is ready for rendering
	VkSemaphore imageAvailableSemaphore;
	//Semaphore to signal that rendering has finished and presentation can happen
	VkSemaphore renderFinishedSemaphore;
	//A fence to make sure only one frame is rendering at a time
	VkFence inFlightFence;

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

	void createCommandPool();

	void createCommandBuffer();

	void createSyncObjects();

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

	// Draw commands

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	//Rendering a frame in Vulkan consists of a common set of steps:
	// - Wait for the previous frame to finish
	// - Acquire an image from the swap chain
	// - Record a command buffer which draws the scene onto the iamge
	// - Submit the recorded command buffer
	// - Present the swap chain image
	void drawFrame();

	// Synchronization
	/*
	* In Vulkan synchronization of execution on the GPU is explicit
	* Many Vulkan API calls which start executing work on the GPU are asynchronous, the functions will return before the operation has finished
	* We need to order explicitly the folowing events because they can happen in parralle on the GPU are:
	* - Acquire an image from the swap chain
	* - Execute commands that draw onto the acquire image
	* - Present that image to the screen for presentation, returning it to the swapchain
	* 
	* Semaphores
	* A semaphore is used to add order between queue operations.
	* Queue operations refer to the work we submit to a queue, either in a command buffer or from within a function 
	* 
	* Examples of queues are: the graphics queue and the presentation queue. Semaphores are used both to order work inside the same queue and between different queues.
	* Two kinds of semaphores in Vulkan: binary and timeline
	* 
	* A semaphore (binary) is either unsignaled or signaled. It begins life as unsignaled. The way we use a semaphore to order queue operations is by providing the same semaphore as a 'signal' semaphore in one queue operation and as a 'wait' semaphore in another queue operation
	* - What we tell Vulkan is that operation A will 'signal' semaphore S when it finishes executing, and operation B will 'wait' on semaphore S before it begins executing.
	* - After operation B begins executing, semaphore S is automatically reset back to being unsignaled, allowing it to be used again.
	* 
	* VkCommandBuffer A, B = ... // record command buffers
	* VkSemaphore S = ... // create a semaphore
	* 
	* // enqueue A, signal S when done - starts executing immediately
	* vkQueueSubmit(work: A, signal: S, wait: None)
	* 
	* // enqueue B, wait on S to start
	* vkQueueSubmit(work: B, signal: None, wait: S)
	* 
	* Note: The waiting only happens on the GPU. The CPU continues running without blocking. To make the CPU wait, we need a different synchronization primitive (fences)
	* 
	* Fences
	* A fence has a similar purpose, in that it is used to synchronize execution, but it is for ordering the execution on the CPU, otherwise known as the host.
	* Simply put, if the host needs to know when the GPU has finished something, we use a fence.
	* 
	* Similar to semaphores, fences are either in a signaled or unsignaled state. Whenever we submit work to execute, we can attach a fence to that work. When the work is finished, the fence will be signaled. Then we can make the host wait for the fence to be signaled, guaranteeing that the work has finished before the host continues.
	* 
	* A concrete example is taking a screenshot:
	* Say we have already done the necessary work on the GPU. Now need to transfer the image from the GPU over to the host and then save the memory to a file
	* 
	* VkCommandBuffer A = ... // record command buffer with the transfer
	* VkFence F = ... // create the fence
	* 
	* // enqueue A, start work immediately, signal F when done
	* vkQueueSubmit(work: A, fence: F)
	* 
	* vkWaitForFence(F) // blocks execution until A has finished executing
	* 
	* save_screenshot_to_disk() // can't run until the transfer has finished
	* 
	* In general, it is preferable to not block the host unless necessary. We want to feed the GPU and the host with useful work to do. 
	* Thus we prefer semaphores, or other synchronization primitives not yet covered, to synchronize our work.
	* 
	* Fences must be reset manually to put them back into the unsignaled state
	* 
	* How to use them?
	* - Semaphores: for swapchain operations because they happen on the GPU, thus we don't want to make the host wait around if we can help it.
	* - Fences: Waiting for the previous frame to finish, because we need the host to wait.
	*		This is so we don't draw more than one frame at a time. 
	*		Because we re-record the command buffer every frame, we cannot record the next frame's work to the command buffer until the current frame has finished executing, as we don't want to overwrite the current contents of the command buffer while the GPU is using it.
	* 
	*/



};