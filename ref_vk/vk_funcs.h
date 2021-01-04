
#ifndef VK_GLOBAL_FUNCTION
#define VK_GLOBAL_FUNCTION( f )
#endif

#ifndef VK_FUNCTION
#define VK_FUNCTION( f )
#endif

#ifndef VK_EXT_FUNCTION
#define VK_EXT_FUNCTION( f )
#endif

VK_GLOBAL_FUNCTION(vkGetInstanceProcAddr)
VK_GLOBAL_FUNCTION(vkEnumerateInstanceVersion)
VK_GLOBAL_FUNCTION(vkCreateInstance)
VK_GLOBAL_FUNCTION(vkDestroyInstance)
VK_GLOBAL_FUNCTION(vkEnumerateInstanceExtensionProperties)
VK_GLOBAL_FUNCTION(vkEnumerateInstanceLayerProperties)

VK_FUNCTION(vkEnumeratePhysicalDevices)
VK_FUNCTION(vkGetPhysicalDeviceProperties)
VK_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
VK_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
VK_FUNCTION(vkGetPhysicalDeviceFeatures)
VK_FUNCTION(vkGetPhysicalDeviceFormatProperties)
VK_FUNCTION(vkEnumerateDeviceExtensionProperties)
VK_FUNCTION(vkCreateDevice)
VK_FUNCTION(vkDestroyDevice)
VK_FUNCTION(vkDeviceWaitIdle)

VK_FUNCTION(vkGetDeviceQueue)
VK_FUNCTION(vkQueueSubmit)
VK_FUNCTION(vkQueueWaitIdle)

VK_FUNCTION(vkCreateImage)
VK_FUNCTION(vkDestroyImage)
VK_FUNCTION(vkGetImageMemoryRequirements)
VK_FUNCTION(vkBindImageMemory)
VK_FUNCTION(vkCreateImageView)
VK_FUNCTION(vkDestroyImageView)

VK_FUNCTION(vkCreateShaderModule)
VK_FUNCTION(vkDestroyShaderModule)

VK_FUNCTION(vkCreateDescriptorSetLayout)
VK_FUNCTION(vkDestroyDescriptorSetLayout)

VK_FUNCTION(vkCreateDescriptorPool)
VK_FUNCTION(vkDestroyDescriptorPool)
VK_FUNCTION(vkAllocateDescriptorSets)
VK_FUNCTION(vkFreeDescriptorSets)
VK_FUNCTION(vkResetDescriptorPool)
VK_FUNCTION(vkUpdateDescriptorSets)

VK_FUNCTION(vkCreatePipelineLayout)
VK_FUNCTION(vkDestroyPipelineLayout)

VK_FUNCTION(vkCreateRenderPass)
VK_FUNCTION(vkDestroyRenderPass)

VK_FUNCTION(vkCreatePipelineCache)
VK_FUNCTION(vkDestroyPipelineCache)
VK_FUNCTION(vkGetPipelineCacheData)
VK_FUNCTION(vkCreateGraphicsPipelines)
VK_FUNCTION(vkCreateComputePipelines)
VK_FUNCTION(vkDestroyPipeline)

VK_FUNCTION(vkCreateSampler)
VK_FUNCTION(vkDestroySampler)

VK_FUNCTION(vkCreateFramebuffer)
VK_FUNCTION(vkDestroyFramebuffer)

VK_FUNCTION(vkCreateSemaphore)
VK_FUNCTION(vkDestroySemaphore)
VK_FUNCTION(vkCreateFence)
VK_FUNCTION(vkDestroyFence)
VK_FUNCTION(vkWaitForFences)
VK_FUNCTION(vkResetFences)

VK_FUNCTION(vkCreateCommandPool)
VK_FUNCTION(vkResetCommandPool)
VK_FUNCTION(vkDestroyCommandPool)
VK_FUNCTION(vkAllocateCommandBuffers)
VK_FUNCTION(vkResetCommandBuffer)
VK_FUNCTION(vkFreeCommandBuffers)
VK_FUNCTION(vkBeginCommandBuffer)
VK_FUNCTION(vkEndCommandBuffer)

VK_FUNCTION(vkCmdBeginRenderPass)
VK_FUNCTION(vkCmdEndRenderPass)
VK_FUNCTION(vkCmdBindPipeline)
VK_FUNCTION(vkCmdBindVertexBuffers)
VK_FUNCTION(vkCmdBindIndexBuffer)
VK_FUNCTION(vkCmdBindDescriptorSets)
VK_FUNCTION(vkCmdDraw)
VK_FUNCTION(vkCmdDrawIndexed)
VK_FUNCTION(vkCmdCopyBuffer)
VK_FUNCTION(vkCmdCopyBufferToImage)
VK_FUNCTION(vkCmdPipelineBarrier)
VK_FUNCTION(vkCmdPushConstants)
VK_FUNCTION(vkCmdDispatch)
VK_FUNCTION(vkCmdSetViewport)
VK_FUNCTION(vkCmdSetScissor)

VK_FUNCTION(vkCreateBuffer)
VK_FUNCTION(vkDestroyBuffer)
VK_FUNCTION(vkGetBufferMemoryRequirements)
VK_FUNCTION(vkBindBufferMemory)

VK_FUNCTION(vkAllocateMemory)
VK_FUNCTION(vkFreeMemory)
VK_FUNCTION(vkMapMemory)
VK_FUNCTION(vkUnmapMemory)
VK_FUNCTION(vkFlushMappedMemoryRanges)
VK_FUNCTION(vkInvalidateMappedMemoryRanges)


//exts
//VK_KHR_surface
VK_EXT_FUNCTION(vkDestroySurfaceKHR)
VK_EXT_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)
VK_EXT_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
VK_EXT_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
VK_EXT_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)
VK_EXT_FUNCTION(vkGetPhysicalDevicePresentRectanglesKHR)
#ifdef ANDROID
//VK_KHR_android_surface
VK_EXT_FUNCTION(vkCreateAndroidSurfaceKHR)
#endif
//VK_KHR_swapchain
VK_EXT_FUNCTION(vkCreateSwapchainKHR)
VK_EXT_FUNCTION(vkDestroySwapchainKHR)
VK_EXT_FUNCTION(vkGetSwapchainImagesKHR)
VK_EXT_FUNCTION(vkAcquireNextImageKHR)
VK_EXT_FUNCTION(vkQueuePresentKHR)
//VK_EXT_debug_utils
VK_EXT_FUNCTION(vkCreateDebugUtilsMessengerEXT)
VK_EXT_FUNCTION(vkDestroyDebugUtilsMessengerEXT)
//VK_EXT_debug_report
VK_EXT_FUNCTION(vkCreateDebugReportCallbackEXT)
VK_EXT_FUNCTION(vkDestroyDebugReportCallbackEXT)
VK_EXT_FUNCTION(vkDebugReportMessageEXT)

#undef VK_FUNCTION
#undef VK_GLOBAL_FUNCTION
#undef VK_EXT_FUNCTION
