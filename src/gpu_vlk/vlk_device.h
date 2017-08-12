#pragma once

#include "gpu_vlk/vlk_types.h"
#include "gpu_vlk/vlk_command_list.h"
#include "gpu_vlk/vlk_resources.h"
#include "gpu/resources.h"
#include "core/array.h"
#include "core/concurrency.h"

namespace GPU
{
	class VLKDevice
	{
	public:
		VLKDevice(VkInstance instance, VkPhysicalDevice device);
		~VLKDevice();

		ErrorCode LoadVLKDeviceFunctions();

		void CreateCommandQueues();
		void CreateRootSignatures();
		void CreateDefaultPSOs();
		void CreateUploadAllocators();
		void CreateDescriptorHeapAllocators();

		void NextFrame();

		ErrorCode CreateSwapChain(VLKSwapChain& outResource, const SwapChainDesc& desc, const char* debugName);
		ErrorCode CreateBuffer(
		    VLKBuffer& outBuffer, const BufferDesc& desc, const void* initialData, const char* debugName);
		ErrorCode CreateTexture(VLKTexture& outTexture, const TextureDesc& desc,
		    const TextureSubResourceData* initialData, const char* debugName);

		ErrorCode CreateShader(VLKShader& outShader, const ShaderDesc& desc, const char* debugName);

		ErrorCode CreateGraphicsPipelineState(
		    VLKGraphicsPipelineState& outGps, D3D12_GRAPHICS_PIPELINE_STATE_DESC desc, const char* debugName);
		ErrorCode CreateComputePipelineState(
		    VLKComputePipelineState& outCps, D3D12_COMPUTE_PIPELINE_STATE_DESC desc, const char* debugName);

		ErrorCode CreatePipelineBindingSet(
		    VLKPipelineBindingSet& outPipelineBindingSet, const PipelineBindingSetDesc& desc, const char* debugName);
		void DestroyPipelineBindingSet(VLKPipelineBindingSet& pipelineBindingSet);

		ErrorCode CreateFrameBindingSet(
		    VLKFrameBindingSet& outFrameBindingSet, const FrameBindingSetDesc& desc, const char* debugName);
		void DestroyFrameBindingSet(VLKFrameBindingSet& frameBindingSet);

		ErrorCode UpdateSRVs(VLKPipelineBindingSet& pipelineBindingSet, i32 first, i32 num,
		    ID3D12Resource** resources, const D3D12_SHADER_RESOURCE_VIEW_DESC* descs);
		ErrorCode UpdateUAVs(VLKPipelineBindingSet& pipelineBindingSet, i32 first, i32 num,
		    ID3D12Resource** resources, const D3D12_UNORDERED_ACCESS_VIEW_DESC* descs);
		ErrorCode UpdateCBVs(VLKPipelineBindingSet& pipelineBindingSet, i32 first, i32 num,
		    const D3D12_CONSTANT_BUFFER_VIEW_DESC* descs);
		ErrorCode UpdateSamplers(
		    VLKPipelineBindingSet& pipelineBindingSet, i32 first, i32 num, const D3D12_SAMPLER_DESC* descs);
		ErrorCode UpdateFrameBindingSet(VLKFrameBindingSet& frameBindingSet,
		    const D3D12_RENDER_TARGET_VIEW_DESC* rtvDescs, const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDescs);

		ErrorCode SubmitCommandList(D3D12CommandList& commandList);


		operator bool() const { return !!vkDevice_; }

		Core::Vector<VkLayerProperties> deviceLayerProperties_;
		Core::Vector<VkExtensionProperties> deviceExtensionProperties_;
		Core::Vector<VkQueueFamilyProperties> queueProperties_;
		VkPhysicalDeviceFeatures deviceFeatures_;
		VkPhysicalDeviceProperties deviceProperties_;
		VkPhysicalDeviceMemoryProperties memoryProperties_;

		VkDevice vkDevice_;

		Core::Vector<VkQueue> vkDirectQueues_;
		Core::Vector<VkQueue> vkCopyQueues_;
		Core::Vector<VkQueue> vkAsyncComputeQueues_;

		/// Frame counter.
		i64 frameIdx_ = 0;
		VkFence vkFrameFence_;
		HANDLE frameFenceEvent_ = 0;

		/// Upload management.
		Core::Mutex uploadMutex_;
		Core::Array<class D3D12LinearHeapAllocator*, MAX_GPU_FRAMES> uploadAllocators_;
		class VLKCommandList* uploadCommandList_;
		VkFence vkUploadFence_;
		HANDLE uploadFenceEvent_ = 0;
		volatile i64 uploadFenceIdx_ = 0;

		/// Descriptor heap allocators.
		class VLKDescriptorHeapAllocator* cbvSrvUavAllocator_ = nullptr;
		class VLKDescriptorHeapAllocator* samplerAllocator_ = nullptr;
		class VLKDescriptorHeapAllocator* rtvAllocator_ = nullptr;
		class VLKDescriptorHeapAllocator* dsvAllocator_ = nullptr;

		VLKLinearHeapAllocator& GetUploadAllocator() { return *uploadAllocators_[frameIdx_ % MAX_GPU_FRAMES]; }

		/// Root signatures.
		Core::Vector<ComPtr<ID3D12RootSignature>> d3dRootSignatures_;

		Core::Vector<ComPtr<ID3D12PipelineState>> d3dDefaultPSOs_;

	private:
#define VK_DEVICE_FUNC( func ) PFN_##func func = nullptr;
		VK_DEVICE_FUNCS
#undef VK_DEVICE_FUNC
	};
} // namespace GPU
