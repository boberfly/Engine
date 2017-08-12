#pragma once

#include "core/types.h"
#include "core/library.h"
#include "gpu/resources.h"
#include "gpu/types.h"

#if PLATFORM_ANDROID
#	define VK_USE_PLATFORM_ANDROID_KHR
#	define KHR_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif PLATFORM_LINUX
//#	define VK_USE_PLATFORM_MIR_KHR
#	define VK_USE_PLATFORM_XLIB_KHR
#	define VK_USE_PLATFORM_XCB_KHR
//#	define VK_USE_PLATFORM_WAYLAND_KHR
// SDL still uses xlib as default on Linux
#	define KHR_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif PLATFORM_WINDOWS
#	define VK_USE_PLATFORM_WIN32_KHR
#	define KHR_PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#else
#	define KHR_PLATFORM_SURFACE_EXTENSION_NAME ""
#endif

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define VK_GLOBAL_FUNCS \
	VK_GLOBAL_FUNC(vkCreateInstance) \
	VK_GLOBAL_FUNC(vkEnumerateInstanceExtensionProperties) \
	VK_GLOBAL_FUNC(vkEnumerateInstanceLayerProperties)

#if PLATFORM_WINDOWS
#define VK_PLATFORM_FUNCS \
	VK_INSTANCE_FUNC(vkCreateWin32SurfaceKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceWin32PresentationSupportKHR)
#elif PLATFORM_ANDROID
#define VK_PLATFORM_FUNCS \
	VK_INSTANCE_FUNC(vkCreateAndroidSurfaceKHR)
#elif PLATFORM_LINUX
#define VK_PLATFORM_FUNCS \
	VK_INSTANCE_FUNC(vkCreateXlibSurfaceKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceXlibPresentationSupportKHR) \
	VK_INSTANCE_FUNC(vkCreateXcbSurfaceKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceXcbPresentationSupportKHR) \
	VK_INSTANCE_FUNC(vkCreateWaylandSurfaceKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceWaylandPresentationSupportKHR) \
	VK_INSTANCE_FUNC(vkCreateMirSurfaceKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceMirPresentationSupportKHR)
#else
#define VK_PLATFORM_FUNCS
#endif

#define VK_INSTANCE_FUNCS \
	VK_INSTANCE_FUNC(vkGetDeviceProcAddr) \
	VK_INSTANCE_FUNC(vkDestroyInstance) \
	VK_INSTANCE_FUNC(vkEnumeratePhysicalDevices) \
	VK_INSTANCE_FUNC(vkEnumerateDeviceExtensionProperties) \
	VK_INSTANCE_FUNC(vkEnumerateDeviceLayerProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceFormatProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceImageFormatProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR) \
	VK_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR) \
	VK_INSTANCE_FUNC(vkCreateDevice) \
	VK_INSTANCE_FUNC(vkDestroyDevice) \
	VK_INSTANCE_FUNC(vkDestroySurfaceKHR) \
	/* VK_EXT_debug_report */
	VK_INSTANCE_FUNC(vkCreateDebugReportCallbackEXT) \
	VK_INSTANCE_FUNC(vkDestroyDebugReportCallbackEXT) \
	VK_INSTANCE_FUNC(vkDebugReportMessageEXT)

