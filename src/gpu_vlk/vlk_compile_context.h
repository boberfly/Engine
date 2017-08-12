#pragma once

#include "gpu_vlk/vlk_types.h"
#include "gpu_vlk/vlk_resources.h"
#include "gpu/resources.h"
#include "core/map.h"

namespace Core
{
	inline u32 Hash(u32 input, const GPU::VLKResource* data) { return HashCRC32(input, &data, sizeof(data)); }
}

namespace GPU
{

	struct VLKCompileContext
	{
		VLKCompileContext(class VLKBackend& backend);
		~VLKCompileContext();

		class VLKBackend& backend_;
		ID3D12GraphicsCommandList* d3dCommandList_ = nullptr;

		Core::Map<const D3D12Resource*, D3D12_RESOURCE_STATES> stateTracker_;
		Core::Map<const D3D12Resource*, D3D12_RESOURCE_BARRIER> pendingBarriers_;
		Core::Vector<D3D12_RESOURCE_BARRIER> barriers_;

		DrawState drawState_;
		DrawState* cachedDrawState_ = nullptr;
		Viewport cachedViewport_;	
		ScissorRect cachedScissorRect_;
		u8 cachedStencilRef_ = 0;

		ErrorCode CompileCommandList(class VLKCommandList& outCommandList, const class CommandList& commandList);
		ErrorCode CompileCommand(const struct CommandDraw* command);
		ErrorCode CompileCommand(const struct CommandDrawIndirect* command);
		ErrorCode CompileCommand(const struct CommandDispatch* command);
		ErrorCode CompileCommand(const struct CommandDispatchIndirect* command);
		ErrorCode CompileCommand(const struct CommandClearRTV* command);
		ErrorCode CompileCommand(const struct CommandClearDSV* command);
		ErrorCode CompileCommand(const struct CommandClearUAV* command);
		ErrorCode CompileCommand(const struct CommandUpdateBuffer* command);
		ErrorCode CompileCommand(const struct CommandUpdateTextureSubResource* command);
		ErrorCode CompileCommand(const struct CommandCopyBuffer* command);
		ErrorCode CompileCommand(const struct CommandCopyTextureSubResource* command);

		ErrorCode SetDrawBinding(Handle dbsHandle, PrimitiveTopology primitive);
		ErrorCode SetPipelineBinding(Handle pbsHandle);
		ErrorCode SetFrameBinding(Handle fbsHandle);
		ErrorCode SetDrawState(const DrawState* drawState);

		/**
		 * Add resource transition.
		 * @param resource Resource to transition.
		 * @param state States to transition.
		 */
		void AddTransition(const D3D12Resource* resource, D3D12_RESOURCE_STATES state);

		/**
		 * Flush resource transitions.
		 */
		void FlushTransitions();

		/**
		 * Restore default state of resources.
		 */
		void RestoreDefault();
	};
} // namespace GPU
