#pragma once

#include "gpu_tf/tf_types.h"
//#include "gpu_tf/tf_descriptor_heap_allocator.h"
#include "gpu/resources.h"
#include "core/array.h"
#include "core/misc.h"
#include "core/vector.h"
#include <utility>
#include <cstdalign>

namespace GPU
{
	/**
	 * Resource read wrapper.
	 * This is used to synchronize CPU access to the resource object.
	 * by managing a lightweight rw lock.
	 */
	template<typename TYPE>
	struct ResourceRead final
	{
		ResourceRead() = default;

		ResourceRead(const Core::RWLock& lock, const TYPE& res)
		    : lock_(&lock)
		    , res_(&res)
		{
			lock_->BeginRead();
		}

		~ResourceRead()
		{
			if(lock_)
				lock_->EndRead();
		}

		template<typename TYPE2>
		ResourceRead(ResourceRead<TYPE2>&& other, const TYPE& res)
		    : lock_(nullptr)
		    , res_(&res)
		{
			std::swap(lock_, other.lock_);
		}

		ResourceRead& operator=(ResourceRead&& other)
		{
			std::swap(lock_, other.lock_);
			std::swap(res_, other.res_);
			return *this;
		}

		ResourceRead(const ResourceRead&) = delete;
		ResourceRead(ResourceRead&&) = default;

		const TYPE& operator*() const { return *res_; }
		const TYPE* operator->() const { return res_; }
		explicit operator bool() const { return !!res_; }

	private:
		template<typename>
		friend struct ResourceRead;

		const Core::RWLock* lock_ = nullptr;
		const TYPE* res_ = nullptr;
	};

	/**
	 * Resource write wrapper.
	 * This is used to synchronize CPU access to the resource object
	 * by managing a lightweight rw lock.
	 */
	template<typename TYPE>
	struct ResourceWrite final
	{
		ResourceWrite() = default;

		ResourceWrite(Core::RWLock& lock, TYPE& res)
		    : lock_(&lock)
		    , res_(&res)
		{
			lock_->BeginWrite();
		}

		~ResourceWrite()
		{
			if(lock_)
				lock_->EndWrite();
		}

		template<typename TYPE2>
		ResourceWrite(ResourceWrite<TYPE2>&& other, TYPE& res)
		    : lock_(nullptr)
		    , res_(&res)
		{
			std::swap(lock_, other.lock_);
		}

		ResourceWrite(const ResourceWrite&) = delete;
		ResourceWrite(ResourceWrite&&) = default;

		TYPE& operator*() { return *res_; }
		TYPE* operator->() { return res_; }
		explicit operator bool() const { return !!res_; }

	private:
		template<typename>
		friend struct ResourceWrite;

		Core::RWLock* lock_ = nullptr;
		TYPE* res_ = nullptr;
	};

	/**
	 * Resource pool.
	 */
	template<typename TYPE>
	class ResourcePool
	{
	public:
		// 256 resources per block.
		static const i32 INDEX_BITS = 8;
		static const i32 BLOCK_SIZE = 1 << INDEX_BITS;

		ResourcePool(const char* name)
		    : name_(name)
		{
			DBG_LOG("GPU::ResourcePool<%s>: Size: %lld, Stored Size: %lld\n", name_, sizeof(TYPE), sizeof(Resource));
		}

		~ResourcePool()
		{
			for(auto& rb : rbs_)
				Core::GeneralAllocator().Delete(rb);
		}

		ResourceRead<TYPE> Read(Handle handle) const
		{
			Core::ScopedReadLock lock(blockLock_);
			const i32 idx = handle.GetIndex();
			const i32 blockIdx = idx >> INDEX_BITS;
			const i32 resourceIdx = idx & (BLOCK_SIZE - 1);

			auto& res = rbs_[blockIdx]->resources_[resourceIdx];

			return ResourceRead<TYPE>(res.lock_, res.res_);
		}

