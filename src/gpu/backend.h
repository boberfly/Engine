#pragma once

#include "gpu/dll.h"
#include "gpu/command_list.h"
#include "gpu/resources.h"
#include "core/array_view.h"
#include "core/types.h"
#include "core/handle.h"
#include "plugin/plugin.h"

namespace GPU
{
	class IBackend
	{
	public:
		virtual ~IBackend() {}

		/**
		 * Device operations.
		 */
		virtual i32 EnumerateAdapters(AdapterInfo* outAdapters, i32 maxAdapters) = 0;

		virtual bool IsInitialized() const = 0;
		virtual ErrorCode Initialize(i32 adapterIdx) = 0;

		/**
		 * Resource creation/destruction.
		 */
		virtual ErrorCode CreateSwapChain(Handle handle, const SwapChainDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateBuffer(
		    Handle handle, const BufferDesc& desc, const void* initialData, const char* debugName) = 0;
		virtual ErrorCode CreateTexture(Handle handle, const TextureDesc& desc,
		    const TextureSubResourceData* initialData, const char* debugName) = 0;
		virtual ErrorCode CreateShader(Handle handle, const ShaderDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateRootSignature(Handle handle, const RootSignatureDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateGraphicsPipelineState(
		    Handle handle, const GraphicsPipelineStateDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateComputePipelineState(
		    Handle handle, const ComputePipelineStateDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreatePipelineBindingSet(
		    Handle handle, const PipelineBindingSetDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateDrawBindingSet(
		    Handle handle, const DrawBindingSetDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateFrameBindingSet(
		    Handle handle, const FrameBindingSetDesc& desc, const char* debugName) = 0;
		virtual ErrorCode CreateCommandList(Handle handle, const char* debugName) = 0;
		virtual ErrorCode CreateFence(Handle handle, const char* debugName) = 0;
		//virtual ErrorCode CreateSemaphore(Handle handle, const char* debugName) = 0;
		virtual ErrorCode DestroyResource(Handle handle) = 0;

		/**
		 * Binding management.
		 */
		virtual ErrorCode AllocTemporaryPipelineBindingSet(Handle handle, const PipelineBindingSetDesc& desc) = 0;
		virtual ErrorCode UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingCBV> descs) = 0;
		virtual ErrorCode UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingSRV> descs) = 0;
		virtual ErrorCode UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingUAV> descs) = 0;
		virtual ErrorCode UpdatePipelineBindings(
		    Handle handle, i32 base, Core::ArrayView<const SamplerState> descs) = 0;
		virtual ErrorCode CopyPipelineBindings(
		    Core::ArrayView<const PipelineBinding> dst, Core::ArrayView<const PipelineBinding> src) = 0;
		virtual ErrorCode ValidatePipelineBindings(Core::ArrayView<const PipelineBinding> pb) = 0;

		/**
		 * Command list management.
		 */
		virtual ErrorCode CompileCommandList(Handle handle, const CommandList& commandList) = 0;
		virtual ErrorCode SubmitCommandLists(Core::ArrayView<Handle> handles) = 0;

		/**
		 * Swapchain management.
		 */
		virtual ErrorCode PresentSwapChain(Handle handle) = 0;
		virtual ErrorCode ResizeSwapChain(Handle handle, i32 width, i32 height) = 0;

		/**
		 * Frame management.
		 */
		virtual void NextFrame() = 0;
	};

	/**
	 * Define backend plugin.
	 */
	struct BackendPlugin : Plugin::Plugin
	{
		DECLARE_PLUGININFO(GPU::BackendPlugin, 0);

		/// API this backend represents.
		const char* api_ = nullptr;

		typedef IBackend* (*CreateBackendFn)(const SetupParams&);
		CreateBackendFn CreateBackend = nullptr;

		typedef void (*DestroyBackendFn)(IBackend*&);
		DestroyBackendFn DestroyBackend = nullptr;
	};


} // namespace GPU
