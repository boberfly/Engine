#include "gpu_tf/tf_types.h"
#include "core/concurrency.h"
#include "core/debug.h"
#include "core/misc.h"
#include "core/string.h"

#include <OS/Image/ImageEnums.h>

namespace GPU
{
	bool Initialized = false;

	::BufferUsage GPU::GetBufferUsage(GPU::BindFlags bindFlags)
	{
		::BufferUsage retVal;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::CONSTANT_BUFFER))
			retVal |= BUFFER_USAGE_UNIFORM;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::INDEX_BUFFER))
			retVal |= BUFFER_USAGE_INDEX;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::VERTEX_BUFFER))
			retVal |= BUFFER_USAGE_VERTEX;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::INDIRECT_BUFFER))
			retVal |= BUFFER_USAGE_INDIRECT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::UNORDERED_ACCESS))
			retVal |= BUFFER_USAGE_STORAGE_UAV;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::SHADER_RESOURCE))
			retVal |= BUFFER_USAGE_STORAGE_SRV;
		return retVal;
	}

	::ResourceState GPU::GetResourceStates(GPU::BindFlags bindFlags)
	{
		::ResourceState retVal = RESOURCE_STATE_UNDEFINED;
		if(Core::ContainsAnyFlags(bindFlags, BindFlags::VERTEX_BUFFER | BindFlags::CONSTANT_BUFFER))
			retVal |= RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::INDEX_BUFFER))
			retVal |= RESOURCE_STATE_INDEX_BUFFER;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::INDIRECT_BUFFER))
			retVal |= RESOURCE_STATE_INDIRECT_ARGUMENT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::SHADER_RESOURCE))
			retVal |= RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | RESOURCE_STATE_SHADER_RESOURCE;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::STREAM_OUTPUT))
			retVal |= RESOURCE_STATE_STREAM_OUT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::RENDER_TARGET))
			retVal |= RESOURCE_STATE_RENDER_TARGET;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::DEPTH_STENCIL))
			retVal |= RESOURCE_STATE_DEPTH_WRITE | RESOURCE_STATE_DEPTH_READ;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::UNORDERED_ACCESS))
			retVal |= RESOURCE_STATE_UNORDERED_ACCESS;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::PRESENT))
			retVal |= RESOURCE_STATE_PRESENT;
		return retVal;
	}

	::ResourceMemoryUsage GPU::GetResourceMemoryUsage(GPU::MemoryUsage usage)
	{
		::ResourceMemoryUsage retVal = RESOURCE_MEMORY_USAGE_UNKNOWN;
		switch(usage)
		{
		case MemoryUsage::GPU_ONLY:
			return RESOURCE_MEMORY_USAGE_GPU_ONLY;
		case MemoryUsage::CPU_ONLY:
			return RESOURCE_MEMORY_USAGE_CPU_ONLY;
		case MemoryUsage::CPU_TO_GPU:
			return RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		case MemoryUsage::GPU_TO_CPU:
			return RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
		default:
			return RESOURCE_MEMORY_USAGE_UNKNOWN;
		}
	}

	::BufferCreationFlags GPU::GetBufferCreationFlags(GPU::BindFlags bindFlags)
	{
		::BufferCreationFlags retVal = BUFFER_CREATION_FLAG_NONE;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::OWN_MEMORY_BIT))
			retVal |= BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::PERSISTENT_MAP_BIT))
			retVal |= BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::ESRAM))
			retVal |= BUFFER_CREATION_FLAG_ESRAM;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::NO_DESCRIPTOR_VIEW_CREATION))
			retVal |= BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION;
		return retVal;
	}

	::TextureType GetTextureType(GPU::TextureType type)
	{
		switch(type)
		{
		case TextureType::TEX1D:
			return TEXTURE_TYPE_1D;
		case TextureType::TEX2D:
			return TEXTURE_TYPE_2D;
		case TextureType::TEX3D:
			return TEXTURE_TYPE_3D;
		case TextureType::TEXCUBE:
			return TEXTURE_TYPE_CUBE;
		default:
			return TEXTURE_TYPE_UNDEFINED;
		}
	}

	::TextureUsage GPU::GetTextureUsage(GPU::BindFlags bindFlags)
	{
		TextureUsage retVal = TEXTURE_USAGE_UNDEFINED;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::UNORDERED_ACCESS))
			retVal |= TEXTURE_USAGE_UNORDERED_ACCESS;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::SHADER_RESOURCE))
			retVal |= TEXTURE_USAGE_SAMPLED_IMAGE;
		return retVal;
	}

	::TextureCreationFlags GPU::GetTextureCreationFlags(GPU::BindFlags bindFlags)
	{
		::TextureCreationFlags retVal = TEXTURE_CREATION_FLAG_NONE;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::OWN_MEMORY_BIT))
			retVal |= TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::EXPORT_BIT))
			retVal |= TEXTURE_CREATION_FLAG_EXPORT_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::EXPORT_ADAPTER_BIT))
			retVal |= TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::IMPORT_BIT))
			retVal |= TEXTURE_CREATION_FLAG_IMPORT_BIT;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::ESRAM))
			retVal |= TEXTURE_CREATION_FLAG_ESRAM;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::ON_TILE))
			retVal |= TEXTURE_CREATION_FLAG_ON_TILE;
		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::NO_COMPRESSION))
			retVal |= TEXTURE_CREATION_FLAG_NO_COMPRESSION;
	}

	::RenderTargetType GetRenderTargetType(GPU::TextureType type)
	{
		switch(type)
		{
		case TextureType::TEX1D:
			return RENDER_TARGET_TYPE_1D;
		case TextureType::TEX2D:
			return RENDER_TARGET_TYPE_2D;
		case TextureType::TEX3D:
			return RENDER_TARGET_TYPE_3D;
		case TextureType::BUFFER:
			return RENDER_TARGET_TYPE_BUFFER;
		default:
			return RENDER_TARGET_TYPE_UNKNOWN;
		}
	}

	::RenderTargetUsage GPU::GetRenderTargetUsage(GPU::BindFlags bindFlags, GPU::Format format)
	{
		::RenderTargetUsage retVal = RENDER_TARGET_USAGE_UNDEFINED;

		switch (format)
		{
		case GPU::Format::R32G32B32A32_TYPELESS:
		case GPU::Format::R32G32B32A32_FLOAT:
		case GPU::Format::R32G32B32A32_UINT:
		case GPU::Format::R32G32B32A32_SINT:
		case GPU::Format::R32G32B32_TYPELESS:
		case GPU::Format::R32G32B32_FLOAT:
		case GPU::Format::R32G32B32_UINT:
		case GPU::Format::R32G32B32_SINT:
		case GPU::Format::R16G16B16A16_TYPELESS:
		case GPU::Format::R16G16B16A16_FLOAT:
		case GPU::Format::R16G16B16A16_UNORM:
		case GPU::Format::R16G16B16A16_UINT:
		case GPU::Format::R16G16B16A16_SNORM:
		case GPU::Format::R16G16B16A16_SINT:
		case GPU::Format::R32G32_TYPELESS:
		case GPU::Format::R32G32_FLOAT:
		case GPU::Format::R32G32_UINT:
		case GPU::Format::R32G32_SINT:
		case GPU::Format::R32G8X24_TYPELESS:
		case GPU::Format::B8G8R8A8_UNORM:
		case GPU::Format::B8G8R8X8_UNORM:
		case GPU::Format::B8G8R8A8_TYPELESS:
		case GPU::Format::B8G8R8A8_UNORM_SRGB:
		case GPU::Format::B8G8R8X8_TYPELESS:
		case GPU::Format::B8G8R8X8_UNORM_SRGB:
		case GPU::Format::R10G10B10A2_TYPELESS:
		case GPU::Format::R10G10B10A2_UNORM:
		case GPU::Format::R10G10B10A2_UINT:
		case GPU::Format::R11G11B10_FLOAT:
		case GPU::Format::R8G8B8A8_TYPELESS:
		case GPU::Format::R8G8B8A8_UNORM:
		case GPU::Format::R8G8B8A8_UNORM_SRGB:
		case GPU::Format::R8G8B8A8_UINT:
		case GPU::Format::R8G8B8A8_SNORM:
		case GPU::Format::R8G8B8A8_SINT:
		case GPU::Format::R16G16_TYPELESS:
		case GPU::Format::R16G16_FLOAT:
		case GPU::Format::R16G16_UNORM:
		case GPU::Format::R16G16_UINT:
		case GPU::Format::R16G16_SNORM:
		case GPU::Format::R16G16_SINT:
		case GPU::Format::R32_TYPELESS:
		case GPU::Format::R32_FLOAT:
		case GPU::Format::R32_UINT:
		case GPU::Format::R32_SINT:
		case GPU::Format::R24G8_TYPELESS:
		case GPU::Format::R16_UNORM:
		case GPU::Format::R16_UINT:
		case GPU::Format::R16_SNORM:
		case GPU::Format::R16_SINT:
		case GPU::Format::R8_TYPELESS:
		case GPU::Format::R8_UNORM:
		case GPU::Format::R8_UINT:
		case GPU::Format::R8_SNORM:
		case GPU::Format::R8_SINT:
		case GPU::Format::A8_UNORM:
		case GPU::Format::R8G8_TYPELESS:
		case GPU::Format::R8G8_UNORM:
		case GPU::Format::R8G8_UINT:
		case GPU::Format::R8G8_SNORM:
		case GPU::Format::R8G8_SINT:
		case GPU::Format::R16_TYPELESS:
		case GPU::Format::R16_FLOAT:
			retVal |= RENDER_TARGET_USAGE_COLOR;
			break;
		case GPU::Format::D32_FLOAT_S8X24_UINT:
		case GPU::Format::R32_FLOAT_X8X24_TYPELESS:
		case GPU::Format::X32_TYPELESS_G8X24_UINT:
		case GPU::Format::D32_FLOAT:
		case GPU::Format::D24_UNORM_S8_UINT:
		case GPU::Format::R24_UNORM_X8_TYPELESS:
		case GPU::Format::X24_TYPELESS_G8_UINT:
		case GPU::Format::D16_UNORM:
			retVal |= RENDER_TARGET_USAGE_DEPTH_STENCIL;
			break;
		default:
			break;
		}

		if(Core::ContainsAllFlags(bindFlags, GPU::BindFlags::UNORDERED_ACCESS))
			retVal |= RENDER_TARGET_USAGE_UNORDERED_ACCESS;

		return retVal;
	}

	::ImageFormat::Enum GetImageFormat(GPU::Format format)
	{
		switch (format)
		{
		case GPU::Format::INVALID:                   return ImageFormat::NONE;
		case GPU::Format::R32G32B32A32_TYPELESS:     return ImageFormat::RGBA32I;
		case GPU::Format::R32G32B32A32_FLOAT:        return ImageFormat::RGBA32F;
		case GPU::Format::R32G32B32A32_UINT:         return ImageFormat::RGBA32UI;
		case GPU::Format::R32G32B32A32_SINT:         return ImageFormat::RGBA32I;
		case GPU::Format::R32G32B32_TYPELESS:        return ImageFormat::RGB32I;
		case GPU::Format::R32G32B32_FLOAT:           return ImageFormat::RGB32F;
		case GPU::Format::R32G32B32_UINT:            return ImageFormat::RGB32UI;
		case GPU::Format::R32G32B32_SINT:            return ImageFormat::RGB32I;
		case GPU::Format::R16G16B16A16_TYPELESS:     return ImageFormat::RGBA16;
		case GPU::Format::R16G16B16A16_FLOAT:        return ImageFormat::RGBA16F;
		case GPU::Format::R16G16B16A16_UNORM:        return ImageFormat::RGBA16;
		case GPU::Format::R16G16B16A16_UINT:         return ImageFormat::RGBA16UI;
		case GPU::Format::R16G16B16A16_SNORM:
		case GPU::Format::R16G16B16A16_SINT:         return ImageFormat::RGBA16S;
		case GPU::Format::R32G32_TYPELESS:           return ImageFormat::RG32UI;
		case GPU::Format::R32G32_FLOAT:              return ImageFormat::RG32F;
		case GPU::Format::R32G32_UINT:               return ImageFormat::RG32UI;
		case GPU::Format::R32G32_SINT:               return ImageFormat::RG32I;
		case GPU::Format::R32G8X24_TYPELESS:
		case GPU::Format::D32_FLOAT_S8X24_UINT:
		case GPU::Format::R32_FLOAT_X8X24_TYPELESS:
		case GPU::Format::X32_TYPELESS_G8X24_UINT:   return ImageFormat::X8D24PAX32;
		case GPU::Format::R10G10B10A2_TYPELESS:
		case GPU::Format::R10G10B10A2_UNORM:
		case GPU::Format::R10G10B10A2_UINT:          return ImageFormat::RGB10A2;
		case GPU::Format::R11G11B10_FLOAT:           return ImageFormat::RG11B10F;
		case GPU::Format::R8G8B8A8_TYPELESS:
		case GPU::Format::R8G8B8A8_UNORM:
		case GPU::Format::R8G8B8A8_UNORM_SRGB:
		case GPU::Format::R8G8B8A8_UINT:             return ImageFormat::RGBA8;
		case GPU::Format::R8G8B8A8_SNORM:
		case GPU::Format::R8G8B8A8_SINT:             return ImageFormat::RGBA8S;
		case GPU::Format::R16G16_TYPELESS:           return ImageFormat::RG16;
		case GPU::Format::R16G16_FLOAT:              return ImageFormat::RG16F;
		case GPU::Format::R16G16_UNORM:              return ImageFormat::RG16;
		case GPU::Format::R16G16_UINT:               return ImageFormat::RG16UI;
		case GPU::Format::R16G16_SNORM:
		case GPU::Format::R16G16_SINT:               return ImageFormat::RG16I;
		case GPU::Format::R32_TYPELESS:              return ImageFormat::R32I;
		case GPU::Format::D32_FLOAT:                 return ImageFormat::D32F;
		case GPU::Format::R32_FLOAT:                 return ImageFormat::R32F;
		case GPU::Format::R32_UINT:                  return ImageFormat::R32UI;
		case GPU::Format::R32_SINT:                  return ImageFormat::R32I;
		case GPU::Format::R24G8_TYPELESS:
		case GPU::Format::D24_UNORM_S8_UINT:
		case GPU::Format::R24_UNORM_X8_TYPELESS:     return ImageFormat::D24S8;
		case GPU::Format::X24_TYPELESS_G8_UINT:      return ImageFormat::D24S8;
		case GPU::Format::R8G8_TYPELESS:
		case GPU::Format::R8G8_UNORM:
		case GPU::Format::R8G8_UINT:                 return ImageFormat::RG8;
		case GPU::Format::R8G8_SNORM:
		case GPU::Format::R8G8_SINT:                 return ImageFormat::RG8S;
		case GPU::Format::R16_TYPELESS:              return ImageFormat::R16;
		case GPU::Format::R16_FLOAT:                 return ImageFormat::R16F;
		case GPU::Format::D16_UNORM:                 return ImageFormat::D16;
		case GPU::Format::R16_UNORM:
		case GPU::Format::R16_UINT:                  return ImageFormat::R16;
		case GPU::Format::R16_SNORM:
		case GPU::Format::R16_SINT:                  return ImageFormat::R16S;
		case GPU::Format::R8_TYPELESS:
		case GPU::Format::R8_UNORM:
		case GPU::Format::R8_UINT:                   return ImageFormat::R8;
		case GPU::Format::R8_SNORM:
		case GPU::Format::R8_SINT:                   return ImageFormat::R8S;
		case GPU::Format::A8_UNORM:                  return ImageFormat::R8;
//		case R1_UNORM:                  return VK_FORMAT_UNDEFINED;
		case GPU::Format::BC1_TYPELESS:
		case GPU::Format::BC1_UNORM:
		case GPU::Format::BC1_UNORM_SRGB:            return ImageFormat::DXT1;
		case GPU::Format::BC2_TYPELESS:
		case GPU::Format::BC2_UNORM:
		case GPU::Format::BC2_UNORM_SRGB:            return ImageFormat::DXT3;
		case GPU::Format::BC3_TYPELESS:
		case GPU::Format::BC3_UNORM:
		case GPU::Format::BC3_UNORM_SRGB:            return ImageFormat::DXT5;
		case GPU::Format::BC4_TYPELESS:
		case GPU::Format::BC4_UNORM:
		case GPU::Format::BC4_SNORM:                 return ImageFormat::ATI1N;
		case GPU::Format::BC5_TYPELESS:
		case GPU::Format::BC5_UNORM:
		case GPU::Format::BC5_SNORM:                 return ImageFormat::ATI2N;
		case GPU::Format::B5G6R5_UNORM:              return ImageFormat::RGB565;
//		case B5G5R5A1_UNORM:            return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case GPU::Format::B8G8R8A8_UNORM:
		case GPU::Format::B8G8R8X8_UNORM:
		case GPU::Format::B8G8R8A8_TYPELESS:
		case GPU::Format::B8G8R8A8_UNORM_SRGB:
		case GPU::Format::B8G8R8X8_TYPELESS:
		case GPU::Format::B8G8R8X8_UNORM_SRGB:       return ImageFormat::BGRA8;
		default: return ImageFormat::NONE;
		}
	}

	bool GetSrgbFormat(GPU::Format format)
	{
		switch (format)
		{
			case GPU::Format::R8G8B8A8_UNORM_SRGB:
			case GPU::Format::B8G8R8A8_UNORM_SRGB:
			case GPU::Format::B8G8R8X8_UNORM_SRGB:
			case GPU::Format::BC1_UNORM_SRGB:
			case GPU::Format::BC2_UNORM_SRGB:
			case GPU::Format::BC3_UNORM_SRGB:
			case GPU::Format::BC7_UNORM_SRGB: return true;
			default: return false;
		}
	}

	::PrimitiveTopology GetPrimitiveTopology(GPU::PrimitiveTopology topology)
	{
		switch(topology)
		{
		case PrimitiveTopology::POINT_LIST:
			return PRIMITIVE_TOPO_POINT_LIST;
		case PrimitiveTopology::LINE_LIST:
			return PRIMITIVE_TOPO_LINE_LIST;
		case PrimitiveTopology::LINE_STRIP:
			return PRIMITIVE_TOPO_LINE_STRIP;
		case PrimitiveTopology::TRIANGLE_LIST:
			return PRIMITIVE_TOPO_TRI_LIST;
		case PrimitiveTopology::TRIANGLE_STRIP:
			return PRIMITIVE_TOPO_TRI_STRIP;
		case PrimitiveTopology::PATCH_LIST:
			return PRIMITIVE_TOPO_PATCH_LIST;
		default:
			DBG_ASSERT(false);
			return PRIMITIVE_TOPO_POINT_LIST;
		}
	}

	::BufferDesc GPU::GetBufferDesc(const GPU::BufferDesc& desc, const char* debugName)
	{
		::BufferDesc bufferDesc;
		memset(&bufferDesc, 0, sizeof(bufferDesc));
		/// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
		bufferDesc.mUsage = GPU::GetBufferUsage(desc.bindFlags_);
		/// Size of the buffer (in bytes)
		bufferDesc.mSize = desc.size_;
		/// Decides which memory heap buffer will use (default, upload, readback)
		bufferDesc.mMemoryUsage = GPU::GetResourceMemoryUsage(desc.memoryUsage_);
		/// Creation flags of the buffer
		bufferDesc.mFlags = GPU::GetBufferCreationFlags(desc.bindFlags_);
		/// What state will the buffer get created in
		bufferDesc.mStartState = GPU::GetResourceStates(desc.bindFlags_);
		/// Specifies whether the buffer will have 32 bit or 16 bit indices (applicable to BUFFER_USAGE_INDEX)
		if(Core::ContainsAllFlags(desc.bindFlags_, BindFlags::INDEX_BUFFER))
			bufferDesc.mIndexType = (::IndexType)desc.indexType_;
		/// Vertex stride of the buffer (applicable to BUFFER_USAGE_VERTEX)
		if(Core::ContainsAllFlags(desc.bindFlags_, BindFlags::VERTEX_BUFFER))
			bufferDesc.mVertexStride = (u32)desc.vertexStride_;
		/// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		bufferDesc.mFirstElement = (u64)desc.offset_;
		/// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		bufferDesc.mElementCount = (u64)desc.elementCount_;
		/// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		bufferDesc.mStructStride = (u64)desc.structStride_;
		/// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		//bufferDesc.pCounterBuffer = ;
		/// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
		bufferDesc.mFormat = GPU::GetImageFormat(desc.format_);
		/// Flags specifying the special features of the buffer(typeless buffer,...) (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
		//bufferDesc.mFeatures = ;
		/// Debug name used in gpu profile
		wchar wName[512] = {0};
		Core::StringConvertUTF8toUTF16(debugName, (i32)strlen(debugName), wName, 512);
		bufferDesc.pDebugName = wName;
		//bufferDesc.pSharedNodeIndices = ;
		//bufferDesc.mNodeIndex = ;
		//bufferDesc.mSharedNodeIndexCount = ;

		return bufferDesc;
	}

	::TextureDesc GetTextureDesc(const GPU::TextureDesc& desc, const char* debugName)
	{
		::TextureDesc textureDesc;
		memset(&textureDesc, 0, sizeof(textureDesc));
		/// Type of texture (1D, 2D, 3D, Cube)
		textureDesc.mType = GetTextureType(desc.type_);
		/// Texture creation flags (decides memory allocation strategy, sharing access,...)
		textureDesc.mFlags = GPU::GetTextureCreationFlags(desc.bindFlags_);
		/// Width
		textureDesc.mWidth = desc.width_;
		/// Height
		textureDesc.mHeight = desc.height_;
		/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
		textureDesc.mDepth = desc.type_ == TextureType::TEX3D ? desc.depth_ : 1;
		/// Start array layer (Should be between 0 and max array layers for this texture)
		textureDesc.mBaseArrayLayer = 0;
		/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
		if(desc.type_ == TextureType::TEXCUBE)
			textureDesc.mArraySize = 6;
		else
			textureDesc.mArraySize = desc.elements_;
		/// Most detailed mip level (Should be between 0 and max mip levels for this texture)
		textureDesc.mBaseMipLevel = 0;
		/// Number of mip levels
		textureDesc.mMipLevels = desc.levels_;
		/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
		textureDesc.mSampleCount = SAMPLE_COUNT_1;
		/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
		textureDesc.mSampleQuality = 0;
		/// Internal image format
		textureDesc.mFormat = GetImageFormat(desc.format_);
		/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
		textureDesc.mClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
		/// Flags specifying suitable usage of the texture(UAV, SRV, RTV, DSV)
		textureDesc.mUsage = GetTextureUsage(desc.bindFlags_);
		/// What state will the texture get created in
		textureDesc.mStartState = GetResourceStates(desc.bindFlags_);
		/// Pointer to native texture handle if the texture does not own underlying resource
		//const void*				textureDesc.pNativeHandle = ;
		/// Set whether texture is srgb
		textureDesc.mSrgb = GetSrgbFormat(desc.format_);
		/// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
		textureDesc.mHostVisible = false;
		/// Debug name used in gpu profile
		wchar wName[512] = {0};
		Core::StringConvertUTF8toUTF16(debugName, (i32)strlen(debugName), wName, 512);
		textureDesc.pDebugName = wName;
		//uint32_t*				textureDesc.pSharedNodeIndices = ;
		//uint32_t				textureDesc.mSharedNodeIndexCount = ;
		//uint32_t				textureDesc.mNodeIndex = ;

		return textureDesc;
	}

	::RenderTargetDesc GetRenderTargetDesc(const GPU::TextureDesc& desc, const char* debugName)
	{
		::RenderTargetDesc textureDesc;
		memset(&textureDesc, 0, sizeof(textureDesc));
		/// Type of texture (1D, 2D, 3D, Cube)
		textureDesc.mType = GetRenderTargetType(desc.type_);
		/// Texture creation flags (decides memory allocation strategy, sharing access,...)
		textureDesc.mFlags = GPU::GetTextureCreationFlags(desc.bindFlags_);
		/// Width
		textureDesc.mWidth = desc.width_;
		/// Height
		textureDesc.mHeight = desc.height_;
		/// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
		textureDesc.mDepth = desc.type_ == TextureType::TEX3D ? desc.depth_ : 1;
		/// Start array layer (Should be between 0 and max array layers for this texture)
		textureDesc.mBaseArrayLayer = 0;
		/// Texture array size (Should be 1 if texture is not a texture array or cubemap)
		if(desc.type_ == TextureType::TEXCUBE)
			textureDesc.mArraySize = 6;
		else
			textureDesc.mArraySize = desc.elements_;
		/// Most detailed mip level (Should be between 0 and max mip levels for this texture)
		textureDesc.mBaseMipLevel = 0;
		/// Number of mip levels
		//textureDesc.mMipLevels = desc.levels_;
		/// Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
		textureDesc.mSampleCount = SAMPLE_COUNT_1;
		/// The image quality level. The higher the quality, the lower the performance. The valid range is between zero and the value appropriate for mSampleCount
		textureDesc.mSampleQuality = 0;
		/// Internal image format
		textureDesc.mFormat = GetImageFormat(desc.format_);
		/// Optimized clear value (recommended to use this same value when clearing the rendertarget)
		textureDesc.mClearValue = {0.0f, 0.0f, 0.0f, 0.0f};
		/// Flags specifying suitable usage of the texture(UAV, SRV, RTV, DSV)
		textureDesc.mUsage = GetRenderTargetUsage(desc.bindFlags_, desc.format_);
		/// What state will the texture get created in
		//textureDesc.mStartState = GetResourceStates(desc.bindFlags_);
		/// Pointer to native texture handle if the texture does not own underlying resource
		//const void*				textureDesc.pNativeHandle = ;
		/// Set whether texture is srgb
		textureDesc.mSrgb = GetSrgbFormat(desc.format_);
		/// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
		//textureDesc.mHostVisible = false;
		/// Debug name used in gpu profile
		wchar wName[512] = {0};
		Core::StringConvertUTF8toUTF16(debugName, (i32)strlen(debugName), wName, 512);
		textureDesc.pDebugName = wName;
		//uint32_t*				textureDesc.pSharedNodeIndices = ;
		//uint32_t				textureDesc.mSharedNodeIndexCount = ;
		//uint32_t				textureDesc.mNodeIndex = ;

		return textureDesc;
	}

	::AddressMode GetAddressingMode(GPU::AddressingMode addressMode)
	{
		switch(addressMode)
		{
		case AddressingMode::WRAP:
			return AddressMode::ADDRESS_MODE_REPEAT;
		case AddressingMode::MIRROR:
			return AddressMode::ADDRESS_MODE_MIRROR;
		case AddressingMode::CLAMP:
			return AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE;
		case AddressingMode::BORDER:
			return AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER;
		default:
			DBG_ASSERT(false);
			return AddressMode::ADDRESS_MODE_REPEAT;
		}
	};

	::FilterType GetFilterType(GPU::FilteringMode filterMode, u32 anisotropy)
	{
		switch(filterMode)
		{
		case FilteringMode::NEAREST: return ::FilterType::FILTER_NEAREST;
		case FilteringMode::LINEAR: return ::FilterType::FILTER_LINEAR;
		case FilteringMode::NEAREST_MIPMAP_NEAREST: return ::FilterType::FILTER_NEAREST;
		case FilteringMode::LINEAR_MIPMAP_NEAREST:
			if(anisotropy > 1)
				return ::FilterType::FILTER_BILINEAR_ANISO;
			else
				return ::FilterType::FILTER_BILINEAR;
		case FilteringMode::NEAREST_MIPMAP_LINEAR: return ::FilterType::FILTER_NEAREST;
		case FilteringMode::LINEAR_MIPMAP_LINEAR:
			if(anisotropy > 1)
				return ::FilterType::FILTER_TRILINEAR_ANISO;
			else
				return ::FilterType::FILTER_TRILINEAR;
		default:
			DBG_ASSERT(false);
			return ::FilterType::FILTER_NEAREST;
		}
	}

	::MipMapMode GetMipMapMode(GPU::FilteringMode filterMode)
	{
		switch(filterMode)
		{
		case FilteringMode::NEAREST:
		case FilteringMode::LINEAR:
		case FilteringMode::NEAREST_MIPMAP_NEAREST:
		case FilteringMode::LINEAR_MIPMAP_NEAREST:
			return ::MipMapMode::MIPMAP_MODE_NEAREST;
		case FilteringMode::NEAREST_MIPMAP_LINEAR:
		case FilteringMode::LINEAR_MIPMAP_LINEAR:
			return ::MipMapMode::MIPMAP_MODE_LINEAR;
		default:
			DBG_ASSERT(false);
			return ::MipMapMode::MIPMAP_MODE_NEAREST;
		}
	}

	::SamplerDesc GetSampler(const GPU::SamplerState& state)
	{
		::SamplerDesc desc = {};
		memset(&desc, 0, sizeof(desc));
		desc.mMinFilter = GetFilterType(state.minFilter_, state.maxAnisotropy_);
		desc.mMagFilter = GetFilterType(state.magFilter_, state.maxAnisotropy_);
		desc.mMipMapMode = GetMipMapMode(state.minFilter_);
		desc.mAddressU = GetAddressingMode(state.addressU_);
		desc.mAddressV = GetAddressingMode(state.addressV_);
		desc.mAddressW = GetAddressingMode(state.addressW_);
		desc.mMipLosBias = state.mipLODBias_;
		desc.mMaxAnisotropy = state.maxAnisotropy_;
		//desc.mCompareFunc =;
		return desc;
	}

} // namespace GPU
