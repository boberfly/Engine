#pragma once

#include "gpu/dll.h"
#include "gpu/backend.h"
#include "gpu_tf/tf_command_list.h"
#include "gpu_tf/tf_compile_context.h"
#include "gpu_tf/tf_resources.h"

#include "core/concurrency.h"
#include "core/library.h"
#include "core/vector.h"

namespace GPU
{
	class TFBackend : public GPU::IBackend
	{
	public:
		TFBackend(const GPU::SetupParams& setupParams);
		~TFBackend();

		/**
		 * device operations.
		 */
		i32 EnumerateAdapters(GPU::AdapterInfo* outAdapters, i32 maxAdapters) override;
		bool IsInitialized() const override;
		GPU::ErrorCode Initialize(i32 adapterIdx) override;

		/**
		 * Resource creation/destruction.
		 */
		GPU::ErrorCode CreateSwapChain(GPU::Handle handle, const GPU::SwapChainDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateBuffer(
			GPU::Handle handle, const GPU::BufferDesc& desc, const void* initialData, const char* debugName) override;
		GPU::ErrorCode CreateTexture(GPU::Handle handle, const GPU::TextureDesc& desc, const GPU::TextureSubResourceData* initialData,
			const char* debugName) override;
		GPU::ErrorCode CreateShader(GPU::Handle handle, const GPU::ShaderDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateRootSignature(GPU::Handle handle, const GPU::RootSignatureDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateGraphicsPipelineState(
			GPU::Handle handle, const GPU::GraphicsPipelineStateDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateComputePipelineState(
			GPU::Handle handle, const GPU::ComputePipelineStateDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreatePipelineBindingSet(
			GPU::Handle handle, const GPU::PipelineBindingSetDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateDrawBindingSet(GPU::Handle handle, const GPU::DrawBindingSetDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateFrameBindingSet(GPU::Handle handle, const GPU::FrameBindingSetDesc& desc, const char* debugName) override;
		GPU::ErrorCode CreateCommandList(GPU::Handle handle, const char* debugName) override;
		GPU::ErrorCode CreateFence(GPU::Handle handle, const char* debugName) override;
		GPU::ErrorCode CreateSemaphore(GPU::Handle handle, const char* debugName) override;
		GPU::ErrorCode DestroyResource(GPU::Handle handle) override;

		GPU::ErrorCode AllocTemporaryPipelineBindingSet(GPU::Handle handle, const GPU::PipelineBindingSetDesc& desc) override;
		GPU::ErrorCode UpdatePipelineBindings(GPU::Handle pbs, i32 base, Core::ArrayView<const GPU::BindingCBV> descs) override;
		GPU::ErrorCode UpdatePipelineBindings(GPU::Handle pbs, i32 base, Core::ArrayView<const GPU::BindingSRV> descs) override;
		GPU::ErrorCode UpdatePipelineBindings(GPU::Handle pbs, i32 base, Core::ArrayView<const GPU::BindingUAV> descs) override;
		GPU::ErrorCode UpdatePipelineBindings(GPU::Handle pbs, i32 base, Core::ArrayView<const GPU::SamplerState> descs) override;
		GPU::ErrorCode CopyPipelineBindings(
			Core::ArrayView<const GPU::PipelineBinding> dst, Core::ArrayView<const GPU::PipelineBinding> src) override;
		GPU::ErrorCode ValidatePipelineBindings(Core::ArrayView<const GPU::PipelineBinding> pb) override;

		GPU::ErrorCode CompileCommandList(GPU::Handle handle, const GPU::CommandList& commandList) override;
		GPU::ErrorCode SubmitCommandLists(Core::ArrayView<GPU::Handle> handles) override;

		GPU::ErrorCode PresentSwapChain(GPU::Handle handle) override;
		GPU::ErrorCode ResizeSwapChain(GPU::Handle handle, i32 width, i32 height) override;

		void NextFrame() override;

		void CreateStaticSamplers();
		void CreateCommandQueues();
		void CreateCommandSignatures();

		/**
		 * Will return TFResource for a @a handle.
		 * Supports BUFFER, TEXTURE, and SWAP_CHAIN.
		 */
		GPU::ResourceRead<TFResource> GetTFResource(GPU::Handle handle);

		/**
		 * Will return TFBuffer for a @a handle.
		 * Supports BUFFER.
		 */
		GPU::ResourceRead<TFBuffer> GetTFBuffer(GPU::Handle handle);

		/**
		 * Will return TFResource for a @a handle.
		 * Supports TEXTURE and SWAP_CHAIN.
		 * @param bufferIdx If >= 0, will return appropriate buffer for swapchain.
		 */
		GPU::ResourceRead<TFTexture> GetTFTexture(GPU::Handle handle, i32 bufferIdx = -1);

		const GPU::SetupParams& setupParams_;

		/// Cached adapter infos.
		Core::Vector<GPU::AdapterInfo> adapterInfos_;

		/// The-Forge Renderer.
		::Renderer* renderer_ = nullptr;

		/// Command queues.
		::Queue* tfDirectQueue_;       // direct
		::Queue* tfAsyncComputeQueue_; // compute

		/// Frame semaphore.
		::Semaphore* frameSemaphore_;

		/// Resources.
		GPU::ResourcePool<TFSwapChain> swapchainResources_;
		GPU::ResourcePool<TFBuffer> bufferResources_;
		GPU::ResourcePool<TFTexture> textureResources_;
		GPU::ResourcePool<TFShader> shaders_;
		GPU::ResourcePool<TFRootSignature> rootSignatures_;
		GPU::ResourcePool<TFGraphicsPipelineState> graphicsPipelineStates_;
		GPU::ResourcePool<TFComputePipelineState> computePipelineStates_;
		GPU::ResourcePool<TFPipelineBindingSet> pipelineBindingSets_;
		GPU::ResourcePool<TFDrawBindingSet> drawBindingSets_;
		GPU::ResourcePool<TFFrameBindingSet> frameBindingSets_;
		GPU::ResourcePool<TFCmd> commandLists_;
		GPU::ResourcePool<TFFence> fences_;
		GPU::ResourcePool<TFSemaphore> semaphores_;

		/// Static samplers.
		Core::Vector<::Sampler*> tfStaticSamplers_;

		/// Command signatures
		::CommandSignature* tfDrawCmdSig_;
		::CommandSignature* tfDrawIndexedCmdSig_;
		::CommandSignature* tfDispatchCmdSig_;
	};

} // namespace GPU