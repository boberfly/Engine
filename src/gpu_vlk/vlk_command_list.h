#pragma once

#include "gpu_vlk/dll.h"
#include "gpu_vlk/vlk_types.h"
#include "gpu/command_list.h"
#include "core/vector.h"

namespace GPU
{
	class VLKDevice;

	class VLKCommandList
	{
	public:
		VLKCommandList(VLKDevice& vlkDevice, u32 nodeMask, D3D12_COMMAND_LIST_TYPE type);
		~VLKCommandList();

		/**
		 * Open a command list for use.
		 */
		ID3D12GraphicsCommandList* Open();

		/**
		 * Close command list.
		 */
		ErrorCode Close();

		/**
		 * Submit command list to queue.
		 */
		ErrorCode Submit(ID3D12CommandQueue* d3dCommandQueue);

		VLKDevice& device_;
		D3D12_COMMAND_LIST_TYPE type_;
		Core::Vector<ComPtr<ID3D12CommandAllocator>> d3dCommandAllocators_;
		ComPtr<ID3D12GraphicsCommandList> d3dCommandList_;
		i32 listCount_ = MAX_GPU_FRAMES;
		i32 listIdx_ = 0;
		bool isOpen_ = false;

		/// Fence to wait on pending command lists to finish execution.
		HANDLE fenceEvent_;
		ComPtr<ID3D12Fence> d3dFence_;
	};

} // namespace GPU
