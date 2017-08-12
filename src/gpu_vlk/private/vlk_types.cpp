#include "gpu_vlk/vlk_types.h"
#include "core/concurrency.h"
#include "core/debug.h"
#include "core/misc.h"
#include "core/string.h"

namespace GPU
{
	bool Initialized = false;
	Core::LibHandle VLKHandle = 0;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#define VK_GLOBAL_FUNC( func ) extern PFN_##func func = nullptr;
	VK_GLOBAL_FUNCS
#undef VK_GLOBAL_FUNC

	ErrorCode LoadLibraries()
	{
		if(Initialized)
			return ErrorCode::OK;

		VLKHandle = Core::LibraryOpen(
#if PLATFORM_WINDOWS
					"vulkan-1.dll"
#elif PLATFORM_ANDROID
					"libvulkan.so"
#else
					"libvulkan.so.1"
#endif
		);

		if(!VLKHandle)
			return ErrorCode::FAIL;
		// Load symbols.
		vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)Core::LibrarySymbol(VLKHandle, "vkGetInstanceProcAddr");

		if(!vkGetInstanceProcAddr)
			return ErrorCode::FAIL;

		// Load symbol loops via vkGetInstanceProcAddr
#define VK_IMPORT_GLOBAL_FUNC( func ) \
		if( !(func = (PFN_##func)vkGetInstanceProcAddr( nullptr, #func )) ) { \
			return ErrorCode::FAIL; \
		}
#define VK_GLOBAL_FUNC VK_IMPORT_GLOBAL_FUNC
		VK_GLOBAL_FUNCS
#undef VK_GLOBAL_FUNC

		// Done.
		return ErrorCode::OK;
	}

	VkBufferUsageFlags GetBufferUsageFlags(BindFlags bindFlags)
	{
		VkBufferUsageFlags retVal = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if(Core::ContainsAnyFlags(bindFlags, BindFlags::CONSTANT_BUFFER))
			retVal |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if(Core::ContainsAnyFlags(bindFlags, BindFlags::VERTEX_BUFFER))
			retVal |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::INDEX_BUFFER))
			retVal |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::INDIRECT_BUFFER))
			retVal |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::UNORDERED_ACCESS))
			retVal |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::SHADER_RESOURCE) && Core::ContainsAllFlags(bindFlags, BindFlags::CONSTANT_BUFFER))
			retVal |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::SHADER_RESOURCE) && Core::ContainsAllFlags(bindFlags, BindFlags::UNORDERED_ACCESS))
			retVal |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
		return retVal;
	}

	VkImageUsageFlags GetImageUsageFlags(BindFlags bindFlags)
	{
		VkImageUsageFlags retVal = VK_IMAGE_LAYOUT_TRANSFER_SRC_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::RENDER_TARGET))
			retVal |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::DEPTH_STENCIL))
			retVal |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::UNORDERED_ACCESS))
			retVal |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if(Core::ContainsAllFlags(bindFlags, BindFlags::SHADER_RESOURCE))
			retVal |= VK_IMAGE_USAGE_SAMPLED_BIT;
		return retVal;
	}

	VkImageType GetImageType(TextureType type)
	{
		switch(type)
		{
		case TextureType::TEX1D:
			return VK_IMAGE_TYPE_1D;
		case TextureType::TEX2D:
			return VK_IMAGE_TYPE_2D;
		case TextureType::TEX3D:
			return VK_IMAGE_TYPE_3D;
		case TextureType::TEXCUBE:
			return VK_IMAGE_TYPE_2D;
		default:
			return VK_IMAGE_TYPE_1D;
		}
	}

	VkImageViewType GetImageViewType(ViewDimension dim)
	{
		switch(dim)
		{
		case ViewDimension::BUFFER:
		case ViewDimension::TEX1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case ViewDimension::TEX1D_ARRAY:
			return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		case ViewDimension::TEX2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		case ViewDimension::TEX2D_ARRAY:
			return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case ViewDimension::TEX3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		case ViewDimension::TEXCUBE:
			return VK_IMAGE_VIEW_TYPE_CUBE;
		case ViewDimension::TEXCUBE_ARRAY:
			return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
		default:
			return VK_IMAGE_VIEW_TYPE_1D;
		}
	}

	VkFormat GetFormat(Format format)
	{
		switch (format)
		{
		case INVALID:                   return VK_FORMAT_UNDEFINED;
		case R32G32B32A32_TYPELESS:     return VK_FORMAT_R32G32B32A32_UINT;
		case R32G32B32A32_FLOAT:        return VK_FORMAT_R32G32B32A32_SFLOAT;
		case R32G32B32A32_UINT:         return VK_FORMAT_R32G32B32A32_UINT;
		case R32G32B32A32_SINT:         return VK_FORMAT_R32G32B32A32_SINT;
		case R32G32B32_TYPELESS:        return VK_FORMAT_R32G32B32_UINT;
		case R32G32B32_FLOAT:           return VK_FORMAT_R32G32B32_SFLOAT;
		case R32G32B32_UINT:            return VK_FORMAT_R32G32B32_UINT;
		case R32G32B32_SINT:            return VK_FORMAT_R32G32B32_SINT;
		case R16G16B16A16_TYPELESS:     return VK_FORMAT_R16G16B16A16_UINT;
		case R16G16B16A16_FLOAT:        return VK_FORMAT_R16G16B16A16_SFLOAT;
		case R16G16B16A16_UNORM:        return VK_FORMAT_R16G16B16A16_UNORM;
		case R16G16B16A16_UINT:         return VK_FORMAT_R16G16B16A16_UINT;
		case R16G16B16A16_SNORM:        return VK_FORMAT_R16G16B16A16_SNORM;
		case R16G16B16A16_SINT:         return VK_FORMAT_R16G16B16A16_SINT;
		case R32G32_TYPELESS:           return VK_FORMAT_R32G32_UINT;
		case R32G32_FLOAT:              return VK_FORMAT_R32G32_SFLOAT;
		case R32G32_UINT:               return VK_FORMAT_R32G32_UINT;
		case R32G32_SINT:               return VK_FORMAT_R32G32_SINT;
		case R32G8X24_TYPELESS:         return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case D32_FLOAT_S8X24_UINT:      return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case R32_FLOAT_S8X24_TYPELESS:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case X32_TYPELESS_G8X24_UINT:   return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case R10G10B10A2_TYPELESS:      return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case R10G10B10A2_UNORM:         return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case R10G10B10A2_UINT:          return VK_FORMAT_A2B10G10R10_UINT_PACK32;
		case R11G11B10_FLOAT:           return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case R8G8B8A8_TYPELESS:         return VK_FORMAT_R8G8B8A8_UINT;
		case R8G8B8A8_UNORM:            return VK_FORMAT_R8G8B8A8_UNORM;
		case R8G8B8A8_UNORM_SRGB:       return VK_FORMAT_R8G8B8A8_SRGB;
		case R8G8B8A8_UINT:             return VK_FORMAT_R8G8B8A8_UINT;
		case R8G8B8A8_SNORM:            return VK_FORMAT_R8G8B8A8_SNORM;
		case R8G8B8A8_SINT:             return VK_FORMAT_R8G8B8A8_SINT;
		case R16G16_TYPELESS:           return VK_FORMAT_R16G16_UINT;
		case R16G16_FLOAT:              return VK_FORMAT_R16G16_SFLOAT;
		case R16G16_UNORM:              return VK_FORMAT_R16G16_UNORM;
		case R16G16_UINT:               return VK_FORMAT_R16G16_UINT;
		case R16G16_SNORM:              return VK_FORMAT_R16G16_SNORM;
		case R16G16_SINT:               return VK_FORMAT_R16G16_SINT;
		case R32_TYPELESS:              return VK_FORMAT_R32_UINT;
		case D32_FLOAT:                 return VK_FORMAT_D32_SFLOAT;
		case R32_FLOAT:                 return VK_FORMAT_R32_SFLOAT;
		case R32_UINT:                  return VK_FORMAT_R32_UINT;
		case R32_SINT:                  return VK_FORMAT_R32_SINT;
		case R24G8_TYPELESS:            return VK_FORMAT_D24_UNORM_S8_UINT;
		case D24_UNORM_S8_UINT:         return VK_FORMAT_D24_UNORM_S8_UINT;
		case R24_UNORM_X8_TYPELESS:     return VK_FORMAT_D24_UNORM_S8_UINT;
		case X24TG8_UINT:               return VK_FORMAT_D24_UNORM_S8_UINT;
		case R8G8_TYPELESS:             return VK_FORMAT_R8G8_UINT;
		case R8G8_UNORM:                return VK_FORMAT_R8G8_UNORM;
		case R8G8_UINT:                 return VK_FORMAT_R8G8_UINT;
		case R8G8_SNORM:                return VK_FORMAT_R8G8_SNORM;
		case R8G8_SINT:                 return VK_FORMAT_R8G8_SINT;
		case R16_TYPELESS:              return VK_FORMAT_R16_UINT;
		case R16_FLOAT:                 return VK_FORMAT_R16_SFLOAT;
		case D16_UNORM:                 return VK_FORMAT_D16_UNORM;
		case R16_UNORM:                 return VK_FORMAT_R16_UINT;
		case R16_UINT:                  return VK_FORMAT_R16_UINT;
		case R16_SNORM:                 return VK_FORMAT_R16_SNORM;
		case R16_SINT:                  return VK_FORMAT_R16_SINT;
		case R8_TYPELESS:               return VK_FORMAT_R8_UINT;
		case R8_UNORM:                  return VK_FORMAT_R8_UNORM;
		case R8_UINT:                   return VK_FORMAT_R8_UINT;
		case R8_SNORM:                  return VK_FORMAT_R8_SNORM;
		case R8_SINT:                   return VK_FORMAT_R8_SINT;
		case A8_UNORM:                  return VK_FORMAT_R8_UNORM;
		case R1_UNORM:                  return VK_FORMAT_UNDEFINED;
		case BC1_TYPELESS:              return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case BC1_UNORM:                 return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case BC1_UNORM_SRGB:            return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case BC2_TYPELESS:              return VK_FORMAT_BC2_UNORM_BLOCK;
		case BC2_UNORM:                 return VK_FORMAT_BC2_UNORM_BLOCK;
		case BC2_UNORM_SRGB:            return VK_FORMAT_BC2_SRGB_BLOCK;
		case BC3_TYPELESS:              return VK_FORMAT_BC3_UNORM_BLOCK;
		case BC3_UNORM:                 return VK_FORMAT_BC3_UNORM_BLOCK;
		case BC3_UNORM_SRGB:            return VK_FORMAT_BC3_SRGB_BLOCK;
		case BC4_TYPELESS:              return VK_FORMAT_BC4_UNORM_BLOCK;
		case BC4_UNORM:                 return VK_FORMAT_BC4_UNORM_BLOCK;
		case BC4_SNORM:                 return VK_FORMAT_BC4_SNORM_BLOCK;
		case BC5_TYPELESS:              return VK_FORMAT_BC5_UNORM_BLOCK;
		case BC5_UNORM:                 return VK_FORMAT_BC5_UNORM_BLOCK;
		case BC5_SNORM:                 return VK_FORMAT_BC5_SNORM_BLOCK;
		case B5G6R5_UNORM:              return VK_FORMAT_B5G6R5_UNORM_PACK16;
		case B5G5R5A1_UNORM:            return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case B8G8R8A8_UNORM:            return VK_FORMAT_B8G8R8A8_UNORM;
		case B8G8R8X8_UNORM:            return VK_FORMAT_B8G8R8_UNORM;
		case B8G8R8A8_TYPELESS:         return VK_FORMAT_B8G8R8A8_UINT;
		case B8G8R8A8_UNORM_SRGB:       return VK_FORMAT_B8G8R8A8_SRGB;
		case B8G8R8X8_TYPELESS:         return VK_FORMAT_B8G8R8_UINT;
		case B8G8R8X8_UNORM_SRGB:       return VK_FORMAT_B8G8R8_SRGB;
		default: return VK_FORMAT_UNDEFINED;
		}
	}

	VkPrimitiveTopology GetPrimitiveTopology(PrimitiveTopology topology)
	{
		switch(topology)
		{
		case PrimitiveTopology::POINT_LIST:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case PrimitiveTopology::LINE_LIST:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case PrimitiveTopology::LINE_STRIP:
			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case PrimitiveTopology::LINE_LIST_ADJ:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
		case PrimitiveTopology::LINE_STRIP_ADJ:
			return VK_PRIMITIVE_TOPOLOGY_LINESTRIP_WITH_ADJACENCY;
		case PrimitiveTopology::TRIANGLE_LIST:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case PrimitiveTopology::TRIANGLE_STRIP:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case PrimitiveTopology::TRIANGLE_LIST_ADJ:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
		case PrimitiveTopology::TRIANGLE_STRIP_ADJ:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
		case PrimitiveTopology::TRIANGLE_STRIP_ADJ:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
		case PrimitiveTopology::PATCH_LIST:
			return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
		default:
			DBG_BREAK;
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		}
	}

	VkBufferCreateInfo GetBufferInfo(const BufferDesc& desc)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		//VkBufferCreateFlags    flags;
		bufferInfo.size = Core::PotRoundUp(desc.size_, 256);
		bufferInfo.usage = GetBufferUsageFlags(desc.bindFlags_);
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		return bufferInfo;
	}

	VkImageCreateInfo GetImageInfo(const TextureDesc& desc)
	{
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.pNext = nullptr;
		/*VkImageCreateFlags */      //imageInfo.flags = ;
		imageInfo.imageType = GetImageType(desc.type_);
		imageInfo.format = GetFormat(desc.format_);
		imageInfo.extent.width = desc.width_;
		imageInfo.extent.height = desc.height_;
		imageInfo.extent.depth = desc.type_ == TextureType::TEX3D ? desc.depth_ : 1;
		imageInfo.mipLevels = desc.levels_;
		imageInfo.arrayLayers = desc.elements_;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = GetImageUsageFlags(desc.bindFlags_);
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if(desc.type_ == TextureType::TEXCUBE)
		{
			imageInfo.arrayLayers *= 6;
		}

		return imageInfo;
	}

	void SetObjectName(VkDevice device, VkDebugReportObjectTypeEXT objectType, u64 object, const char* name)
	{
#if !defined(FINAL)
		if(name)
		{
			VkDebugMarkerObjectNameInfoEXT = nameInfo = {};
			nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
			nameInfo.objectType = objectType;
			nameInfo.object = (uint64_t)object;
			nameInfo.pObjectName = name;
			pfnDebugMarkerSetObjectName(device, &nameInfo);
		}
#endif
	}

	void WaitOnFence(VkDevice device, VkFence fence)
	{
		auto result = vkGetFenceStatus(device, fence);
		if (result == VK_NOT_READY)
			CHECK_VLK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
		else
			CHECK_VLK(result);

		// Reset the fence.
		CHECK_VLK(vkResetFences(device, 1, &fence));
	}


} // namespace GPU
