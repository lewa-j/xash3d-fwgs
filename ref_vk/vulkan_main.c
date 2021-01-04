
#include "vk_local.h"

#define VK_FUNCTION( fun ) PFN_##fun fun;
#define VK_GLOBAL_FUNCTION( fun ) PFN_##fun fun;
#define VK_EXT_FUNCTION( fun ) PFN_##fun fun;
#include "vk_funcs.h"
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

cvar_t	*gl_nosort;
cvar_t	*r_speeds;
cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_lockfrustum;

byte		*r_temppool;

vkstate_t	vkState;
gl_globals_t	tr;

//Global objects
VkInstance instance = VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT debugMessenger;
VK_PhysicalDevice physicalDevice;
VK_Surface surface;
VkDevice device;
VkQueue renderQueue;
VK_Swapchain swapchain;
VkFormat depthFormat = VK_FORMAT_D16_UNORM;
VkRenderPass renderPass = VK_NULL_HANDLE;
VkDescriptorPool descriptorPool;
VkCommandPool commandPool;
VkCommandBuffer renderCommandBuffer;

VK_Buffer vertexBufferQuad;
void *vertexBufferMapped = 0;
testVert_t *tempQuadVerts = 0;
uint32_t currentVert = 0;
VK_Draw2D_t vk_draws[1024];
uint32_t vk_current2D_draw=0;

VkPipelineCache pipelineCache = VK_NULL_HANDLE;//TODO
VK_GraphicsPipeline pipeline2DTexColor;
VK_GraphicsPipeline pipeline2DTexColorTrans;
VK_GraphicsPipeline pipeline2DTexColorAdd;
VkSampler testSampler = VK_NULL_HANDLE;

typedef struct 
{
	VkCommandBuffer commandBuffer;
	VkSemaphore semaphore;
	VkSemaphore signalSemaphore;
	VkFence fence;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	VkFramebuffer framebuffer;
} VK_frame;

VK_frame frameResources;


int VK_CreateInstance()
{
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Xash3D FWGS";
	appInfo.applicationVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.pEngineName = "Xash3D FWGS";
	appInfo.engineVersion = VK_MAKE_VERSION(0,20,0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	uint32_t extsCount = 4;
	const char* extsNames[4]= {0};
	gEngfuncs.VK_GetInstanceExtensions( &extsCount, extsNames );
	gEngfuncs.Con_Printf( "VK_GetInstanceExtensions %d\n", extsCount );

	extsNames[extsCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	extsCount++;
	uint32_t layersCount = 1;
	const char* layersNames[1] = {"VK_LAYER_KHRONOS_validation"};

	for(size_t i=0;i<extsCount;i++){
		gEngfuncs.Con_Printf( "Req inst ext %d: %s\n",i,extsNames[i] );
	}

	VkInstanceCreateInfo instInfo = {0};
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.enabledLayerCount = layersCount;
	instInfo.ppEnabledLayerNames = layersNames;
	instInfo.enabledExtensionCount = extsCount;
	instInfo.ppEnabledExtensionNames = extsNames;

	VkResult r = vkCreateInstance( &instInfo, NULL, &instance );
	if(r){
		gEngfuncs.Host_Error( "vkCreateInstance return %X\n", r );
		return -1;
	}

	return 0;
}

static VkBool32 VK_DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	gEngfuncs.Con_Printf("Vulkan: %d %d| %s| %s\n",messageSeverity,messageType,pCallbackData->pMessageIdName,pCallbackData->pMessage);
	return false;
}

int VK_InitDebug()
{
	//debug
	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {0};
	messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerInfo.messageSeverity =
	//			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	//			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerInfo.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)VK_DebugUtilsCallback;

	VkResult r = vkCreateDebugUtilsMessengerEXT(instance, &messengerInfo, NULL, &debugMessenger);
	if(r){
		gEngfuncs.Host_Error("vkCreateDebugUtilsMessengerEXT return %X\n",r);
		return -1;
	}
	return 0;
}

int VK_GetPhysicalDevice()
{
	uint32_t numPhysDevices;
	vkEnumeratePhysicalDevices(instance, &numPhysDevices, 0);
	gEngfuncs.Con_Printf("Vulkan: physical devices count %d\n", numPhysDevices);
	if(numPhysDevices < 1){
		gEngfuncs.Host_Error("Vulkan: failed to find a compatible GPU\n");
		return -1;
	}
	VkPhysicalDevice *physDevices = Mem_Malloc( r_temppool, sizeof(VkPhysicalDevice)*numPhysDevices);
	vkEnumeratePhysicalDevices(instance, &numPhysDevices, physDevices);
	for(uint32_t i=0; i<numPhysDevices; i++){
		VkPhysicalDeviceProperties physDeviceProps;
		vkGetPhysicalDeviceProperties(physDevices[i], &physDeviceProps);
		gEngfuncs.Con_Printf("Vulkan: device %d: %s (t %X), api ver: %d.%d.%d, driver %d.%d.%d\n",i,physDeviceProps.deviceName,physDeviceProps.deviceType,
				VK_VERSION_MAJOR(physDeviceProps.apiVersion),VK_VERSION_MINOR(physDeviceProps.apiVersion),VK_VERSION_PATCH(physDeviceProps.apiVersion),
				VK_VERSION_MAJOR(physDeviceProps.driverVersion),VK_VERSION_MINOR(physDeviceProps.driverVersion),VK_VERSION_PATCH(physDeviceProps.driverVersion));
	}

	//TODO
	physicalDevice.handle = physDevices[0];
	Mem_Free( physDevices );

	//Properties
	vkGetPhysicalDeviceProperties(physicalDevice.handle, &physicalDevice.props);
	vkGetPhysicalDeviceFeatures(physicalDevice.handle, &physicalDevice.features);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice.handle, &physicalDevice.memProps);
	physicalDevice.queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.handle, &physicalDevice.queueFamilyCount, NULL);
	physicalDevice.queueProps = Mem_Malloc( r_temppool, sizeof(VkQueueFamilyProperties)*physicalDevice.queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice.handle, &physicalDevice.queueFamilyCount, physicalDevice.queueProps);
	
	//gEngfuncs.Con_Printf("device maxVertexInputAttributes %d\n",physicalDevice.props.limits.maxVertexInputAttributes);
	
	return 0;
}

