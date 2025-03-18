#pragma once

#include <iostream>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <optional>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
//For Surface
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//For Surface
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#define VK_USE_PLATFORM_WIN32_KHR

//For model view proj matrix
#define GLM_FORCE_RADIANS

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

//Allow multiple frames to be in-flight at once, that is to say, allow the rendering of one frame to not interfere with the recording of the next.
// - Thus, we need multiple command buffers, semaphores, and fences. 
const int MAX_FRAMES_IN_FLIGHT = 2;

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

//New structure called Vertex with the two attributes that we're going to use in the vertex shader
struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		//All of our per-vertex data is packed together in one array, so we're only going to have one binding. 
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;//The binding parameter specifies the index of the binding in the array of bindings. 
		bindingDescription.stride = sizeof(Vertex);// Specifies the number of bytes from one entry to the next
		//VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
		//K_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		
		//Position
		//The binding is loading one Vertex at a time and the position attribute (pos) is at an offset of 0 bytes from the beginning of this struct
		attributeDescriptions[0].binding = 0;//The binding parameter tells Vulkan from which binding the per-vertex data comes.
		attributeDescriptions[0].location = 0;//The location parameter references the location directive of the input in the vertex shader. The input in the vertex shader with location 0 is the position, which has two 32-bit float components.
		//The following shader types and formats are commonly used together:
		//float: VK_FORMAT_R32_SFLOAT
		//vec2: VK_FORMAT_R32G32_SFLOAT
		//vec3: VK_FORMAT_R32G32B32_SFLOAT
		//vec4: VK_FORMAT_R32G32B32A32_SFLOAT
		//The color type (SFLOAT, UINT, SINT) and bit width should also match the type of the shader input. See the following examples:
		//ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
		//uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
		//double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;//The format parameter describes the type of data for the attribute. Specified using the same enumeration as color formats. Implicitly defines the byte size of attribute data 
		attributeDescriptions[0].offset = offsetof(Vertex, pos);//Specifies the number of bytes since the start of the per-vertex data to read from.

		//Color
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

//Descriptor 
//A descriptor is a way for shaders to freely access resources like buffers and images. 
//We're going to set up a buffer that contains the transformation matrices and have the vertex shader access them through a descriptor
//Usage of descriptors consists of three parts:
// - Specify a descriptor layout during pipeline creation
// - Allocate a descriptor set from a descriptor pool
// - Bind the descriptor set during rendering
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

//Array of vertex data
//We're using exactly the same position and color values as before, but now they're combined into one array of vertices. This is known as interleaving vertex attributes.
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

//Array of index data
//It is possible to use either uint16_t or uint32_t for your index buffer depending on the number of entries in vertices
//uint16_t: 65535 unique vertices
const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

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
	GLFWwindow* window;
	
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

	//Descriptor Set Layout: Provides details about every descriptor binding used in the shaders for pipeline creation, just like we had to do for every vertex attribute and its location index
	// The descriptor layout specifies the types of resources that are going to be accessed by the pipeline, just like a render pass specifies the types of attachments that will be accessed. 
	VkDescriptorSetLayout descriptorSetLayout;

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
	std::vector<VkCommandBuffer> commandBuffers;

	//Semaphore to signal that an image has been acquired from the swapchain and is ready for rendering
	std::vector<VkSemaphore> imageAvailableSemaphores;
	//Semaphore to signal that rendering has finished and presentation can happen
	std::vector<VkSemaphore> renderFinishedSemaphores;
	//A fence to make sure only one frame is rendering at a time
	std::vector<VkFence> inFlightFences;

	//Handling resizes explicitly
	//Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize, it is not guaranteed to happen.
	bool framebufferResized = false;

	//To use the right objects every frame, we need to keep track of the current frame.
	uint32_t currentFrame = 0;

	//Vertex Buffer Handle
	VkBuffer vertexBuffer;

	//Allocated Memory for vertex buffer
	VkDeviceMemory vertexBufferMemory;

	//An index buffer is essentially an array of pointers into the vertex buffer.
	// It allows you to reorder the vertex data, and reuse existing data for multiple vertices
	VkBuffer indexBuffer;

	//Alocated Memory for the index buffer
	VkDeviceMemory indexBufferMemory;

	//Uniform Buffers
	std::vector<VkBuffer> uniformBuffers; //Buffer objects
	std::vector<VkDeviceMemory> uniformBuffersMemory;//Allocated memory for vertex buffer
	std::vector<void*> uniformBuffersMapped;// CPU access meomory to write data to

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

	void createDescriptorSetLayout();

	void createGraphicsPipeline();

	void createFramebuffers();

	void createCommandPool();

	void createVertexBuffer();

	void createIndexBuffer();

	void createUniformBuffers();

	void createCommandBuffers();

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

	// Swap chain recreation

	// It is possible for the window surface to change such that the swap chain is no longer compatible with it. 
	// One of the reasons that could cause this to happen is the size of the window changing. We have to catch these events and recreate the swap chain.
	void recreateSwapChain();

	//To make sure that the old versions of these objects (swapcahin, framebuffers and image views) are cleaned up before recreating them
	void cleanupSwapChain();

	//Resize window callback
	//Set the frambufferResized flag
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	//Vertex Buffer Creation

	//Graphics cards can offer different types of memory to allocate from. Each type of memory varies in terms of allowed operations and performance characteristics. We need to combine the requirements of the buffer and our own application requirements to find the right type of memory to use
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	//Copy the contents from one buffer to another (e.g from a Staging buffer [Host-Visible] to a Vertex buffer [device local])
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	// Update uniform buffer

	//Generates a new transformation every frame to make the geometry spin around.
	void updateUniformBuffer(uint32_t currentImage);
};