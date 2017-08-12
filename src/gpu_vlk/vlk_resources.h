#pragma once

#include "gpu_vlk/vlk_types.h"
#include "gpu_vlk/vlk_descriptor_heap_allocator.h"
#include "gpu/resources.h"
#include "core/array.h"
#include "core/misc.h"
#include "core/vector.h"
#include <utility>

namespace GPU
{
	/**
	 * Resource vector.
	 * Just a wrapper around Core::Vector that will automatically
	 * resize when inserting resources.
	 */
	template<typename TYPE>
	class ResourceVector
	{
	public:
		static const i32 RESIZE_ROUND_UP = 32;
		ResourceVector() = default;
		~ResourceVector() = default;

		TYPE& operator[](i32 idx)
		{
			DBG_ASSERT(idx < Handle::MAX_INDEX);
			if(idx >= storage_.size())
				storage_.resize(Core::PotRoundUp(idx + 1, RESIZE_ROUND_UP));
			return storage_[idx];
		}

		const TYPE& operator[](i32 idx) const
		{
			DBG_ASSERT(idx < Handle::MAX_INDEX);
			DBG_ASSERT(idx < storage_.size());
			return storage_[idx];
		}

		i32 size() const { return storage_.size(); }
		void clear() { storage_.clear(); }

	private:
		Core::Vector<TYPE> storage_;
	};

	class VLKScopedBufferBarrier
	{
	public:
		VLKScopedBufferBarrier(VkCommandBuffer commandBuffer, VkBuffer buffer, u32 subresource,
		    D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState)
		    : commandList_(commandList)
		{
			barrier_.sType;
			barrier_.pNext;
			barrier_.srcAccessMask;
			barrier_.dstAccessMask;
			barrier_.srcQueueFamilyIndex;
			barrier_.dstQueueFamilyIndex;
			barrier_.buffer;
			barrier_.offset;
			barrier_.size;

			barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier_.Transition.pResource = resource;
			barrier_.Transition.Subresource = subresource;
			barrier_.Transition.StateBefore = oldState;
			barrier_.Transition.StateAfter = newState;
			commandList_->ResourceBarrier(1, &barrier_);
		}

		~VLKScopedResourceBarrier()
		{
			std::swap(barrier_.Transition.StateBefore, barrier_.Transition.StateAfter);
			commandList_->ResourceBarrier(1, &barrier_);
		}

	private:
		ID3D12GraphicsCommandList* commandList_ = nullptr;
		VkBufferMemoryBarrier barrier_;
	}

	class VLKScopedResourceBarrier
	{
	public:
		VLKScopedResourceBarrier(VkCommandBuffer commandBuffer, ID3D12Resource* resource, u32 subresource,
		    D3D12_RESOURCE_STATES oldState, D3D12_RESOURCE_STATES newState)
		    : commandList_(commandList)
		{
			barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier_.Transition.pResource = resource;
			barrier_.Transition.Subresource = subresource;
			barrier_.Transition.StateBefore = oldState;
			barrier_.Transition.StateAfter = newState;
			commandList_->ResourceBarrier(1, &barrier_);
		}

		~VLKScopedResourceBarrier()
		{
			std::swap(barrier_.Transition.StateBefore, barrier_.Transition.StateAfter);
			commandList_->ResourceBarrier(1, &barrier_);
		}

	private:
		ID3D12GraphicsCommandList* commandList_ = nullptr;
		D3D12_RESOURCE_BARRIER barrier_;
	};


	struct VLKResource
	{
	};

	struct VLKBuffer : VLKResource
	{
		BufferDesc desc_;
		VkBuffer buffer_;
	};

	struct VLKTexture : VLKResource
	{
		TextureDesc desc_;
		VkImage image_;
		VkImageLayout initialLayout_;
		VkAccessFlagBits initialLayoutAccessBits_;
		VkDeviceMemory memory_;
	};

	struct VLKSwapChain
	{
		VkSwapchainKHR swapChain_;
		Core::Vector<VkImage> images_;
		i32 bbIdx_ = 0;
	};

	struct VLKShader
	{
		u8* byteCode_ = nullptr;
		u32 byteCodeSize_ = 0;
		VkShaderModule shaderModule_;
	};

	struct VLKSamplerState
	{
		VkSamplerCreateInfo desc_ = {};
	};

	struct VLKGraphicsPipelineState
	{
		RootSignatureType rootSignature_ = RootSignatureType::GRAPHICS;
		u32 stencilRef_ = 0; // TODO: Make part of draw?
		//ComPtr<ID3D12PipelineState> pipelineState_;
		VkPipeline pipelineState_;

	};

	struct VLKComputePipelineState
	{
		RootSignatureType rootSignature_ = RootSignatureType::COMPUTE;
		ComPtr<ID3D12PipelineState> pipelineState_;
	};

	struct VLKPipelineBindingSet
	{
		RootSignatureType rootSignature_ = RootSignatureType::INVALID;
		ComPtr<ID3D12PipelineState> pipelineState_;
		D3D12DescriptorAllocation srvs_;
		D3D12DescriptorAllocation uavs_;
		D3D12DescriptorAllocation cbvs_;
		D3D12DescriptorAllocation samplers_;
	};

	struct VLKDrawBindingSet
	{
		DrawBindingSetDesc desc_;
		Core::Array<D3D12Resource*, MAX_VERTEX_STREAMS> vbResources_;
		D3D12Resource* ibResource_ = nullptr;
		Core::Array<D3D12_VERTEX_BUFFER_VIEW, MAX_VERTEX_STREAMS> vbs_;
		D3D12_INDEX_BUFFER_VIEW ib_;
	};

	struct VLKFrameBindingSet
	{
		D3D12DescriptorAllocation rtvs_;
		D3D12DescriptorAllocation dsv_;
		Core::Vector<D3D12Resource*> rtvResources_;
		Core::Vector<D3D12Resource*> dsvResources_;
		FrameBindingSetDesc desc_;
		D3D12SwapChain* swapChain_ = nullptr;
		i32 numRTs_ = 0;
		i32 numBuffers_ = 1;
	};
}
