#pragma once

#include "gpu/dll.h"
#include "gpu/backend.h"
#include "gpu_vlk/vlk_command_list.h"
#include "gpu_vlk/vlk_compile_context.h"
#include "gpu_vlk/vlk_resources.h"

#include "core/concurrency.h"
#include "core/library.h"
#include "core/vector.h"

namespace GPU
{
#define VK_INSTANCE_FUNC( func ) extern PFN_##func #func = nullptr;
	VK_INSTANCE_FUNCS
	VK_PLATFORM_FUNCS
#undef VK_INSTANCE_FUNC

	class VLKBackend : public IBackend
	{
	public:
		VLKBackend(void* deviceWindow);
		~VLKBackend();

		/**
		 * device operations.
		 */
		i32 EnumerateAdapters(AdapterInfo* outAdapters, i32 maxAdapters) override;
		bool IsInitialized() const override;
		ErrorCode Initialize(i32 adapterIdx) override;

		/**
		 * Resource creation/destruction.
		 */
		ErrorCode CreateSwapChain(Handle handle, const SwapChainDesc& desc, const char* debugName) override;
		ErrorCode CreateBuffer(
		    Handle handle, const BufferDesc& desc, const void* initialData, const char* debugName) override;
		ErrorCode CreateTexture(Handle handle, const TextureDesc& desc, const TextureSubResourceData* initialData,
		    const char* debugName) override;
		ErrorCode CreateSamplerState(Handle handle, const SamplerState& state, const char* debugName) override;
		ErrorCode CreateShader(Handle handle, const ShaderDesc& desc, const char* debugName) override;
		ErrorCode CreateGraphicsPipelineState(
		    Handle handle, const GraphicsPipelineStateDesc& desc, const char* debugName) override;
		ErrorCode CreateComputePipelineState(
		    Handle handle, const ComputePipelineStateDesc& desc, const char* debugName) override;
		ErrorCode CreatePipelineBindingSet(
		    Handle handle, const PipelineBindingSetDesc& desc, const char* debugName) override;
		ErrorCode CreateDrawBindingSet(Handle handle, const DrawBindingSetDesc& desc, const char* debugName) override;
		ErrorCode CreateFrameBindingSet(Handle handle, const FrameBindingSetDesc& desc, const char* debugName) override;
		ErrorCode CreateCommandList(Handle handle, const char* debugName) override;
		ErrorCode CreateFence(Handle handle, const char* debugName) override;
		ErrorCode DestroyResource(Handle handle) override;

		ErrorCode CompileCommandList(Handle handle, const CommandList& commandList) override;
		ErrorCode SubmitCommandList(Handle handle) override;
		
		ErrorCode PresentSwapChain(Handle handle) override;
		ErrorCode ResizeSwapChain(Handle handle, i32 width, i32 height) override;

		void NextFrame() override;

		/**
		 * Load Vulkan Instance functions via vkGetInstanceProcAddr that are dependent upon a valid VkInstance.
		*/
		ErrorCode LoadVLKInstanceFunctions();

		/**
		 * Will return VLKBuffer for a @a handle.
		 * Supports BUFFER.
		 * Not thread safe.
		 */
		VLKBuffer* GetVLKBuffer(Handle handle);

		/**
		 * Will return VLKTexture for a @a handle.
		 * Supports TEXTURE and SWAP_CHAIN.
		 * Not thread safe.
		 * @param bufferIdx If >= 0, will return appropriate buffer for swapchain.
		 */
		VLKTexture* GetVLKTexture(Handle handle, i32 bufferIdx = -1);

		VkInstance vkInstance_;

		Core::Vector<VkLayerProperties> instanceLayerProperties_;
		Core::Vector<VkExtensionProperties> instanceExtensionProperties_;

		/// Cached adapter infos.
		Core::Vector<VkPhysicalDevice> physicalDevices_;
		Core::Vector<GPU::AdapterInfo> adapterInfos_;

		/// Vulkan devices.
		class VLKDevice* device_ = nullptr;

		/// Resources.
		Core::RWLock resLock_;
		ResourceVector<VLKSwapChain> swapchainResources_;
		ResourceVector<VLKBuffer> bufferResources_;
		ResourceVector<VLKTexture> textureResources_;
		ResourceVector<VLKShader> shaders_;
		ResourceVector<VLKSamplerState> samplerStates_;
		ResourceVector<VLKGraphicsPipelineState> graphicsPipelineStates_;
		ResourceVector<VLKComputePipelineState> computePipelineStates_;
		ResourceVector<VLKPipelineBindingSet> pipelineBindingSets_;
		ResourceVector<VLKDrawBindingSet> drawBindingSets_;
		ResourceVector<VLKFrameBindingSet> frameBindingSets_;
		ResourceVector<VLKCommandList*> commandLists_;
	};
} // namespace GPU