int VK_CreateSurface()
{
	VkResult r = gEngfuncs.VK_CreateSurface(instance, &surface.handle);
	if(!surface.handle){
		gEngfuncs.Host_Error( "Vulkan: Can't create surface\n" );
		return -1;
	}

	r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.handle, surface.handle, &surface.capabilities);
	if(r){
		gEngfuncs.Host_Error( "vkGetPhysicalDeviceSurfaceCapabilitiesKHR return %X)\n",r);
		return -1;
	}

	surface.presentModesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.handle, surface.handle, &surface.presentModesCount, NULL);
	if(surface.presentModesCount){
		surface.presentModes = Mem_Malloc( r_temppool, sizeof(VkPresentModeKHR)*surface.presentModesCount );
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice.handle, surface.handle, &surface.presentModesCount, surface.presentModes);
	}

	surface.formatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.handle, surface.handle, &surface.formatsCount, 0);
	if(surface.formatsCount){
		surface.formats = Mem_Malloc( r_temppool, sizeof(VkSurfaceFormatKHR)*surface.formatsCount );
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.handle, surface.handle, &surface.formatsCount, surface.formats);
	}

	gEngfuncs.Con_Printf("Vulkan: surface capabilities: minImageCount %d, maxImageCount %d, cur extent %dx%d, min extent %dx%d, max extent %dx%d, supportedTransforms 0x%X, currentTransform 0x%X, supportedUsageFlags 0x%X\n", 
		surface.capabilities.minImageCount, surface.capabilities.maxImageCount, surface.capabilities.currentExtent.width, surface.capabilities.currentExtent.height, surface.capabilities.minImageExtent.width, surface.capabilities.minImageExtent.height, 
		surface.capabilities.maxImageExtent.width, surface.capabilities.maxImageExtent.height, surface.capabilities.supportedTransforms, surface.capabilities.currentTransform, surface.capabilities.supportedUsageFlags);

	return r;
}

int VK_CreateDevice()
{
	physicalDevice.queueFamilyIndex = (uint32_t)-1;
	for(uint32_t i=0; i<physicalDevice.queueFamilyCount; i++){
		VkBool32 supported = true;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice.handle, i, surface.handle, &supported);
		gEngfuncs.Con_Printf("qe %d: flags %d, SurfaceSupport %d\n", i, physicalDevice.queueProps[i].queueFlags, supported);
		if(physicalDevice.queueProps[i].queueFlags&VK_QUEUE_GRAPHICS_BIT && supported){
			physicalDevice.queueFamilyIndex = i;
			break;
		}
	}
	
	if(physicalDevice.queueFamilyIndex == (uint32_t)-1){
		gEngfuncs.Host_Error( "PhysicalDevice queue family with graphics and presentation support not found\n" );
		return -1;
	}
	
	VkDeviceQueueCreateInfo queueCreateInfo = {0};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = physicalDevice.queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	float queuePriority = 1.0f;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	const char *extNames[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	VkPhysicalDeviceFeatures desiredFeatures = {0};
	desiredFeatures.samplerAnisotropy = physicalDevice.features.samplerAnisotropy;

	VkDeviceCreateInfo deviceCreateInfo={0};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = extNames;
	deviceCreateInfo.pEnabledFeatures = &desiredFeatures;

	VkResult r = vkCreateDevice(physicalDevice.handle, &deviceCreateInfo, 0, &device);
	if(r){
		gEngfuncs.Host_Error("vkCreateDevice return %X\n",r);
		return -1;
	}
	
	vkGetDeviceQueue(device, physicalDevice.queueFamilyIndex, 0, &renderQueue);
	
	return 0;
}

