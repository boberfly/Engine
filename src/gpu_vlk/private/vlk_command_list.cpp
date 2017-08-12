#include "gpu_vlk/vlk_command_list.h"
#include "gpu_vlk/vlk_device.h"
#include "core/debug.h"

#include <utility>

namespace GPU
{
	VLKCommandList::VLKCommandList(VLKDevice& device, u32 nodeMask, D3D12_COMMAND_LIST_TYPE type)
	    : device_(device)
	{
		VkResult vkr = VK_SUCCESS;

		// TODO: Configurable number of command lists.
		d3dCommandAllocators_.resize(MAX_GPU_FRAMES);
		for(i32 i = 0; i < MAX_GPU_FRAMES; ++i)
		{
			CHECK_D3D(hr = device.d3dDevice_->CreateCommandAllocator(
			              type, IID_ID3D12CommandAllocator, (void**)d3dCommandAllocators_[i].GetAddressOf()));
		}

		type_ = type;
		CHECK_D3D(hr = device.d3dDevice_->CreateCommandList(nodeMask, type, d3dCommandAllocators_[0].Get(), nullptr,
		              IID_ID3D12GraphicsCommandList, (void**)d3dCommandList_.GetAddressOf()));
		CHECK_D3D(hr = d3dCommandList_->Close());
		CHECK_D3D(hr = device.d3dDevice_->CreateFence(
		              0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)d3dFence_.GetAddressOf()));

		fenceEvent_ = ::CreateEvent(nullptr, FALSE, FALSE, "D3D12CommandList");



		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

		CHECK_VLK(vkr = vkAllocateCommandBuffers(device_.vkDevice_, &allocInfo, commandBuffers.data()));
	}

	VLKCommandList::~VLKCommandList()
	{
		if(listIdx_ > 0 && d3dFence_->GetCompletedValue() != listIdx_ - 1)
		{
			d3dFence_->SetEventOnCompletion(listIdx_ - 1, fenceEvent_);
			::WaitForSingleObject(fenceEvent_, INFINITE);
		}

		::CloseHandle(fenceEvent_);
	}

	ID3D12GraphicsCommandList* VLKCommandList::Open()
	{
		DBG_ASSERT(!isOpen_);

		// Check that we've completed uploads from the next allocator.
		if((listIdx_ - d3dFence_->GetCompletedValue()) >= listCount_)
		{
			d3dFence_->SetEventOnCompletion(listIdx_ - listCount_, fenceEvent_);
			::WaitForSingleObject(fenceEvent_, INFINITE);
		}
		i32 listIdx = listIdx_ % listCount_;
		CHECK_D3D(d3dCommandAllocators_[listIdx]->Reset());
		CHECK_D3D(d3dCommandList_->Reset(d3dCommandAllocators_[listIdx].Get(), nullptr));
		isOpen_ = true;
		return d3dCommandList_.Get();
	}

	ErrorCode VLKCommandList::Close()
	{
		DBG_ASSERT(isOpen_);
		HRESULT hr = d3dCommandList_->Close();
		if(FAILED(hr))
			return ErrorCode::FAIL;

		isOpen_ = false;
		return ErrorCode::OK;
	}

	ErrorCode VLKCommandList::Submit(VkQueue vlkCommandQueue)
	{
		DBG_ASSERT(!isOpen_);
		ID3D12CommandList* d3dCommandLists[1] = {d3dCommandList_.Get()};
		d3dCommandQueue->ExecuteCommandLists(1, d3dCommandLists);
		CHECK_D3D(d3dCommandQueue->Signal(d3dFence_.Get(), listIdx_));

		// HACK TO WAIT.
		if(d3dFence_->GetCompletedValue() != listIdx_)
		{
			d3dFence_->SetEventOnCompletion(listIdx_, fenceEvent_);
			::WaitForSingleObject(fenceEvent_, INFINITE);
		}

		++listIdx_;
		return ErrorCode::OK;
	}


} // namespace GPU
