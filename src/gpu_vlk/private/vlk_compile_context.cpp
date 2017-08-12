#include "gpu_vlk/vlk_compile_context.h"
#include "gpu_vlk/vlk_command_list.h"
#include "gpu_vlk/vlk_backend.h"
#include "gpu_vlk/vlk_device.h"
#include "gpu_vlk/vlk_linear_heap_allocator.h"
#include "gpu/command_list.h"
#include "gpu/commands.h"

namespace GPU
{
	VLKCompileContext::VLKCompileContext(VLKBackend& backend)
	    : backend_(backend)
	{
	}

	VLKCompileContext::~VLKCompileContext() {}

	ErrorCode VLKCompileContext::CompileCommandList(VLKCommandList& outCommandList, const CommandList& commandList)
	{
		vlkCommandList_ = outCommandList.Open();
		if(d3dCommandList_)
		{
#define CASE_COMMAND(TYPE_STRUCT)                                                                                      \
	case TYPE_STRUCT::TYPE:                                                                                            \
		RETURN_ON_ERROR(CompileCommand(static_cast<const TYPE_STRUCT*>(command)));                                     \
		break

			for(const auto* command : commandList)
			{
				switch(command->type_)
				{
					CASE_COMMAND(CommandDraw);
					CASE_COMMAND(CommandDrawIndirect);
					CASE_COMMAND(CommandDispatch);
					CASE_COMMAND(CommandDispatchIndirect);
					CASE_COMMAND(CommandClearRTV);
					CASE_COMMAND(CommandClearDSV);
					CASE_COMMAND(CommandClearUAV);
					CASE_COMMAND(CommandUpdateBuffer);
					CASE_COMMAND(CommandUpdateTextureSubResource);
					CASE_COMMAND(CommandCopyBuffer);
					CASE_COMMAND(CommandCopyTextureSubResource);
					break;
				default:
					DBG_BREAK;
				}
			}

#undef CASE_COMMAND

			RestoreDefault();
			return outCommandList.Close();
		}
		return ErrorCode::FAIL;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandDraw* command)
	{
		const auto& dbs = backend_.drawBindingSets_[command->drawBinding_.GetIndex()];

		SetPipelineBinding(command->pipelineBinding_);
		SetFrameBinding(command->frameBinding_);
		SetDrawBinding(command->drawBinding_, command->primitive_);
		SetDrawState(command->drawState_);

		FlushTransitions();
		if(dbs.ib_.BufferLocation == 0)
		{
			vlkCommandList_->DrawInstanced(
			    command->noofVertices_, command->noofInstances_, command->vertexOffset_, command->firstInstance_);
		}
		else
		{
			vlkCommandList_->DrawIndexedInstanced(command->noofVertices_, command->noofInstances_,
			    command->indexOffset_, command->vertexOffset_, command->firstInstance_);
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandDrawIndirect* command)
	{
		return ErrorCode::UNIMPLEMENTED;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandDispatch* command)
	{
		SetPipelineBinding(command->pipelineBinding_);
		vlkCommandList_->Dispatch(command->xGroups_, command->yGroups_, command->zGroups_);
		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandDispatchIndirect* command)
	{
		return ErrorCode::UNIMPLEMENTED;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandClearRTV* command)
	{
		const auto& fbs = backend_.frameBindingSets_[command->frameBinding_.GetIndex()];
		DBG_ASSERT(command->rtvIdx_ < fbs.numRTs_);


		D3D12_CPU_DESCRIPTOR_HANDLE handle = fbs.rtvs_.cpuDescHandle_;
		i32 rtvIdx =
		    fbs.swapChain_ == nullptr ? command->rtvIdx_ : command->rtvIdx_ + fbs.swapChain_->bbIdx_ * MAX_BOUND_RTVS;

		handle.ptr +=
		    rtvIdx * backend_.device_->vlkDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		AddTransition(fbs.rtvResources_[rtvIdx], D3D12_RESOURCE_STATE_RENDER_TARGET);
		FlushTransitions();

		vlkCommandList_->ClearRenderTargetView(handle, command->color_, 0, nullptr);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandClearDSV* command)
	{
		const auto& fbs = backend_.frameBindingSets_[command->frameBinding_.GetIndex()];
		DBG_ASSERT(fbs.desc_.dsv_.resource_);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = fbs.dsv_.cpuDescHandle_;

		i32 dsvIdx = fbs.swapChain_ == nullptr ? 0 : fbs.swapChain_->bbIdx_;
		AddTransition(fbs.dsvResources_[dsvIdx], D3D12_RESOURCE_STATE_DEPTH_WRITE);
		FlushTransitions();

		vlkCommandList_->ClearDepthStencilView(
		    handle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, command->depth_, command->stencil_, 0, nullptr);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandClearUAV* command) { return ErrorCode::UNIMPLEMENTED; }

	ErrorCode VLKCompileContext::CompileCommand(const CommandUpdateBuffer* command)
	{
		const auto* buf = backend_.GetD3D12Buffer(command->buffer_);
		DBG_ASSERT(buf && buf->resource_);

		auto uploadAlloc = backend_.device_->GetUploadAllocator().Alloc(command->size_);
		memcpy(uploadAlloc.address_, command->data_, command->size_);

		AddTransition(buf, D3D12_RESOURCE_STATE_COPY_DEST);
		FlushTransitions();

		d3dCommandList_->CopyBufferRegion(buf->resource_.Get(), command->offset_, uploadAlloc.baseResource_.Get(),
		    uploadAlloc.offsetInBaseResource_, command->size_);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandUpdateTextureSubResource* command)
	{
		const auto* tex = backend_.GetD3D12Texture(command->texture_);
		DBG_ASSERT(tex && tex->resource_);

		auto srcLayout = command->data_;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT dstLayout;
		i32 numRows = 0;
		i64 rowSizeInBytes = 0;
		i64 totalBytes = 0;
		D3D12_RESOURCE_DESC resDesc = GetResourceDesc(tex->desc_);

		backend_.device_->d3dDevice_->GetCopyableFootprints(&resDesc, command->subResourceIdx_, 1, 0, &dstLayout,
		    (u32*)&numRows, (u64*)&rowSizeInBytes, (u64*)&totalBytes);

		auto resAlloc = backend_.device_->GetUploadAllocator().Alloc(totalBytes);
		DBG_ASSERT(srcLayout.rowPitch_ <= rowSizeInBytes);
		const u8* srcData = (const u8*)command->data_.data_;
		u8* dstData = (u8*)resAlloc.address_ + dstLayout.Offset;
		for(i32 slice = 0; slice < tex->desc_.depth_; ++slice)
		{
			const u8* rowSrcData = srcData;
			for(i32 row = 0; row < numRows; ++row)
			{
				memcpy(dstData, srcData, srcLayout.rowPitch_);
				dstData += rowSizeInBytes;
				rowSrcData += srcLayout.rowPitch_;
			}
			rowSrcData += srcLayout.slicePitch_;
		}

		D3D12_TEXTURE_COPY_LOCATION dst;
		dst.pResource = tex->resource_.Get();
		dst.SubresourceIndex = command->subResourceIdx_;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION src;
		src.pResource = resAlloc.baseResource_.Get();
		src.PlacedFootprint = dstLayout;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		AddTransition(tex, D3D12_RESOURCE_STATE_COPY_DEST);
		FlushTransitions();
		d3dCommandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandCopyBuffer* command)
	{
		const auto* dstBuf = backend_.GetD3D12Buffer(command->dstBuffer_);
		const auto* srcBuf = backend_.GetD3D12Buffer(command->srcBuffer_);
		DBG_ASSERT(dstBuf && dstBuf->resource_);
		DBG_ASSERT(srcBuf && srcBuf->resource_);

		AddTransition(dstBuf, D3D12_RESOURCE_STATE_COPY_DEST);
		AddTransition(srcBuf, D3D12_RESOURCE_STATE_COPY_SOURCE);
		FlushTransitions();

		d3dCommandList_->CopyBufferRegion(dstBuf->resource_.Get(), command->dstOffset_, srcBuf->resource_.Get(),
		    command->srcOffset_, command->srcSize_);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::CompileCommand(const CommandCopyTextureSubResource* command)
	{
		const auto* dstTex = backend_.GetD3D12Texture(command->dstTexture_);
		const auto* srcTex = backend_.GetD3D12Texture(command->srcTexture_);
		DBG_ASSERT(dstTex && dstTex->resource_);
		DBG_ASSERT(srcTex && srcTex->resource_);

		D3D12_TEXTURE_COPY_LOCATION dst;
		dst.pResource = dstTex->resource_.Get();
		dst.SubresourceIndex = command->dstSubResourceIdx_;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		D3D12_TEXTURE_COPY_LOCATION src;
		src.pResource = srcTex->resource_.Get();
		src.SubresourceIndex = command->srcSubResourceIdx_;
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

		AddTransition(dstTex, D3D12_RESOURCE_STATE_COPY_DEST);
		AddTransition(srcTex, D3D12_RESOURCE_STATE_COPY_SOURCE);
		FlushTransitions();

		D3D12_BOX box;
		box.left = command->srcBox_.x_;
		box.top = command->srcBox_.y_;
		box.front = command->srcBox_.z_;
		box.right = command->srcBox_.x_ + command->srcBox_.w_;
		box.bottom = command->srcBox_.y_ + command->srcBox_.h_;
		box.back = command->srcBox_.z_ + command->srcBox_.d_;
		d3dCommandList_->CopyTextureRegion(
		    &dst, command->dstPoint_.x_, command->dstPoint_.y_, command->dstPoint_.z_, &src, &box);

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::SetDrawBinding(Handle dbsHandle, PrimitiveTopology primitive)
	{
		const auto& dbs = backend_.drawBindingSets_[dbsHandle.GetIndex()];

		// Setup draw binding.
		if(dbs.ibResource_)
		{
			AddTransition(dbs.ibResource_, D3D12_RESOURCE_STATE_INDEX_BUFFER);
			d3dCommandList_->IASetIndexBuffer(&dbs.ib_);
		}

		for(i32 i = 0; i < MAX_VERTEX_STREAMS; ++i)
			if(dbs.vbResources_[i])
				AddTransition(dbs.vbResources_[i], D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

		d3dCommandList_->IASetVertexBuffers(0, MAX_VERTEX_STREAMS, dbs.vbs_.data());
		d3dCommandList_->IASetPrimitiveTopology(GetPrimitiveTopology(primitive));


		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::SetPipelineBinding(Handle pbsHandle)
	{
		const auto& pbs = backend_.pipelineBindingSets_[pbsHandle.GetIndex()];

		ID3D12DescriptorHeap* heaps[] = {
		    pbs.samplers_.d3dDescriptorHeap_.Get(), pbs.srvs_.d3dDescriptorHeap_.Get(),
		};

		vlkCommandList_->SetDescriptorHeaps(2, heaps);
		vlkCommandList_->SetPipelineState(pbs.pipelineState_.Get());

		switch(pbs.rootSignature_)
		{
		case RootSignatureType::GRAPHICS:
			vlkCommandList_->SetGraphicsRootSignature(
			    backend_.device_->d3dRootSignatures_[(i32)pbs.rootSignature_].Get());
			vlkCommandList_->SetGraphicsRootDescriptorTable(0, pbs.samplers_.gpuDescHandle_);
			vlkCommandList_->SetGraphicsRootDescriptorTable(1, pbs.cbvs_.gpuDescHandle_);
			vlkCommandList_->SetGraphicsRootDescriptorTable(2, pbs.srvs_.gpuDescHandle_);
			vlkCommandList_->SetGraphicsRootDescriptorTable(3, pbs.uavs_.gpuDescHandle_);
			break;
		case RootSignatureType::COMPUTE:
			vlkCommandList_->SetComputeRootSignature(
			    backend_.device_->d3dRootSignatures_[(i32)pbs.rootSignature_].Get());
			vlkCommandList_->SetComputeRootDescriptorTable(0, pbs.samplers_.gpuDescHandle_);
			vlkCommandList_->SetComputeRootDescriptorTable(1, pbs.cbvs_.gpuDescHandle_);
			vlkCommandList_->SetComputeRootDescriptorTable(2, pbs.srvs_.gpuDescHandle_);
			vlkCommandList_->SetComputeRootDescriptorTable(3, pbs.uavs_.gpuDescHandle_);
			break;
		default:
			DBG_BREAK;
			return ErrorCode::FAIL;
		}

		return ErrorCode::OK;
	}

	ErrorCode VLKCompileContext::SetFrameBinding(Handle fbsHandle)
	{
		const auto& fbs = backend_.frameBindingSets_[fbsHandle.GetIndex()];

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescLocal;
		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescLocal;
		D3D12_CPU_DESCRIPTOR_HANDLE* rtvDesc = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE* dsvDesc = nullptr;

		i32 rtvBaseIdx = fbs.swapChain_ == nullptr ? 0 : fbs.swapChain_->bbIdx_ * MAX_BOUND_RTVS;
		i32 dsvBaseIdx = fbs.swapChain_ == nullptr ? 0 : fbs.swapChain_->bbIdx_;

		if(fbs.numRTs_)
		{
			rtvDesc = &rtvDescLocal;
			rtvDescLocal = fbs.rtvs_.cpuDescHandle_;

			for(i32 i = 0; i < fbs.numRTs_; ++i)
			{
				AddTransition(fbs.rtvResources_[rtvBaseIdx + i], D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			rtvDesc->ptr += rtvBaseIdx * backend_.device_->d3dDevice_->GetDescriptorHandleIncrementSize(
			                                 D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		if(fbs.desc_.dsv_.resource_)
		{
			dsvDesc = &dsvDescLocal;
			dsvDescLocal = fbs.dsv_.cpuDescHandle_;

			if(Core::ContainsAllFlags(fbs.desc_.dsv_.flags_, DSVFlags::READ_ONLY_DEPTH))
			{
				AddTransition(fbs.dsvResources_[dsvBaseIdx], D3D12_RESOURCE_STATE_DEPTH_READ);
			}
			else
			{
				AddTransition(fbs.dsvResources_[dsvBaseIdx], D3D12_RESOURCE_STATE_DEPTH_WRITE);
			}

			dsvDesc->ptr += dsvBaseIdx * backend_.device_->d3dDevice_->GetDescriptorHandleIncrementSize(
			                                 D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		}

		FlushTransitions();

		d3dCommandList_->OMSetRenderTargets(fbs.numRTs_, rtvDesc, TRUE, dsvDesc);

		return ErrorCode::OK;
	}
	
	ErrorCode VLKCompileContext::SetDrawState(const DrawState* drawState)
	{
		if(cachedDrawState_ == nullptr || drawState != cachedDrawState_)
		{
			if(drawState->viewport_ != cachedViewport_)
			{
				D3D12_VIEWPORT viewport;
				viewport.TopLeftX = drawState->viewport_.x_;
				viewport.TopLeftY = drawState->viewport_.y_;
				viewport.Width = drawState->viewport_.w_;
				viewport.Height = drawState->viewport_.h_;
				viewport.MinDepth = drawState->viewport_.zMin_;
				viewport.MaxDepth = drawState->viewport_.zMax_;
				d3dCommandList_->RSSetViewports(1, &viewport);
				cachedViewport_ = drawState->viewport_;
			}

			if(drawState->scissorRect_ != cachedScissorRect_)
			{
				D3D12_RECT scissorRect;
				scissorRect.left = drawState->scissorRect_.x_;
				scissorRect.top = drawState->scissorRect_.y_;
				scissorRect.right = drawState->scissorRect_.x_ + drawState->scissorRect_.w_;
				scissorRect.bottom = drawState->scissorRect_.y_ + drawState->scissorRect_.h_;
				d3dCommandList_->RSSetScissorRects(1, &scissorRect);
				cachedScissorRect_ = drawState->scissorRect_;
			}

			if(drawState->stencilRef_ != cachedStencilRef_)
			{
				d3dCommandList_->OMSetStencilRef(drawState->stencilRef_);
				cachedStencilRef_ = drawState->stencilRef_;
			}
		}
		return ErrorCode::OK;
	}

	void VLKCompileContext::AddTransition(const D3D12Resource* resource, D3D12_RESOURCE_STATES state)
	{
		DBG_ASSERT(resource);
		DBG_ASSERT(resource->resource_);

		auto stateEntry = stateTracker_.find(resource);
		if(stateEntry == stateTracker_.end())
		{
			stateEntry = stateTracker_.insert(resource, resource->defaultState_);
		}

		if(state != stateEntry->Second_)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = resource->resource_.Get();
			barrier.Transition.Subresource = 0xffffffff; // TODO.
			barrier.Transition.StateBefore = stateEntry->Second_;
			barrier.Transition.StateAfter = state;
			pendingBarriers_.insert(resource, barrier);
			stateEntry->Second_ = state;
		}
	}

	void VLKCompileContext::FlushTransitions()
	{
		if(pendingBarriers_.size() > 0)
		{
			// Copy pending barriers into flat vector.
			for(auto barrier : pendingBarriers_)
			{
				barriers_.push_back(barrier.Second_);
			}
			pendingBarriers_.clear();

			// Perform resource barriers.
			d3dCommandList_->ResourceBarrier(barriers_.size(), barriers_.data());
			barriers_.clear();
		}
	}

	void VLKCompileContext::RestoreDefault()
	{
		for(auto state : stateTracker_)
		{
			AddTransition(state.First_, state.First_->defaultState_);
		}
		FlushTransitions();
		stateTracker_.clear();
	}

} // namespace GPU