int VK_FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	for(uint32_t i=0; i<physicalDevice.memProps.memoryTypeCount; i++){
		if((typeFilter & (1 << i))&&(physicalDevice.memProps.memoryTypes[i].propertyFlags & properties)==properties){
			return i;
		}
	}

	return -1;
}

int VK_CreateSwapchain()
{
	swapchain.extent = surface.capabilities.currentExtent;
	swapchain.surfaceFormat = surface.formats[0];//TODO
	swapchain.imageCount = surface.capabilities.minImageCount;
	swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;//TODO
	
	VkSwapchainCreateInfoKHR scCreateInfo = {0};
	scCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	scCreateInfo.surface = surface.handle;
	scCreateInfo.minImageCount = swapchain.imageCount;
	scCreateInfo.imageFormat = swapchain.surfaceFormat.format;
	scCreateInfo.imageColorSpace = swapchain.surfaceFormat.colorSpace;
	scCreateInfo.imageExtent = swapchain.extent;
	scCreateInfo.imageArrayLayers = 1;
	scCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	scCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	scCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	scCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	scCreateInfo.presentMode = swapchain.presentMode;
	scCreateInfo.clipped = VK_TRUE;
	scCreateInfo.oldSwapchain = VK_NULL_HANDLE;//TODO

	VkResult r = vkCreateSwapchainKHR(device, &scCreateInfo, NULL, &swapchain.handle);
	if(r){
		gEngfuncs.Host_Error("vkCreateSwapchainKHR return %X\n", r);
		return -1;
	}
	
	uint32_t actualImageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain.handle, &actualImageCount, 0);
	swapchain.imageCount = actualImageCount;
	gEngfuncs.Con_Printf("SwapchainImages count %d\n", actualImageCount);
	swapchain.images = Mem_Malloc( r_temppool, sizeof(VkImage)*swapchain.imageCount );
	vkGetSwapchainImagesKHR(device, swapchain.handle, &swapchain.imageCount, swapchain.images);

	swapchain.imageViews = Mem_Malloc( r_temppool, sizeof(VkImageView)*swapchain.imageCount );
	for(uint32_t i=0; i<swapchain.imageCount; i++){
		swapchain.imageViews[i] = VK_CreateImageView(swapchain.images[i], VK_IMAGE_VIEW_TYPE_2D, swapchain.surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, IMAGE_HAS_ALPHA);
		if(!swapchain.imageViews[i]){
			return -1;
		}
	}
	
	return 0;
}

int VK_CreateCommandPool(uint32_t queueFamily)
{
	VkCommandPoolCreateInfo poolCreateInfo={0};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolCreateInfo.queueFamilyIndex = queueFamily;

	gEngfuncs.Con_Printf("VK_CreateCommandPool (qf %d)\n", queueFamily);
	VkResult r = vkCreateCommandPool(device, &poolCreateInfo, 0, &commandPool);
	if(r){
		gEngfuncs.Host_Error("vkCreateCommandPool return %X\n",r);
		return -1;
	}
	return 0;
}

VkCommandBuffer VK_AllocateCommandBuffer(VkCommandBufferLevel level)
{
	VkCommandBufferAllocateInfo bufferAllocInfo = {0};
	bufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferAllocInfo.commandPool = commandPool;
	bufferAllocInfo.level = level;
	bufferAllocInfo.commandBufferCount = 1;

	VkCommandBuffer out = VK_NULL_HANDLE;
	VkResult r = vkAllocateCommandBuffers(device, &bufferAllocInfo, &out);
	if(r){
		gEngfuncs.Host_Error("vkAllocateCommandBuffers return %X)\n",r);
		return VK_NULL_HANDLE;
	}
	return out;
}

int VK_CreateRenderPass()
{
	uint32_t attachmentCount = 2;
	VkAttachmentDescription attachments[]={
	{
		0,
		swapchain.surfaceFormat.format,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_STORE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	},{
		0,
		depthFormat,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	}
	};

	VkAttachmentReference attachmentRef={0};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef={0};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass={0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo={0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachmentCount;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VkResult r = vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass);
	if(r){
		gEngfuncs.Host_Error("vkCreateRenderPass return %X\n", r);
		return -1;
	}

	return 0;
}

VkFramebuffer VK_CreateFramebuffer(VkRenderPass rp, VkExtent2D extent, VkImageView *attachments)
{
	VkFramebufferCreateInfo framebufferInfo = {0};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = rp;
	framebufferInfo.attachmentCount = 2;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	VkFramebuffer out = VK_NULL_HANDLE;
	VkResult r = vkCreateFramebuffer(device, &framebufferInfo, 0, &out);
	if(r){
		gEngfuncs.Host_Error("vkCreateFramebuffer return %X\n",r);
		return VK_NULL_HANDLE;
	}
	return out;
}

VK_Buffer VK_CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags)
{
	VK_Buffer buf = {0};

	VkBufferCreateInfo bufferInfo = {0};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult r = vkCreateBuffer(device, &bufferInfo, NULL, &buf.handle);
	if(r){
		gEngfuncs.Host_Error("vkCreateBuffer return %X\n", r);
		return buf;//NULL
	}

	VkMemoryRequirements memReqs = {0};
	vkGetBufferMemoryRequirements(device, buf.handle, &memReqs);
	//Log("buffer %p: memory requirements: size %lld, alignment %lld, memory type bits %d\n",buf.handle,memReqs.size,memReqs.alignment,memReqs.memoryTypeBits);
	buf.size = memReqs.size;

	int memType = VK_FindMemoryType(memReqs.memoryTypeBits, memFlags);
	//Log("find memory type: %d\n",memType);

	VkMemoryAllocateInfo allocInfo = {0};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memType;

	r = vkAllocateMemory(device, &allocInfo, NULL, &buf.memory);
	if(r){
		gEngfuncs.Host_Error("vkAllocateMemory return %X\n",r);
		return buf;//NULL
	}

	r = vkBindBufferMemory(device, buf.handle, buf.memory, /*memoryOffset*/0);
	if(r){
		gEngfuncs.Host_Error("vkBindBufferMemory return %X\n",r);
		return buf;//NULL
	}

	return buf;
}