#define VK_DEVICE_FUNCS \
	VK_DEVICE_FUNC(vkGetDeviceQueue) \
	VK_DEVICE_FUNC(vkCreateSwapchainKHR) \
	VK_DEVICE_FUNC(vkDestroySwapchainKHR) \
	VK_DEVICE_FUNC(vkGetSwapchainImagesKHR) \
	VK_DEVICE_FUNC(vkAcquireNextImageKHR) \
	VK_DEVICE_FUNC(vkQueuePresentKHR) \
	VK_DEVICE_FUNC(vkCreateFence) \
	VK_DEVICE_FUNC(vkDestroyFence) \
	VK_DEVICE_FUNC(vkCreateSemaphore) \
	VK_DEVICE_FUNC(vkDestroySemaphore) \
	VK_DEVICE_FUNC(vkResetFences) \
	VK_DEVICE_FUNC(vkCreateCommandPool) \
	VK_DEVICE_FUNC(vkDestroyCommandPool) \
	VK_DEVICE_FUNC(vkResetCommandPool) \
	VK_DEVICE_FUNC(vkAllocateCommandBuffers) \
	VK_DEVICE_FUNC(vkFreeCommandBuffers) \
	VK_DEVICE_FUNC(vkGetBufferMemoryRequirements) \
	VK_DEVICE_FUNC(vkGetImageMemoryRequirements) \
	VK_DEVICE_FUNC(vkAllocateMemory) \
	VK_DEVICE_FUNC(vkFreeMemory) \
	VK_DEVICE_FUNC(vkCreateImage) \
	VK_DEVICE_FUNC(vkDestroyImage) \
	VK_DEVICE_FUNC(vkCreateImageView) \
	VK_DEVICE_FUNC(vkDestroyImageView) \
	VK_DEVICE_FUNC(vkCreateBuffer) \
	VK_DEVICE_FUNC(vkDestroyBuffer) \
	VK_DEVICE_FUNC(vkCreateFramebuffer) \
	VK_DEVICE_FUNC(vkDestroyFramebuffer) \
	VK_DEVICE_FUNC(vkCreateRenderPass) \
	VK_DEVICE_FUNC(vkDestroyRenderPass) \
	VK_DEVICE_FUNC(vkCreateShaderModule) \
	VK_DEVICE_FUNC(vkDestroyShaderModule) \
	VK_DEVICE_FUNC(vkCreatePipelineCache) \
	VK_DEVICE_FUNC(vkDestroyPipelineCache) \
	VK_DEVICE_FUNC(vkGetPipelineCacheData) \
	VK_DEVICE_FUNC(vkMergePipelineCaches) \
	VK_DEVICE_FUNC(vkCreateGraphicsPipelines) \
	VK_DEVICE_FUNC(vkCreateComputePipelines) \
	VK_DEVICE_FUNC(vkDestroyPipeline) \
	VK_DEVICE_FUNC(vkCreatePipelineLayout) \
	VK_DEVICE_FUNC(vkDestroyPipelineLayout) \
	VK_DEVICE_FUNC(vkCreateSampler) \
	VK_DEVICE_FUNC(vkDestroySampler) \
	VK_DEVICE_FUNC(vkCreateDescriptorSetLayout) \
	VK_DEVICE_FUNC(vkDestroyDescriptorSetLayout) \
	VK_DEVICE_FUNC(vkCreateDescriptorPool) \
	VK_DEVICE_FUNC(vkDestroyDescriptorPool) \
	VK_DEVICE_FUNC(vkResetDescriptorPool) \
	VK_DEVICE_FUNC(vkAllocateDescriptorSets) \
	VK_DEVICE_FUNC(vkFreeDescriptorSets) \
	VK_DEVICE_FUNC(vkUpdateDescriptorSets) \
	VK_DEVICE_FUNC(vkQueueSubmit) \
	VK_DEVICE_FUNC(vkQueueWaitIdle) \
	VK_DEVICE_FUNC(vkDeviceWaitIdle) \
	VK_DEVICE_FUNC(vkWaitForFences) \
	VK_DEVICE_FUNC(vkBeginCommandBuffer) \
	VK_DEVICE_FUNC(vkEndCommandBuffer) \
	VK_DEVICE_FUNC(vkCmdPipelineBarrier) \
	VK_DEVICE_FUNC(vkCmdBeginQuery) \
	VK_DEVICE_FUNC(vkCmdEndQuery) \
	VK_DEVICE_FUNC(vkCmdResetQueryPool) \
	VK_DEVICE_FUNC(vkCmdWriteTimestamp) \
	VK_DEVICE_FUNC(vkCmdCopyQueryPoolResults) \
	VK_DEVICE_FUNC(vkCmdPushConstants) \
	VK_DEVICE_FUNC(vkCmdBeginRenderPass) \
	VK_DEVICE_FUNC(vkCmdEndRenderPass) \
	VK_DEVICE_FUNC(vkCmdSetViewport) \
	VK_DEVICE_FUNC(vkCmdDraw) \
	VK_DEVICE_FUNC(vkCmdDrawIndexed) \
	VK_DEVICE_FUNC(vkCmdDrawIndirect) \
	VK_DEVICE_FUNC(vkCmdDrawIndexedIndirect) \
	VK_DEVICE_FUNC(vkCmdDispatch) \
	VK_DEVICE_FUNC(vkCmdDispatchIndirect) \
	VK_DEVICE_FUNC(vkCmdBindPipeline) \
	VK_DEVICE_FUNC(vkCmdSetStencilReference) \
	VK_DEVICE_FUNC(vkCmdSetBlendConstants) \
	VK_DEVICE_FUNC(vkCmdSetScissor) \
	VK_DEVICE_FUNC(vkCmdBindDescriptorSets) \
	VK_DEVICE_FUNC(vkCmdBindIndexBuffer) \
	VK_DEVICE_FUNC(vkCmdBindVertexBuffers) \
	VK_DEVICE_FUNC(vkCmdUpdateBuffer) \
	VK_DEVICE_FUNC(vkCmdClearColorImage) \
	VK_DEVICE_FUNC(vkCmdClearDepthStencilImage) \
	VK_DEVICE_FUNC(vkCmdClearAttachments) \
	VK_DEVICE_FUNC(vkCmdResolveImage) \
	VK_DEVICE_FUNC(vkMapMemory) \
	VK_DEVICE_FUNC(vkUnmapMemory) \
	VK_DEVICE_FUNC(vkFlushMappedMemoryRanges) \
	VK_DEVICE_FUNC(vkInvalidateMappedMemoryRanges) \
	VK_DEVICE_FUNC(vkBindBufferMemory) \
	VK_DEVICE_FUNC(vkBindImageMemory) \
	/* VK_EXT_debug_marker */
	VK_DEVICE_FUNC(vkDebugMarkerSetObjectTagEXT) \
	VK_DEVICE_FUNC(vkDebugMarkerSetObjectNameEXT) \
	VK_DEVICE_FUNC(vkCmdDebugMarkerBeginEXT) \
	VK_DEVICE_FUNC(vkCmdDebugMarkerEndEXT) \
	VK_DEVICE_FUNC(vkCmdDebugMarkerInsertEXT)