		ResourceWrite<TYPE> Write(Handle handle)
		{
			Core::ScopedWriteLock lock(blockLock_);
			const i32 idx = handle.GetIndex();
			const i32 blockIdx = idx >> INDEX_BITS;
			const i32 resourceIdx = idx & (BLOCK_SIZE - 1);

			// TODO: Change this code to use virtual memory and just commit as required.
			while(blockIdx >= rbs_.size())
			{
				rbs_.push_back(Core::GeneralAllocator().New<Resources>());
			}

			auto& res = rbs_[blockIdx]->resources_[resourceIdx];

			return ResourceWrite<TYPE>(res.lock_, res.res_);
		}

		i32 size() const { return rbs_.size() * BLOCK_SIZE; }

	private:
		struct Resource
		{
			Core::RWLock lock_;
			TYPE res_;
		};

		struct Resources
		{
			Core::Array<Resource, BLOCK_SIZE> resources_;
		};

		Core::RWLock blockLock_;
		Core::Vector<Resources*> rbs_;
		const char* name_ = nullptr;
	};

	struct TFResource
	{
		i32 numSubResources_ = 0;
		::ResourceState supportedStates_ = RESOURCE_STATE_COMMON;
		::ResourceState defaultState_ = RESOURCE_STATE_COMMON;
	};

	struct TFBuffer : TFResource
	{
		BufferDesc desc_;
		::Buffer* buffer_ = nullptr;
	};

	struct TFTexture : TFResource
	{
		TextureDesc desc_;
		::Texture* texture_ = nullptr;
		::RenderTarget* renderTarget_ = nullptr;
	};

	struct TFSwapChain
	{
		::SwapChain* swapChain_ = nullptr;
		i32 bbIdx_ = 0;
	};

	struct TFShader
	{
		u8* byteCode_ = nullptr;
		u32 byteCodeSize_ = 0;
	};

	struct TFSubresourceRange
	{
		const TFResource* resource_ = nullptr;
		i32 firstSubRsc_ = 0;
		i32 numSubRsc_ = 0;

		explicit operator bool() const { return resource_ && numSubRsc_ > 0; }
	};

	struct TFGraphicsPipelineState
	{
		u32 stencilRef_ = 0; // TODO: Make part of draw?
		::RootSignature* rootSignature_ = nullptr;
		::Pipeline* pipeline_ = nullptr;
		::Shader* shader_ = nullptr;
		::BlendState* blendState_ = nullptr;
		::DepthState* depthState_ = nullptr;
		::RasterizerState* rasterizerState_ = nullptr;
		//const char* name_ = nullptr;
	};

	struct TFComputePipelineState
	{
		::RootSignature* rootSignature_ = nullptr;
		::Pipeline* pipeline_ = nullptr;
		::Shader* shader_ = nullptr;
		//const char* name_ = nullptr;
	};

	struct TFPipelineBindingSet
	{
		//D3D12DescriptorAllocation cbvs_;
		//D3D12DescriptorAllocation srvs_;
		//D3D12DescriptorAllocation uavs_;
		//D3D12DescriptorAllocation samplers_;
		//Buffer* cbvs_ = nullptr;
		//Buffer* srvs_ = nullptr;
		//Buffer* uavs_ = nullptr;

		Core::Vector<TFSubresourceRange> srvTransitions_;
		Core::Vector<TFSubresourceRange> uavTransitions_;
		Core::Vector<TFSubresourceRange> cbvTransitions_;

		bool shaderVisible_ = false;
		bool temporary_ = false;
	};

	struct TFDrawBindingSet
	{
		DrawBindingSetDesc desc_;
		Core::Array<const TFBuffer*, MAX_VERTEX_STREAMS> vBuffers_ = {};
		const TFBuffer* iBuffer_ = nullptr;
		//Core::Array<D3D12_VERTEX_BUFFER_VIEW, MAX_VERTEX_STREAMS> vbs_ = {};
		//D3D12_INDEX_BUFFER_VIEW ib_;
	};

	struct TFFrameBindingSet
	{
		Core::Vector<::RenderTarget*> renderTargets_;
		::RenderTarget* dsRenderTarget_;
		FrameBindingSetDesc desc_;
		const TFSwapChain* swapChain_ = nullptr;
		i32 numRTs_ = 0;
		i32 numBuffers_ = 1;
	};

	struct TFFence
	{
		Fence* fence_;
		const char* name_ = nullptr;
	};

	struct TFSemaphore
	{
		Semaphore* semaphore_;
		const char* name_ = nullptr;
	};
}