VkDescriptorSetLayout VK_CreateDescriptorSetLayout( VkDescriptorSetLayoutBinding * bindings, uint32_t count )
{

	VkDescriptorSetLayoutCreateInfo descSetLayInfo = {0};
	descSetLayInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayInfo.bindingCount = count;
	descSetLayInfo.pBindings = bindings;

	VkDescriptorSetLayout out = VK_NULL_HANDLE;
	VkResult r = vkCreateDescriptorSetLayout(device, &descSetLayInfo, NULL, &out);
	if(r){
		gEngfuncs.Host_Error("vkCreateDescriptorSetLayout return %X\n", r);
		return VK_NULL_HANDLE;
	}
	return out;
}

int VK_CreatePipelines()
{
	VkShaderModule vertModule = VK_CreateShader( "shaders/test.vert.spv" );
	VkShaderModule fragModule = VK_CreateShader( "shaders/test.frag.spv" );
	VK_VertexInputDesc vertInputDesc = {
		1,
		{{0, sizeof(testVert_t), VK_VERTEX_INPUT_RATE_VERTEX}},
		3,
		{{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(testVert_t, pos)},
		 {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(testVert_t, uv)},
		 {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(testVert_t, color)}}
	};
	VkDescriptorSetLayoutBinding bindings[] = {
		/*{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, 0},*/
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0}};
	VkDescriptorSetLayout testDescLayout = VK_CreateDescriptorSetLayout( bindings, 1 );//2
	VkPipelineLayout testPipeLayout = VK_CreatePipelineLayout( &testDescLayout, 0 );

	pipeline2DTexColor = VK_CreateGraphicsPipeline( vertModule, fragModule, &vertInputDesc, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, testDescLayout, testPipeLayout, renderPass, swapchain.extent, 0 );
	pipeline2DTexColorTrans = VK_CreateGraphicsPipeline( vertModule, fragModule, &vertInputDesc, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, testDescLayout, testPipeLayout, renderPass, swapchain.extent, kRenderTransTexture );
	pipeline2DTexColorAdd = VK_CreateGraphicsPipeline( vertModule, fragModule, &vertInputDesc, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, testDescLayout, testPipeLayout, renderPass, swapchain.extent, kRenderTransAdd );

	return 0;
}