namespace GPU
{
	/**
	 * Library handling.
	 */
	extern Core::LibHandle VLKHandle = 0;
	extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#define VK_GLOBAL_FUNC( func ) extern PFN_##func func = nullptr;
	VK_GLOBAL_FUNCS
#undef VK_GLOBAL_FUNC

	ErrorCode LoadLibraries();

	/**
	 * Enums.
	 */
	enum class RootSignatureType
	{
		INVALID = -1,
		GRAPHICS,
		COMPUTE,
		MAX
	};

	enum class DescriptorHeapSubType : i32
	{
		INVALID = -1,
		CBV = 0,
		SRV,
		UAV,
		SAMPLER,
		RTV,
		DSV,
	};


	/**
	 * Conversion.
	 */
	VkBufferUsageFlags GetBufferUsageFlags(BindFlags bindFlags);
	VkImageUsageFlags GetImageUsageFlags(BindFlags bindFlags);
	VkImageType GetImageType(TextureType type);
	VkImageViewType GetImageViewType(ViewDimension dim);
	VkFormat GetFormat(Format format);
	VkPrimitiveTopology GetPrimitiveTopology(PrimitiveTopology topology);
	VkBufferCreateInfo GetBufferInfo(const BufferDesc& desc);
	VkImageCreateInfo GetImageInfo(const TextureDesc& desc);

	/**
	 * Utility.
	 */
	void SetObjectName(VkDebugReportObjectTypeEXT objectType, u64 object, const char* name);
	void WaitOnFence(VkDevice device, VkFence fence);


} // namespace GPU

#ifdef DEBUG
#define CHECK_ERRORCODE(a) DBG_ASSERT((a) == ErrorCode::OK)
#define CHECK_VLK(a) DBG_ASSERT((a) == VK_SUCCESS)
#else
#define CHECK_ERRORCODE(a)
#define CHECK_VLK(a) a
#endif