VkSemaphore VK_CreateSemaphore()
{
	VkSemaphoreCreateInfo semaphoreInfo = {0};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore out = VK_NULL_HANDLE;
	VkResult r = vkCreateSemaphore(device, &semaphoreInfo, NULL, &out);
	if(r){
		gEngfuncs.Host_Error("vkCreateSemaphore return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return out;
}

VkFence VK_CreateFence(qboolean signaled)
{
	VkFenceCreateInfo fenceInfo = {0};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	VkFence out = VK_NULL_HANDLE;
	VkResult r = vkCreateFence(device, &fenceInfo, NULL, &out);
	if(r){
		gEngfuncs.Host_Error("vkCreateFence return %X\n",r);
		return VK_NULL_HANDLE;
	}

	return out;
}

int VK_CreateFrameResources()
{
	//frameResources.commandBuffer = VK_AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	frameResources.semaphore = VK_CreateSemaphore();
	frameResources.signalSemaphore = VK_CreateSemaphore();
	frameResources.fence = VK_CreateFence(true);
	//CreateDepthResources(frameResources.depthImage, frameResources.depthImageView);
	frameResources.depthImage = VK_CreateImage(&frameResources.depthImageMemory, swapchain.extent, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	frameResources.depthImageView = VK_CreateImageView(frameResources.depthImage, VK_IMAGE_VIEW_TYPE_2D, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, IMAGE_HAS_ALPHA);
	frameResources.framebuffer = VK_NULL_HANDLE;
	
	/*//transition layout
	VkCommandBuffer comBuf = commandPool.BeginSingleTimeCommands();
	//VK_IMAGE_ASPECT_STENCIL_BIT
	img.PipelineBarrier(comBuf, VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	commandPool.EndSingleTimeCommands(comBuf);*/

	return 0;
}

VkDescriptorSet vk_textureDescriptorSets[MAX_TEXTURES];
static uint32_t vk_lastTexturesCount = 0;

int VK_CreateDescriptorPool()
{
	uint32_t totalObjects = MAX_TEXTURES;
	//uint32_t buffersCount = 0;
	uint32_t texturesCount = MAX_TEXTURES;

	VkDescriptorPoolSize poolSizes[] = {
		/*{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffersCount },*/
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturesCount }
	};

	VkDescriptorPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;//VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
	poolInfo.maxSets = totalObjects;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = poolSizes;

	VkResult r = vkCreateDescriptorPool( device, &poolInfo, NULL, &descriptorPool );
	if(r){
		gEngfuncs.Host_Error( "vkCreateDescriptorPool return %X\n", r );
		return -1;
	}

	return 0;
}

void VK_UpdateDescriptors()
{
	uint32_t texturesCount = 0;
	qboolean needUpload = 0;
	for(uint32_t i=0; i<MAX_TEXTURES; i++){
		vk_texture_t *tex = R_GetTexture(i);
		if(tex->imageView){
			texturesCount++;
			if(!tex->descriptor)
				needUpload = 1;
		}
	}

	if(!texturesCount)
		return;

	if(vk_lastTexturesCount == texturesCount && !needUpload)
		return;

	if(vk_lastTexturesCount < texturesCount)
	{
		if(vk_lastTexturesCount){
			//vkFreeDescriptorSets( device, descriptorPool, vk_textureDescriptorSets, vk_lastTexturesCount );
			vkResetDescriptorPool( device, descriptorPool, 0 );
		}

		uint32_t layoutsCount = texturesCount;
		VkDescriptorSetLayout *layouts = Mem_Malloc( r_temppool, sizeof(VkDescriptorSetLayout)*layoutsCount );
		for(uint32_t i=0; i<layoutsCount; i++){
			layouts[i] = pipeline2DTexColor.descriptorSetLayout;
		}

		VkDescriptorSetAllocateInfo allocInfo = {0};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = layoutsCount;
		allocInfo.pSetLayouts = layouts;

		VkResult r = vkAllocateDescriptorSets(device, &allocInfo, vk_textureDescriptorSets);
		Mem_Free( layouts );
		
		if(r){
			gEngfuncs.Host_Error("vkAllocateDescriptorSets return %X(%s)\n",r);
			return;
		}
		vk_lastTexturesCount = texturesCount;
	}
	
	VkWriteDescriptorSet descWrites[MAX_TEXTURES] = {0};
	VkDescriptorImageInfo imageInfos[MAX_TEXTURES] = {0};
	uint32_t wdsId = 0;
	for(uint32_t i=0; i<MAX_TEXTURES; i++){
		vk_texture_t *tex = R_GetTexture(i);
		if(!tex->imageView)
			continue;
		
		imageInfos[wdsId] = (VkDescriptorImageInfo){testSampler, tex->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

		descWrites[wdsId] = (VkWriteDescriptorSet){0};
		descWrites[wdsId].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrites[wdsId].dstSet = vk_textureDescriptorSets[wdsId];
		descWrites[wdsId].dstBinding = 0;
		descWrites[wdsId].dstArrayElement = 0;
		descWrites[wdsId].descriptorCount = 1;
		descWrites[wdsId].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descWrites[wdsId].pImageInfo = &imageInfos[wdsId];
		
		tex->descriptor = vk_textureDescriptorSets[wdsId];
		
		wdsId++;
	}
	vkUpdateDescriptorSets(device, wdsId, descWrites, 0, NULL);
	gEngfuncs.Con_Printf("Updated %d DescriptorSets\n", wdsId);
}

void VK_Test()
{
	VK_Buffer stagingBuffer = VK_CreateBuffer(swapchain.extent.width*swapchain.extent.height*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	void *mapped = 0;
	vkMapMemory(device, stagingBuffer.memory, 0, stagingBuffer.size, 0, &mapped);
	for(int i=0; i<swapchain.extent.width*swapchain.extent.height*4;)
	{
		((uint8_t*)mapped)[i] = i%256;
		((uint8_t*)mapped)[i+1] = i/4%256;
		((uint8_t*)mapped)[i+2] = i/16%256;
		i+=4;
	}
	vkUnmapMemory(device, stagingBuffer.memory);
	
	
	uint32_t imgIndex = 0;
	VkResult r = vkAcquireNextImageKHR(device, swapchain.handle, 1000000000, VK_NULL_HANDLE, VK_NULL_HANDLE, &imgIndex);
	gEngfuncs.Con_Printf( "AcquireNextImage r %d, id %d\n", r, imgIndex );
	
	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(renderCommandBuffer, &beginInfo);
	
	VkBufferImageCopy region = {0,0,0,
		{VK_IMAGE_ASPECT_COLOR_BIT,0,0,1},
		{0,0,0},
		{swapchain.extent.width,swapchain.extent.height,1}};
	vkCmdCopyBufferToImage(renderCommandBuffer, stagingBuffer.handle, swapchain.images[imgIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vkEndCommandBuffer(renderCommandBuffer);
	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderCommandBuffer;
	vkQueueSubmit(renderQueue, 1, &submitInfo, /*fence*/VK_NULL_HANDLE);
	vkQueueWaitIdle(renderQueue);

	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 0;
	presentInfo.pWaitSemaphores = 0;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.handle;
	presentInfo.pImageIndices = &imgIndex;

	r = vkQueuePresentKHR(renderQueue, &presentInfo);
	vkQueueWaitIdle(renderQueue);
	//
}

VkCommandBuffer VK_BeginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo cbAllocInfo = {0};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = commandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbAllocInfo.commandBufferCount = 1;

	VkCommandBuffer comBuf = VK_NULL_HANDLE;
	VkResult r = vkAllocateCommandBuffers(device, &cbAllocInfo, &comBuf);
	if(r){
		gEngfuncs.Host_Error("vkAllocateCommandBuffers return %X\n",r);
		return VK_NULL_HANDLE;
	}

	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	r = vkBeginCommandBuffer(comBuf, &beginInfo);
	if(r){
		gEngfuncs.Host_Error("vkBeginCommandBuffer return %X\n", r);
		//vkFreeCommandBuffers(device, commandPool, 1, &comBuf);
		return VK_NULL_HANDLE;
	}
	return comBuf;
}

int VK_EndSingleTimeCommands(VkCommandBuffer comBuf)
{
	VkResult r = vkEndCommandBuffer(comBuf);
	if(r){
		gEngfuncs.Host_Error("vkEndCommandBuffer return %X\n",r);
		return -1;
	}

	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &comBuf;
	r = vkQueueSubmit(renderQueue, 1, &submitInfo, /*fence*/VK_NULL_HANDLE);
	if(r){
		gEngfuncs.Host_Error("vkQueueSubmit return %X\n",r);
		return -1;
	}
	vkQueueWaitIdle(renderQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &comBuf);
	return 0;
}

int frame=0;
void VK_Present()
{
	VkResult r = VK_SUCCESS;
	r = vkWaitForFences(device, 1, &frameResources.fence, true, 2000000000);
	if(r){
		gEngfuncs.Host_Error("vkWaitForFences return %X\n",r);
		return;
	}
	r = vkResetFences(device, 1, &frameResources.fence);
	if(r){
		gEngfuncs.Host_Error("vkResetFences return %X\n",r);
		return;
	}
	
	
	vkUnmapMemory(device, vertexBufferQuad.memory);
	
	if(frameResources.framebuffer){
		vkDestroyFramebuffer(device, frameResources.framebuffer, NULL);
		frameResources.framebuffer = VK_NULL_HANDLE;
	}
	
	VK_UpdateDescriptors();

	uint32_t imgIndex = 0;
	r = vkAcquireNextImageKHR(device, swapchain.handle, 1000000000, frameResources.semaphore, VK_NULL_HANDLE, &imgIndex);
	if(r == VK_ERROR_OUT_OF_DATE_KHR){
		//VK_RecreateSwapchain();
		//return;
	}else if(r<0){
		gEngfuncs.Host_Error("vkAcquireNextImageKHR error %X\n",r);
		return;
	}
	
	VkImageView attachments[] = {swapchain.imageViews[imgIndex], frameResources.depthImageView};
	frameResources.framebuffer = VK_CreateFramebuffer(renderPass, swapchain.extent, attachments);

	//record
	VkCommandBufferBeginInfo beginInfo = {0};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(renderCommandBuffer, &beginInfo);
	
	VkClearValue clearVals[2] = {0};
	clearVals[0].color = (VkClearColorValue){{0.1f, 0.1f, 0.1f, 1.0f}};
	clearVals[1].depthStencil = (VkClearDepthStencilValue){1.0f,0};
	VkRenderPassBeginInfo renderPassBeginInfo = {0};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = frameResources.framebuffer;
	renderPassBeginInfo.renderArea = (VkRect2D){{0,0},swapchain.extent};
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearVals;
	vkCmdBeginRenderPass(renderCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
	
	VkViewport viewport = {0,0, swapchain.extent.width, swapchain.extent.height, 0, 1};
	VkRect2D scissor = {{0,0}, {swapchain.extent.width, swapchain.extent.height}};
	vkCmdSetViewport( renderCommandBuffer, 0, 1, &viewport );
	vkCmdSetScissor( renderCommandBuffer, 0, 1, &scissor );
	
	//vkCmdBindPipeline(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColor.pipeline);
	
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(renderCommandBuffer, 0, 1, &vertexBufferQuad.handle, &offset);
	
	uint32_t lastRenderMode = -1;
	for(uint32_t i=0; i<=vk_current2D_draw; i++)
	{
		if(!vk_draws[i].count)
			continue;
		if(lastRenderMode != vk_draws[i].renderMode){
			switch( vk_draws[i].renderMode )
			{
			case kRenderNormal:
			default:
				vkCmdBindPipeline(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColor.pipeline);
				break;
			case kRenderTransColor:
			case kRenderTransTexture:
				vkCmdBindPipeline(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColorTrans.pipeline);
				break;
			case kRenderTransAlpha:
				//TODO alpha test
				vkCmdBindPipeline(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColor.pipeline);
				break;
			case kRenderGlow:
			case kRenderTransAdd:
				vkCmdBindPipeline(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColorAdd.pipeline);
				break;
			}
			lastRenderMode = vk_draws[i].renderMode;
		}
		//gEngfuncs.Con_Printf( "Draw2D: %d offset %d, count %d, tex %d %s, iv %d, desc %d\n", i, vk_draws[i].offset, vk_draws[i].count, vk_draws[i].tex, R_GetTexture(vk_draws[i].tex)->name, (int)R_GetTexture(vk_draws[i].tex)->imageView, R_GetTexture(vk_draws[i].tex)->descriptor );
		if(R_GetTexture(vk_draws[i].tex)->descriptor)
			vkCmdBindDescriptorSets(renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline2DTexColor.pipelineLayout, 0, 1, &R_GetTexture(vk_draws[i].tex)->descriptor, 0, NULL);
		vkCmdDraw(renderCommandBuffer, vk_draws[i].count, 1, vk_draws[i].offset, 0);
	}
	
	vkCmdEndRenderPass(renderCommandBuffer);

	vkEndCommandBuffer(renderCommandBuffer);

	//submit
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;//VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {0};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frameResources.semaphore;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderCommandBuffer;//frameResources.commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frameResources.signalSemaphore;
	vkQueueSubmit(renderQueue, 1, &submitInfo, frameResources.fence);

	//present
	VkPresentInfoKHR presentInfo = {0};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frameResources.signalSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.handle;
	presentInfo.pImageIndices = &imgIndex;
	r = vkQueuePresentKHR(renderQueue, &presentInfo);
	if(r){
		gEngfuncs.Host_Error("vkQueuePresentKHR return %X\n",r);
		return;
	}
	vkQueueWaitIdle(renderQueue);
	
	vkMapMemory(device, vertexBufferQuad.memory, 0, vertexBufferQuad.size, 0, &vertexBufferMapped);
	tempQuadVerts = (testVert_t*)vertexBufferMapped;
	currentVert = 0;
	vk_current2D_draw = 0;
	vk_draws[0].offset = 0;
	vk_draws[0].count = 0;
}

void VK_Shutdown()
{
	vkDeviceWaitIdle(device);
	
	vkDestroySampler(device, testSampler, NULL);
	
	vkDestroyPipeline(device, pipeline2DTexColorTrans.pipeline, NULL);
	vkDestroyPipeline(device, pipeline2DTexColorAdd.pipeline, NULL);
	
	vkDestroyPipeline(device, pipeline2DTexColor.pipeline, NULL);
	vkDestroyShaderModule(device,pipeline2DTexColor.shaderVert, NULL);
	vkDestroyShaderModule(device,pipeline2DTexColor.shaderFrag, NULL);
	vkDestroyPipelineLayout(device,pipeline2DTexColor.pipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(device,pipeline2DTexColor.descriptorSetLayout, NULL);
	
	vkDestroyBuffer(device, vertexBufferQuad.handle, NULL);
	vkFreeMemory(device, vertexBufferQuad.memory, NULL);

	vkDestroyDescriptorPool( device, descriptorPool, NULL );
	
	vkDestroySemaphore(device, frameResources.semaphore, NULL);
	vkDestroySemaphore(device, frameResources.signalSemaphore, NULL);
	vkDestroyFence(device, frameResources.fence, NULL);
	vkDestroyImageView(device, frameResources.depthImageView, NULL);
	vkDestroyImage(device, frameResources.depthImage, NULL);
	vkFreeMemory(device, frameResources.depthImageMemory, NULL);
	if(frameResources.framebuffer){
		vkDestroyFramebuffer(device, frameResources.framebuffer, NULL);
	}
	
	vkDestroyRenderPass(device, renderPass, NULL);
	
	vkDestroyCommandPool(device, commandPool, NULL);
	vkDestroySwapchainKHR(device, swapchain.handle, NULL);
	for(uint32_t i=0; i<swapchain.imageCount; i++){
		vkDestroyImageView(device, swapchain.imageViews[i], NULL);
	}
	Mem_Free( swapchain.images );
	Mem_Free( swapchain.imageViews );
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface.handle, NULL);
	Mem_Free( surface.presentModes );
	Mem_Free( surface.formats );
	Mem_Free( physicalDevice.queueProps );
	
	if(debugMessenger){
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
	}
	
	vkDestroyInstance(instance, NULL);
}


void GL_InitExtensions( void )
{
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)gEngfuncs.VK_GetInstanceProcAddr;
	if(!vkGetInstanceProcAddr){
		gEngfuncs.Host_Error("Can't get vkGetInstanceProcAddr");
		return;
	}
	
	vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(NULL,"vkCreateInstance");
	gEngfuncs.Con_Printf( "vkCreateInstance %p\n", vkCreateInstance );
	vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(NULL,"vkEnumerateInstanceVersion");
	vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(NULL,"vkEnumerateInstanceExtensionProperties");
	vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkGetInstanceProcAddr(NULL,"vkEnumerateInstanceLayerProperties");

	if( vkEnumerateInstanceVersion ){
		uint32_t apiVer = 0;
		vkEnumerateInstanceVersion( &apiVer );
		gEngfuncs.Con_Printf( "Instance version %d.%d.%d\n",VK_VERSION_MAJOR(apiVer),VK_VERSION_MINOR(apiVer),VK_VERSION_PATCH(apiVer) );
	}else{
		gEngfuncs.Con_Printf( "Vulkan 1.1+ unsupported (vkEnumerateInstanceVersion)\n" );
	}
	
	VK_CreateInstance();
	
	vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr( instance, "vkDestroyInstance" );

#define VK_FUNCTION( fun ) fun = (PFN_##fun)vkGetInstanceProcAddr( instance, #fun );
#define VK_EXT_FUNCTION( fun ) VK_FUNCTION( fun )
//TODO fix VK_EXT_FUNCTION
#include "vk_funcs.h"

	VK_InitDebug();
}

void GL_ClearExtensions( void )
{

}

void GL_InitCommands( void )
{
	r_speeds = gEngfuncs.Cvar_Get( "r_speeds", "0", FCVAR_ARCHIVE, "shows renderer speeds" );
	r_norefresh = gEngfuncs.Cvar_Get( "r_norefresh", "0", 0, "disable 3D rendering (use with caution)" );
	r_drawentities = gEngfuncs.Cvar_Get( "r_drawentities", "1", FCVAR_CHEAT, "render entities" );
	r_lockfrustum = gEngfuncs.Cvar_Get( "r_lockfrustum", "0", FCVAR_CHEAT, "lock frustrum area at current point (cull test)" );

	gl_nosort = gEngfuncs.Cvar_Get( "gl_nosort", "0", FCVAR_ARCHIVE, "disable sorting of translucent surfaces" );

	//gEngfuncs.Cmd_AddCommand( "r_info", R_RenderInfo_f, "display renderer info" );
	//gEngfuncs.Cmd_AddCommand( "timerefresh", SCR_TimeRefresh_f, "turn quickly and print rendering statistcs" );
}

void GL_RemoveCommands( void )
{
	
}

//STUB
static void GL_SetDefaultState( void )
{
	memset( &vkState, 0, sizeof( vkState ));
	Vector4Set( vkState.color, 1,1,1,1 );
	//GL_SetDefaultTexState ();

	// init draw stack
	tr.draw_list = &tr.draw_stack[0];
	tr.draw_stack_pos = 0;
}

//STUB
qboolean R_Init( void )
{
	GL_InitCommands();
	
	GL_SetDefaultState();
	
	if( !gEngfuncs.R_Init_Video( REF_VULKAN ))
	{
		gEngfuncs.R_Free_Video();

		gEngfuncs.Host_Error( "Can't initialize video subsystem\nProbably driver was not installed" );
		return false;
	}
	
	r_temppool = Mem_AllocPool( "Render Zone" );

	VK_GetPhysicalDevice();
	VK_CreateSurface();
	VK_CreateDevice();
	VK_CreateSwapchain();
	VK_CreateCommandPool(physicalDevice.queueFamilyIndex);
	renderCommandBuffer = VK_AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VK_CreateRenderPass();
	testSampler = VK_CreateSampler();
	VK_CreatePipelines();
	VK_CreateFrameResources();
	VK_CreateDescriptorPool();
	
	//TODO staging buffer
	//VK_BUFFER_USAGE_TRANSFER_DST_BIT
	vertexBufferQuad = VK_CreateBuffer(4 * 1024 * 1024 * sizeof(testVert_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(device, vertexBufferQuad.memory, 0, vertexBufferQuad.size, 0, &vertexBufferMapped);
	tempQuadVerts = (testVert_t*)vertexBufferMapped;
	currentVert = 0;
	
	//VK_Test();
	
	//GL_SetDefaults();
	//R_CheckVBO();
	R_InitImages();
	R_SpriteInit();
	R_StudioInit();
	//R_AliasInit();
	R_ClearDecals();
	R_ClearScene();

	return true;
}


void R_Shutdown( void )
{
	GL_RemoveCommands();
	R_ShutdownImages();
	
	VK_Shutdown();
	
	Mem_FreePool( &r_temppool );
	
	gEngfuncs.R_Free_Video();
}
