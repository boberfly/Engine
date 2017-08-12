#include "gpu_vlk/vlk_backend.h"
#include "gpu_vlk/vlk_device.h"
#include "gpu_vlk/vlk_linear_heap_allocator.h"
#include "core/debug.h"
#include "core/string.h"

#include <utility>

const char *validationLayerNames[] = {
    "VK_LAYER_LUNARG_standard_validation",
};
constexpr size_t validationLayerCount = sizeof(validationLayerNames) / sizeof(validationLayerNames[0]);


const char *requiredExtensionNames[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	KHR_PLATFORM_SURFACE_EXTENSION_NAME // Defined in vlk_types.h
};
constexpr size_t requiredExtensionCount = sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);


const char *requiredDeviceExtensionNames[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
constexpr size_t requiredDeviceExtensionCount = sizeof(requiredDeviceExtensionNames) / sizeof(requiredDeviceExtensionNames[0]);


static bool hasLayer(const std::string &layerName, const Core::Vector<VkLayerProperties> &layerProperties)
{
    for (auto &layer : layerProperties)
    {
        if (layer.layerName == layerName)
            return true;
    }

    return false;
}

static bool hasExtension(const std::string &extensionName, const Core::Vector<VkExtensionProperties> &extensionProperties)
{
    for (auto &extension : extensionProperties)
    {
        if (extension.extensionName == extensionName)
            return true;
    }

    return false;
}

static bool hasValidationLayers(const Core::Vector<VkLayerProperties> &layers)
{
    for (size_t i = 0; i < validationLayerCount; ++i)
    {
        if (!hasLayer(validationLayerNames[i], layers))
            return false;
    }

    return true;
}

static bool hasRequiredExtensions(const Core::Vector<VkExtensionProperties> &extensions)
{
    for (size_t i = 0; i < requiredExtensionCount; ++i)
    {
        if (!hasExtension(requiredExtensionNames[i], extensions))
            return false;
    }

    return true;
}

#if !defined(FINAL)
VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFunction(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
	{
		switch (messageCode)
		{
		case 2: // Unused vertex output
		case 3: // Unused vertex attribute.
			return VK_FALSE;
		default:
			// Ignore this case.
			break;
        }

    }

    DBG_LOG("%s: %d: %s\n", pLayerPrefix, messageCode, pMessage);
    if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
        return VK_TRUE;
    return VK_FALSE;
}
#endif

extern "C" {
EXPORT bool GetPlugin(struct Plugin::Plugin* outPlugin, Core::UUID uuid)
{
	bool retVal = false;

	// Fill in base info.
	if(uuid == Plugin::Plugin::GetUUID() || uuid == GPU::BackendPlugin::GetUUID())
	{
		if(outPlugin)
		{
			outPlugin->systemVersion_ = Plugin::PLUGIN_SYSTEM_VERSION;
			outPlugin->pluginVersion_ = GPU::BackendPlugin::PLUGIN_VERSION;
			outPlugin->uuid_ = GPU::BackendPlugin::GetUUID();
			outPlugin->name_ = "VLK Backend";
			outPlugin->desc_ = "Vulkan backend.";
		}
		retVal = true;
	}

	// Fill in plugin specific.
	if(uuid == GPU::BackendPlugin::GetUUID())
	{
		if(outPlugin)
		{
			auto* plugin = static_cast<GPU::BackendPlugin*>(outPlugin);
			plugin->api_ = "VLK";
			plugin->CreateBackend = [](
			    const GPU::SetupParams& setupParams) -> GPU::IBackend* { return new GPU::VLKBackend(setupParams); };
			plugin->DestroyBackend = [](GPU::IBackend*& backend) {
				delete backend;
				backend = nullptr;
			};
		}
		retVal = true;
	}

	return retVal;
}
}

namespace GPU
{
	VLKBackend::VLKBackend(const SetupParams& setupParams)
	{
		auto retVal = LoadLibraries();
		DBG_ASSERT(retVal == ErrorCode::OK);

		Core::Vector<const char *> instanceLayers;
		Core::Vector<const char *> instanceExtensions;

		VkResult error;

#if !defined(FINAL)
		// Set the instance layers
		u32 instanceLayerCount;
		CHECK_VLK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

		if (instanceLayerCount > 0)
		{
			instanceLayerProperties_.resize(instanceLayerCount);
			CHECK_VLK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties_.data()));

			if (hasValidationLayers(instanceLayerProperties_))
			{
				for (i32 i = 0; i < validationLayerCount; ++i)
					instanceLayers.push_back(validationLayerNames[i]);
			}
		}
#endif
		// Get the extensions
		u32 instanceExtensionCount;
		CHECK_VLK(error = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
		if (error || instanceExtensionCount == 0)
		{
			DBG_LOG("No extensions found.\n");
		}

		instanceExtensionProperties.resize(instanceExtensionCount);
		CHECK_VLK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensionProperties.get()));

		if (!hasRequiredExtensions(instanceExtensionProperties))
		{
			DBG_LOG("Required extensions are missing.\n");
			return false;
		}

		// Enable the required extensions
		for (i32 i = 0; i < requiredExtensionCount; ++i)
			instanceExtensions.push_back(requiredExtensionNames[i]);

#if !defined(FINAL)
		// Enable the debug reporting extension
		bool hasDebugReportExtension = false;
		if (hasExtension("VK_EXT_debug_report", instanceExtensionProperties))
		{
			hasDebugReportExtension = true;
			instanceExtensions.push_back("VK_EXT_debug_report");
		}
#endif
		// TODO: GPU base classes need to somehow get app info to pass to here...
		VkApplicationInfo applicationInfo = {};
		//memset(&applicationInfo, 0, sizeof(applicationInfo));
		applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		applicationInfo.pNext = nullptr;
		applicationInfo.pApplicationName = "Engine App";
		//applicationInfo.pApplicationName = &setupParams.appName;
		applicationInfo.applicationVersion = 0;
		//applicationInfo.applicationVersion = setupParams.appVersion;
		applicationInfo.pEngineName = "Engine";
		//applicationInfo.pEngineName = &setupParams.engineName;
		applicationInfo.engineVersion = 0;
		applicationInfo.apiVersion = 0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.pApplicationInfo = &applicationInfo;
		createInfo.enabledLayerCount = instanceLayers.size();
		createInfo.ppEnabledLayerNames = instanceLayers.data();
		createInfo.enabledExtensionCount = instanceExtensions.size();
		createInfo.ppEnabledExtensionNames = instanceExtensions.data();

		// Create the instace
		CHECK_VLK(vkCreateInstance(&createInfo, nullptr, &vkInstance_));

		// Load the instance functions which are dependent on an instance.
		retVal = LoadVLKInstanceFunctions();
		DBG_ASSERT(retVal == ErrorCode::OK);

#if !defined(FINAL)
	    if (hasDebugReportExtension)
	    {
	        VkDebugReportCallbackCreateInfoEXT debugInfo;
	        memset(&debugInfo, 0, sizeof(debugInfo));
	        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	        debugInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
	            VK_DEBUG_REPORT_WARNING_BIT_EXT |
	            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	        debugInfo.pfnCallback = debugReportCallbackFunction;
	        debugInfo.pUserData = this;

	        error = vkCreateDebugReportCallbackEXT(vkInstance_, &debugInfo, nullptr, &debugReportCallback);
	        if (error)
	            DBG_LOG("Failed to register debug report callback.\n");
	    }
#endif
	}

	VLKBackend::~VLKBackend()
	{
		delete device_;
		physicalDevices_.clear();
		adapterInfos_.clear();
		//dxgiFactory_ = nullptr;

#if !defined(FINAL)
#endif
	}

	i32 VLKBackend::EnumerateAdapters(AdapterInfo* outAdapters, i32 maxAdapters)
	{
		VkResult error;
		if(adapterInfos_.size() == 0)
		{
			u32 gpuCount = 0;
			CHECK_VLK(vkEnumeratePhysicalDevices(vkInstance_, &gpuCount, nullptr));
			if (gpuCount == 0)
			{
			    DBG_LOG("Failed to enumerate the gpus.\n");
			    return -1;
			}
			if (gpuCount > maxAdapters)
				gpuCount = maxAdapters;

			physicalDevices_.resize(gpuCount);
			error = vkEnumeratePhysicalDevices(vkInstance_, &gpuCount, physicalDevices.data());
			if (error)
			{
			    DBG_LOG("Failed to enumerate the gpus.\n");
			    return -1;
			}
			for (i32 i = 0; i < physicalDevices_.size(); ++i)
			{
				VkPhysicalDevice physicalDevice;
				AdapterInfo outAdapter;

				//VkPhysicalDeviceFeatures deviceFeatures;
				VkPhysicalDeviceProperties deviceProperties;
				VkPhysicalDeviceMemoryProperties memoryProperties;
				//vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
				CHECK_VLK(vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties));
				CHECK_VLK(vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties));

				i64 dedicatedVideoMemory = 0;
				i64 dedicatedSystemMemory = 0;
				i64 sharedSystemMemory = 0;

				for (i32 j = 0; j < memoryProperties.memoryHeapCount; ++j)
				{
					auto &heap = memoryProperties.memoryHeaps[j];
					if(heap.flagBits & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
						dedicatedVideoMemory = (i64)heap.size;
				}

				outAdapter.deviceIdx_ = i;
				outAdapter.description_ = deviceProperties.deviceName;
				outAdapter.vendorId_ = deviceProperties.vendorID;
				outAdapter.deviceId_ = deviceProperties.deviceID;
				outAdapter.subSysId_ = 0;
				outAdapter.revision_ = 0;
				outAdapter.dedicatedVideoMemory_ = dedicatedVideoMemory;
				outAdapter.dedicatedSystemMemory_ = dedicatedSystemMemory;
				outAdapter.sharedSystemMemory_ = sharedSystemMemory;

				adapterInfos_.push_back(outAdapter);
				physicalDevices_.push_back(physicalDevice);
			}
		}

		for(i32 idx = 0; idx < maxAdapters; ++idx)
		{
			outAdapters[idx] = adapterInfos_[idx];
		}

		return adapterInfos_.size();
	}

	bool VLKBackend::IsInitialized() const { return !!device_; }

	ErrorCode VLKBackend::Initialize(i32 adapterIdx)
	{
		device_ = new VLKDevice(vkInstance_, physicalDevices_[adapterIdx]);
		if(*device_ == false)
		{
			delete device_;
			device_ = nullptr;
			return ErrorCode::FAIL;
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateSwapChain(Handle handle, const SwapChainDesc& desc, const char* debugName)
	{
		VLKSwapChain swapChain;
		ErrorCode retVal = device_->CreateSwapChain(swapChain, desc, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		swapchainResources_[handle.GetIndex()] = swapChain;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateBuffer(
	    Handle handle, const BufferDesc& desc, const void* initialData, const char* debugName)
	{
		VLKBuffer buffer;
		buffer.desc_ = desc;
		ErrorCode retVal = device_->CreateBuffer(buffer, desc, initialData, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		bufferResources_[handle.GetIndex()] = buffer;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateTexture(
	    Handle handle, const TextureDesc& desc, const TextureSubResourceData* initialData, const char* debugName)
	{
		VLKTexture texture;
		texture.desc_ = desc;
		ErrorCode retVal = device_->CreateTexture(texture, desc, initialData, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		textureResources_[handle.GetIndex()] = texture;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateSamplerState(Handle handle, const SamplerState& state, const char* debugName)
	{
		VLKSamplerState samplerState;

		auto GetAddessingMode = [](AddressingMode addressMode) {
			switch(addressMode)
			{
			case AddressingMode::WRAP:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case AddressingMode::MIRROR:
				return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case AddressingMode::CLAMP:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case AddressingMode::BORDER:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			default:
				DBG_BREAK;
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			}
		};

		auto GetFilteringMode = [](FilteringMode filterMode) {
			switch(filterMode)
			{
			case FilteringMode::NEAREST:
				return VK_FILTER_NEAREST;
			case FilteringMode::LINEAR:
				return VK_FILTER_LINEAR;
			case FilteringMode::NEAREST_MIPMAP_NEAREST:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case FilteringMode::LINEAR_MIPMAP_NEAREST:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case FilteringMode::NEAREST_MIPMAP_LINEAR:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case FilteringMode::LINEAR_MIPMAP_LINEAR:
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			default:
				DBG_BREAK;
				return VK_FILTER_NEAREST;
			}
		};

		auto GetMipmapMode = [](FilteringMode min, FilteringMode mag) {
			if (min == NEAREST && mag == NEAREST)
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			else if (min == LINEAR && mag == NEAREST)
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			else if (min == NEAREST && mag == LINEAR)
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			else if (min == LINEAR && mag == LINEAR)
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			else if (min == LINEAR_MIPMAP_LINEAR && mag == LINEAR_MIPMAP_LINEAR)
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			else
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}

		auto GetBorderColor = [](float borderColor[4]) {
			if (borderColor[0] == 0.0f && borderColor[3] == 0.0f)
				return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			else if (borderColor[0] == 0.0f && borderColor[3] > 0.0f)
				return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			else if (borderColor[0] > 0.0f && borderColor[3] > 0.0f)
				return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			else
				return VK_BORDER_COLOR_TRANSPAENT_BLACK;
		}

		memset(&samplerState, 0, sizeof(samplerState));
		samplerState.desc_.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerState.desc_.pNext = nullptr;
		samplerState.desc_.flags = 0;
		samplerState.desc_.magFilter = GetFilteringMode(state.magFilter_);
		samplerState.desc_.minFilter = GetFilteringMode(state.minFilter_);
		samplerState.desc_.mipmapMode = GetMipmapMode(state.minFilter_, state.magFilter_);
		samplerState.desc_.addressModeU = GetAddessingMode(state.addressU_);
		samplerState.desc_.addressModeV = GetAddessingMode(state.addressV_);
		samplerState.desc_.addressModeW = GetAddessingMode(state.addressW_);
		samplerState.desc_.mipLodBias = state.mipLODBias_;
		samplerState.desc_.anisotropyEnable = state.maxAnisotropy_ > 0;
		samplerState.desc_.maxAnisotropy = state.maxAnisotropy_;
		/*VkBool32                compareEnable;*/
		/*VkCompareOp             compareOp;*/
		samplerState.desc_.minLod = state.minLOD_;
		samplerState.desc_.maxLod = state.maxLOD_;
		samplerState.desc_.borderColor = GetBorderColor(state.borderColor_);
		/*VkBool32                unnormalizedCoordinates;*/

		Core::ScopedWriteLock lock(resLock_);
		samplerStates_[handle.GetIndex()] = samplerState;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateShader(Handle handle, const ShaderDesc& desc, const char* debugName)
	{
		VLKShader shader;
		shader.byteCode_ = new u8[desc.dataSize_];
		shader.byteCodeSize_ = desc.dataSize_;
		memcpy(shader.byteCode_, desc.data_, desc.dataSize_);

		ErrorCode retVal = device_->CreateShader(shader, desc, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		shaders_[handle.GetIndex()] = shader;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateGraphicsPipelineState(
	    Handle handle, const GraphicsPipelineStateDesc& desc, const char* debugName)
	{
		VLKGraphicsPipelineState gps;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
		VkGraphicsPipelineCreateInfo info = {};

		    /*VkStructureType    */                              info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		    /*const void*    */                                  info.pNext = nullptr;
		    /*VkPipelineCreateFlags     */                       info.flags = 0;
		    /*uint32_t       */                                  info.stageCount = ;
		    /*const VkPipelineShaderStageCreateInfo* */          info.pStages = ;
		    /*const VkPipelineVertexInputStateCreateInfo*  */    info.pVertexInputState = &vertexInputState;
		    /*const VkPipelineInputAssemblyStateCreateInfo* */   info.pInputAssemblyState = &inputAssemblyState;
		    /*const VkPipelineTessellationStateCreateInfo* */    info.pTessellationState = &tessellationState;
		    /*const VkPipelineViewportStateCreateInfo*     */    info.pViewportState = &viewportState;
		    /*const VkPipelineRasterizationStateCreateInfo* */   info.pRasterizationState = &rasterizationState;
		    /*const VkPipelineMultisampleStateCreateInfo*  */    info.pMultisampleState = &multisampleState;
		    /*const VkPipelineDepthStencilStateCreateInfo* */    info.pDepthStencilState = &depthStencilState;
		    /*const VkPipelineColorBlendStateCreateInfo* */      info.pColorBlendState = &colorBlendState;
		    /*const VkPipelineDynamicStateCreateInfo* */         info.pDynamicState = &dynamicState;
		    /*VkPipelineLayout   */                              info.layout = layout;
		    /*VkRenderPass */                                    info.renderPass = renderPass;
		    /*uint32_t   */                                      info.subpass = 0;
		    /*VkPipeline  */                                     info.basePipelineHandle = ;
		    /*int32_t   */                                       info.basePipelineIndex = ;




		typedef struct VkPipelineShaderStageCreateInfo {
		    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		    pNext = nullptr;
		    flags = 0;
		    VkShaderStageFlagBits               stage;
		    VkShaderModule                      module;
		    const char*                         pName;
		    const VkSpecializationInfo*         pSpecializationInfo;
		} VkPipelineShaderStageCreateInfo;

		auto GetShaderBytecode = [&](ShaderType shaderType) {
			D3D12_SHADER_BYTECODE byteCode = {};
			auto shaderHandle = desc.shaders_[(i32)shaderType];
			if(shaderHandle)
			{
				const auto& shader = shaders_[shaderHandle.GetIndex()];
				byteCode.pShaderBytecode = shader.byteCode_;
				byteCode.BytecodeLength = shader.byteCodeSize_;
			}
			return byteCode;
		};

		auto GetBlendState = [](BlendState blendState) {
			auto GetBlend = [](BlendType type) {
				switch(type)
				{
				case BlendType::ZERO:
					return VK_BLEND_FACTOR_ZERO;
				case BlendType::ONE:
					return VK_BLEND_FACTOR_ONE;
				case BlendType::SRC_COLOR:
					return VK_BLEND_FACTOR_SRC_COLOR;
				case BlendType::INV_SRC_COLOR:
					return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
				case BlendType::SRC_ALPHA:
					return VK_BLEND_FACTOR_SRC_ALPHA;
				case BlendType::INV_SRC_ALPHA:
					return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				case BlendType::DEST_COLOR:
					return VK_BLEND_FACTOR_DST_COLOR;
				case BlendType::INV_DEST_COLOR:
					return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
				case BlendType::DEST_ALPHA:
					return VK_BLEND_FACTOR_DST_ALPHA;
				case BlendType::INV_DEST_ALPHA:
					return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
				default:
					DBG_BREAK;
					return VK_BLEND_FACTOR_ZERO;
				}
			};

			auto GetBlendOp = [](BlendFunc func) {
				switch(func)
				{
				case BlendFunc::ADD:
					return VK_BLEND_OP_ADD;
				case BlendFunc::SUBTRACT:
					return VK_BLEND_OP_SUBTRACT;
				case BlendFunc::REV_SUBTRACT:
					return VK_BLEND_OP_REVERSE_SUBTRACT;
				case BlendFunc::MINIMUM:
					return VK_BLEND_OP_MIN;
				case BlendFunc::MAXIMUM:
					return VK_BLEND_OP_MAX;
				default:
					DBG_BREAK;
					return VK_BLEND_OP_ADD;
				}
			};

			D3D12_RENDER_TARGET_BLEND_DESC state;
			state.BlendEnable = blendState.enable_ ? TRUE : FALSE;
			state.LogicOpEnable = FALSE;
			state.SrcBlend = GetBlend(blendState.srcBlend_);
			state.DestBlend = GetBlend(blendState.destBlend_);
			state.BlendOp = GetBlendOp(blendState.blendOp_);
			state.SrcBlendAlpha = GetBlend(blendState.srcBlendAlpha_);
			state.DestBlendAlpha = GetBlend(blendState.destBlendAlpha_);
			state.BlendOpAlpha = GetBlendOp(blendState.blendOpAlpha_);
			state.LogicOp = D3D12_LOGIC_OP_CLEAR;
			state.RenderTargetWriteMask = blendState.writeMask_;
			return state;
		};

		auto GetRasterizerState = [](const RenderState& renderState) {
			auto GetFillMode = [](FillMode fillMode) {
				switch(fillMode)
				{
				case FillMode::SOLID:
					return VK_POLYGON_MODE_FILL;
				case FillMode::WIREFRAME:
					return VK_POLYGON_MODE_LINE;
				default:
					DBG_BREAK;
					return VK_POLYGON_MODE_FILL;
				}
			};

			auto GetCullMode = [](CullMode cullMode) {
				switch(cullMode)
				{
				case CullMode::NONE:
					return VK_CULL_MODE_NONE;
				case CullMode::CCW:
					return VK_CULL_MODE_FRONT_BIT;
				case CullMode::CW:
					return VK_CULL_MODE_BACK_BIT;
				default:
					DBG_BREAK;
					return VK_CULL_MODE_NONE;
				}
			};

			VkPipelineRasterizationStateCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			info.pNext = nullptr;
			info.flags = 0;
			info.depthClampEnable = VK_FALSE;
			info.rasterizerDiscardEnable = VK_FALSE;
			info.polygonMode = GetFillMode(renderState.fillMode_);
			info.cullMode = GetCullMode(renderState.cullMode_);
			info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			info.depthBiasEnable = (renderState.depthBias_ > 0.0f) ? VK_TRUE : VK_FALSE;
			info.depthBiasConstantFactor = renderState.depthBias_;
			info.depthBiasClamp = 0.0f;
			info.depthBiasSlopeFactor = renderState.slopeScaledDepthBias_;
			info.lineWidth = 1.0f;

			return info;
		};

		auto GetDepthStencilState = [](const RenderState& renderState) {
			auto GetCompareMode = [](CompareMode mode) {
				switch(mode)
				{
				case CompareMode::NEVER:
					return VK_COMPARE_OP_NEVER;
				case CompareMode::LESS:
					return VK_COMPARE_OP_LESS;
				case CompareMode::EQUAL:
					return VK_COMPARE_OP_EQUAL;
				case CompareMode::LESS_EQUAL:
					return VK_COMPARE_OP_LESS_OR_EQUAL;
				case CompareMode::GREATER:
					return VK_COMPARE_OP_GREATER;
				case CompareMode::NOT_EQUAL:
					return VK_COMPARE_OP_NOT_EQUAL;
				case CompareMode::GREATER_EQUAL:
					return VK_COMPARE_OP_GREATER_OR_EQUAL;
				case CompareMode::ALWAYS:
					return VK_COMPARE_OP_ALWAYS;
				default:
					DBG_BREAK;
					return VK_COMPARE_OP_NEVER;
				}
			};

			auto GetStencilFaceState = [&GetCompareMode](StencilFaceState stencilFaceState) {
				auto GetStencilOp = [](StencilFunc func) {
					switch(func)
					{
					case StencilFunc::KEEP:
						return VK_STENCIL_OP_KEEP;
					case StencilFunc::ZERO:
						return VK_STENCIL_OP_ZERO;
					case StencilFunc::REPLACE:
						return VK_STENCIL_OP_REPLACE;
					case StencilFunc::INCR:
						return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					case StencilFunc::INCR_WRAP:
						return VK_STENCIL_OP_INCREMENT_AND_WRAP;
					case StencilFunc::DECR:
						return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					case StencilFunc::DECR_WRAP:
						return VK_STENCIL_OP_DECREMENT_AND_WRAP;
					case StencilFunc::INVERT:
						return VK_STENCIL_OP_INVERT;
					default:
						DBG_BREAK;
						return VK_STENCIL_OP_KEEP;
					}
				};

				D3D12_DEPTH_STENCILOP_DESC desc;
				desc.StencilFailOp = GetStencilOp(stencilFaceState.fail_);
				desc.StencilDepthFailOp = GetStencilOp(stencilFaceState.depthFail_);
				desc.StencilPassOp = GetStencilOp(stencilFaceState.pass_);
				desc.StencilFunc = GetCompareMode(stencilFaceState.func_);

				return desc;
			};

			D3D12_DEPTH_STENCIL_DESC desc;

			desc.DepthEnable = renderState.depthEnable_;
			desc.DepthWriteMask =
			    renderState.depthWriteMask_ ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
			desc.DepthFunc = GetCompareMode(renderState.depthFunc_);
			desc.StencilEnable = renderState.stencilEnable_;
			desc.StencilReadMask = renderState.stencilRead_;
			desc.StencilWriteMask = renderState.stencilWrite_;
			desc.BackFace = GetStencilFaceState(renderState.stencilBack_);
			desc.FrontFace = GetStencilFaceState(renderState.stencilFront_);

			return desc;
		};

		auto GetTopologyType = [](TopologyType type) {
			switch(type)
			{
			case TopologyType::POINT:
				return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			case TopologyType::LINE:
				return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			case TopologyType::TRIANGLE:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			case TopologyType::PATCH:
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
			default:
				DBG_BREAK;
				return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			}
		};

		auto GetSemanticName = [](VertexUsage usage) {
			switch(usage)
			{
			case VertexUsage::POSITION:
				return "POSITION";
			case VertexUsage::BLENDWEIGHTS:
				return "BLENDWEIGHTS";
			case VertexUsage::BLENDINDICES:
				return "BLENDINDICES";
			case VertexUsage::NORMAL:
				return "NORMAL";
			case VertexUsage::TEXCOORD:
				return "TEXCOORD";
			case VertexUsage::TANGENT:
				return "TANGENT";
			case VertexUsage::BINORMAL:
				return "BINORMAL";
			case VertexUsage::COLOR:
				return "COLOR";
			default:
				DBG_BREAK;
				return "";
			}
		};

		{
			Core::ScopedReadLock lock(resLock_);
			gpsDesc.VS = GetShaderBytecode(ShaderType::VERTEX);
			gpsDesc.GS = GetShaderBytecode(ShaderType::GEOMETRY);
			gpsDesc.HS = GetShaderBytecode(ShaderType::HULL);
			gpsDesc.DS = GetShaderBytecode(ShaderType::DOMAIN);
			gpsDesc.PS = GetShaderBytecode(ShaderType::PIXEL);
		}

		gpsDesc.NodeMask = 0x0;

		gpsDesc.NumRenderTargets = desc.numRTs_;
		gpsDesc.BlendState.AlphaToCoverageEnable = FALSE;
		gpsDesc.BlendState.IndependentBlendEnable = TRUE;
		for(i32 i = 0; i < MAX_BOUND_RTVS; ++i)
		{
			gpsDesc.BlendState.RenderTarget[i] = GetBlendState(desc.renderState_.blendStates_[i]);
			gpsDesc.RTVFormats[i] = GetFormat(desc.rtvFormats_[i]);
		}
		gpsDesc.DSVFormat = GetFormat(desc.dsvFormat_);

		gpsDesc.SampleDesc.Count = 1;
		gpsDesc.SampleDesc.Quality = 0;

		gpsDesc.RasterizerState = GetRasterizerState(desc.renderState_);
		gpsDesc.DepthStencilState = GetDepthStencilState(desc.renderState_);

		gpsDesc.PrimitiveTopologyType = GetTopologyType(desc.topology_);

		gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		Core::Array<D3D12_INPUT_ELEMENT_DESC, 16> elementDesc;
		gpsDesc.InputLayout.NumElements = desc.numVertexElements_;
		gpsDesc.InputLayout.pInputElementDescs = elementDesc.data();
		for(i32 i = 0; i < desc.numVertexElements_; ++i)
		{
			elementDesc[i].SemanticName = GetSemanticName(desc.vertexElements_[i].usage_);
			elementDesc[i].SemanticIndex = desc.vertexElements_[i].usageIdx_;
			elementDesc[i].Format = GetFormat(desc.vertexElements_[i].format_);
			elementDesc[i].AlignedByteOffset = desc.vertexElements_[i].offset_;
			elementDesc[i].InputSlot = desc.vertexElements_[i].streamIdx_;
			elementDesc[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA; // TODO: expose outside of here.
			elementDesc[i].InstanceDataStepRate = 0;                                    // TODO: expose outside of here.
		}
		gps.stencilRef_ = desc.renderState_.stencilRef_;

		ErrorCode retVal = device_->CreateGraphicsPipelineState(gps, gpsDesc, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		graphicsPipelineStates_[handle.GetIndex()] = gps;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateComputePipelineState(
	    Handle handle, const ComputePipelineStateDesc& desc, const char* debugName)
	{
		D3D12ComputePipelineState cps;
		D3D12_COMPUTE_PIPELINE_STATE_DESC cpsDesc = {};

		{
			Core::ScopedReadLock lock(resLock_);
			const auto& shader = shaders_[desc.shader_.GetIndex()];
			cpsDesc.CS.BytecodeLength = shader.byteCodeSize_;
			cpsDesc.CS.pShaderBytecode = shader.byteCode_;
			cpsDesc.NodeMask = 0x0;
		}

		ErrorCode retVal = device_->CreateComputePipelineState(cps, cpsDesc, debugName);
		if(retVal != ErrorCode::OK)
			return retVal;

		Core::ScopedWriteLock lock(resLock_);
		computePipelineStates_[handle.GetIndex()] = cps;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreatePipelineBindingSet(
	    Handle handle, const PipelineBindingSetDesc& desc, const char* debugName)
	{
		D3D12PipelineBindingSet pbs;

		// Grab all resources to create pipeline binding set with.
		Core::Array<ID3D12Resource*, MAX_SRV_BINDINGS> srvResources;
		Core::Array<D3D12_SHADER_RESOURCE_VIEW_DESC, MAX_UAV_BINDINGS> srvs;
		Core::Array<ID3D12Resource*, MAX_SRV_BINDINGS> uavResources;
		Core::Array<D3D12_UNORDERED_ACCESS_VIEW_DESC, MAX_UAV_BINDINGS> uavs;
		Core::Array<D3D12_CONSTANT_BUFFER_VIEW_DESC, MAX_CBV_BINDINGS> cbvs;
		Core::Array<D3D12_SAMPLER_DESC, MAX_SAMPLER_BINDINGS> samplers;

		memset(srvResources.data(), 0, sizeof(srvResources));
		memset(srvs.data(), 0, sizeof(srvs));
		memset(uavResources.data(), 0, sizeof(uavResources));
		memset(uavs.data(), 0, sizeof(uavs));
		memset(cbvs.data(), 0, sizeof(cbvs));
		memset(samplers.data(), 0, sizeof(samplers));

		{
			Core::ScopedReadLock lock(resLock_);
			for(i32 i = 0; i < desc.numSRVs_; ++i)
			{
				auto srvHandle = desc.srvs_[i].resource_;
				DBG_ASSERT(srvHandle.GetType() == ResourceType::BUFFER || srvHandle.GetType() == ResourceType::TEXTURE);
				DBG_ASSERT(srvHandle);

				D3D12Resource* resource = GetD3D12Resource(srvHandle);
				DBG_ASSERT(resource);
				srvResources[i] = resource->resource_.Get();

				const auto& srv = desc.srvs_[i];
				srvs[i].Format = GetFormat(srv.format_);
				srvs[i].ViewDimension = GetSRVDimension(srv.dimension_);
				srvs[i].Shader4ComponentMapping =
				    D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
				        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
				        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
				switch(srv.dimension_)
				{
				case ViewDimension::BUFFER:
					srvs[i].Buffer.FirstElement = srv.mostDetailedMip_FirstElement_;
					srvs[i].Buffer.NumElements = srv.mipLevels_NumElements_;
					srvs[i].Buffer.StructureByteStride = 0;
					srvs[i].Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
					break;
				case ViewDimension::TEX1D:
					srvs[i].Texture1D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].Texture1D.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].Texture1D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEX1D_ARRAY:
					srvs[i].Texture1DArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].Texture1DArray.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].Texture1DArray.ArraySize = srv.arraySize_;
					srvs[i].Texture1DArray.FirstArraySlice = srv.firstArraySlice_;
					srvs[i].Texture1DArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEX2D:
					srvs[i].Texture2D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].Texture2D.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].Texture2D.PlaneSlice = srv.planeSlice_;
					srvs[i].Texture2D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEX2D_ARRAY:
					srvs[i].Texture2DArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].Texture2DArray.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].Texture2DArray.ArraySize = srv.arraySize_;
					srvs[i].Texture2DArray.FirstArraySlice = srv.firstArraySlice_;
					srvs[i].Texture2DArray.PlaneSlice = srv.planeSlice_;
					srvs[i].Texture2DArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEX3D:
					srvs[i].Texture3D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].Texture3D.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].Texture3D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEXCUBE:
					srvs[i].TextureCube.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].TextureCube.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].TextureCube.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				case ViewDimension::TEXCUBE_ARRAY:
					srvs[i].TextureCubeArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
					srvs[i].TextureCubeArray.MipLevels = srv.mipLevels_NumElements_;
					srvs[i].TextureCubeArray.NumCubes = srv.arraySize_;
					srvs[i].TextureCubeArray.First2DArrayFace = srv.firstArraySlice_;
					srvs[i].TextureCubeArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
					break;
				default:
					DBG_BREAK;
					return ErrorCode::FAIL;
					break;
				}
			}

			for(i32 i = 0; i < desc.numUAVs_; ++i)
			{
				auto uavHandle = desc.uavs_[i].resource_;
				DBG_ASSERT(uavHandle.GetType() == ResourceType::BUFFER || uavHandle.GetType() == ResourceType::TEXTURE);
				DBG_ASSERT(uavHandle);

				D3D12Resource* resource = GetD3D12Resource(uavHandle);
				DBG_ASSERT(resource);
				uavResources[i] = resource->resource_.Get();

				const auto& uav = desc.uavs_[i];
				uavs[i].Format = GetFormat(uav.format_);
				uavs[i].ViewDimension = GetUAVDimension(uav.dimension_);
				switch(uav.dimension_)
				{
				case ViewDimension::BUFFER:
					uavs[i].Buffer.FirstElement = uav.mipSlice_FirstElement_;
					uavs[i].Buffer.NumElements = uav.firstArraySlice_FirstWSlice_NumElements_;
					uavs[i].Buffer.StructureByteStride = 0;
					break;
				case ViewDimension::TEX1D:
					uavs[i].Texture1D.MipSlice = uav.mipSlice_FirstElement_;
					break;
				case ViewDimension::TEX1D_ARRAY:
					uavs[i].Texture1DArray.MipSlice = uav.mipSlice_FirstElement_;
					uavs[i].Texture1DArray.ArraySize = uav.arraySize_PlaneSlice_WSize_;
					uavs[i].Texture1DArray.FirstArraySlice = uav.firstArraySlice_FirstWSlice_NumElements_;
					break;
				case ViewDimension::TEX2D:
					uavs[i].Texture2D.MipSlice = uav.mipSlice_FirstElement_;
					uavs[i].Texture2D.PlaneSlice = uav.arraySize_PlaneSlice_WSize_;
					break;
				case ViewDimension::TEX2D_ARRAY:
					uavs[i].Texture2DArray.MipSlice = uav.mipSlice_FirstElement_;
					uavs[i].Texture2DArray.ArraySize = uav.arraySize_PlaneSlice_WSize_;
					uavs[i].Texture2DArray.FirstArraySlice = uav.firstArraySlice_FirstWSlice_NumElements_;
					uavs[i].Texture2DArray.PlaneSlice = uav.arraySize_PlaneSlice_WSize_;
					break;
				case ViewDimension::TEX3D:
					uavs[i].Texture3D.MipSlice = uav.mipSlice_FirstElement_;
					uavs[i].Texture3D.FirstWSlice = uav.firstArraySlice_FirstWSlice_NumElements_;
					uavs[i].Texture3D.WSize = uav.arraySize_PlaneSlice_WSize_;
					break;
				default:
					DBG_BREAK;
					return ErrorCode::FAIL;
					break;
				}
			}

			for(i32 i = 0; i < desc.numCBVs_; ++i)
			{
				auto cbvHandle = desc.cbvs_[i].resource_;
				DBG_ASSERT(cbvHandle.GetType() == ResourceType::BUFFER);
				DBG_ASSERT(cbvHandle);

				const auto& cbv = desc.cbvs_[i];

				cbvs[i].BufferLocation =
				    bufferResources_[cbvHandle.GetIndex()].resource_->GetGPUVirtualAddress() + cbv.offset_;
				cbvs[i].SizeInBytes = cbv.size_;
			}

			for(i32 i = 0; i < desc.numSamplers_; ++i)
			{
				auto samplerHandle = desc.samplers_[i].resource_;
				DBG_ASSERT(samplerHandle);
				samplers[i] = samplerStates_[samplerHandle.GetIndex()].desc_;
			}

			// Get pipeline state.
			if(desc.pipelineState_.GetType() == ResourceType::GRAPHICS_PIPELINE_STATE)
			{
				auto& gps = graphicsPipelineStates_[desc.pipelineState_.GetIndex()];
				pbs.rootSignature_ = gps.rootSignature_;
				pbs.pipelineState_ = gps.pipelineState_;
			}
			else if(desc.pipelineState_.GetType() == ResourceType::COMPUTE_PIPELINE_STATE)
			{
				auto& cps = computePipelineStates_[desc.pipelineState_.GetIndex()];
				pbs.rootSignature_ = cps.rootSignature_;
				pbs.pipelineState_ = cps.pipelineState_;
			}
			else
			{
				DBG_BREAK;
			}

			ErrorCode retVal;
			RETURN_ON_ERROR(retVal = device_->CreatePipelineBindingSet(pbs, desc, debugName));
			RETURN_ON_ERROR(retVal = device_->UpdateCBVs(pbs, 0, desc.numCBVs_, cbvs.data()));
			RETURN_ON_ERROR(retVal = device_->UpdateSRVs(pbs, 0, desc.numSRVs_, srvResources.data(), srvs.data()));
			RETURN_ON_ERROR(retVal = device_->UpdateUAVs(pbs, 0, desc.numUAVs_, uavResources.data(), uavs.data()));
			RETURN_ON_ERROR(retVal = device_->UpdateSamplers(pbs, 0, desc.numSamplers_, samplers.data()));
		}

		Core::ScopedWriteLock lock(resLock_);
		pipelineBindingSets_[handle.GetIndex()] = pbs;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateDrawBindingSet(Handle handle, const DrawBindingSetDesc& desc, const char* debugName)
	{
		D3D12DrawBindingSet dbs;
		memset(&dbs.ib_, 0, sizeof(dbs.ib_));
		memset(dbs.vbs_.data(), 0, sizeof(dbs.vbs_));

		dbs.desc_ = desc;

		{
			Core::ScopedReadLock lock(resLock_);
			if(desc.ib_.resource_)
			{
				D3D12Buffer* buffer = GetD3D12Buffer(desc.ib_.resource_);
				DBG_ASSERT(buffer);

				DBG_ASSERT(Core::ContainsAllFlags(buffer->supportedStates_, D3D12_RESOURCE_STATE_INDEX_BUFFER));
				dbs.ibResource_ = buffer;

				dbs.ib_.BufferLocation = buffer->resource_->GetGPUVirtualAddress() + desc.ib_.offset_;
				dbs.ib_.SizeInBytes = Core::PotRoundUp(desc.ib_.size_, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
				switch(desc.ib_.stride_)
				{
				case 2:
					dbs.ib_.Format = DXGI_FORMAT_R16_UINT;
					break;
				case 4:
					dbs.ib_.Format = DXGI_FORMAT_R32_UINT;
					break;
				default:
					return ErrorCode::FAIL;
					break;
				}
			}

			i32 idx = 0;
			for(const auto& vb : desc.vbs_)
			{
				if(vb.resource_)
				{
					D3D12Buffer* buffer = GetD3D12Buffer(vb.resource_);
					DBG_ASSERT(buffer);
					DBG_ASSERT(Core::ContainsAllFlags(
					    buffer->supportedStates_, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
					dbs.vbResources_[idx] = buffer;

					dbs.vbs_[idx].BufferLocation = buffer->resource_->GetGPUVirtualAddress() + vb.offset_;
					dbs.vbs_[idx].SizeInBytes = Core::PotRoundUp(vb.size_, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
					dbs.vbs_[idx].StrideInBytes = vb.stride_;
				}
				++idx;
			}
		}

		//
		Core::ScopedWriteLock lock(resLock_);
		drawBindingSets_[handle.GetIndex()] = dbs;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateFrameBindingSet(Handle handle, const FrameBindingSetDesc& desc, const char* debugName)
	{
		D3D12FrameBindingSet fbs;

		fbs.desc_ = desc;
		{
			Core::ScopedReadLock lock(resLock_);
			Core::Vector<D3D12_RENDER_TARGET_VIEW_DESC> rtvDescs;
			Core::Vector<D3D12_DEPTH_STENCIL_VIEW_DESC> dsvDescs;

			// Check if we're using a swapchain.
			{
				const auto& rtv = desc.rtvs_[0];
				Handle resource = rtv.resource_;
				if(resource.GetType() == ResourceType::SWAP_CHAIN)
				{
					auto& swapChain = swapchainResources_[resource.GetIndex()];
					fbs.numBuffers_ = swapChain.textures_.size();
					fbs.swapChain_ = &swapChain;
				}
			}

			// Resize to support number of buffers.
			rtvDescs.resize(MAX_BOUND_RTVS * fbs.numBuffers_);
			dsvDescs.resize(fbs.numBuffers_);
			memset(rtvDescs.data(), 0, sizeof(D3D12_RENDER_TARGET_VIEW_DESC) * rtvDescs.size());
			memset(dsvDescs.data(), 0, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC) * dsvDescs.size());
			fbs.rtvResources_.resize(MAX_BOUND_RTVS * fbs.numBuffers_, nullptr);
			fbs.dsvResources_.resize(fbs.numBuffers_, nullptr);

			for(i32 bufferIdx = 0; bufferIdx < fbs.numBuffers_; ++bufferIdx)
			{
				for(i32 rtvIdx = 0; rtvIdx < MAX_BOUND_RTVS; ++rtvIdx)
				{
					auto& rtvDesc = rtvDescs[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
					D3D12Resource*& rtvResource = fbs.rtvResources_[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
					const auto& rtv = desc.rtvs_[rtvIdx];
					Handle resource = rtv.resource_;
					if(resource)
					{
						// Only first element can be a swap chain, and only one RTV can be set if using a swap chain.
						DBG_ASSERT(rtvIdx == 0 || resource.GetType() == ResourceType::TEXTURE);
						DBG_ASSERT(rtvIdx == 0 || !fbs.swapChain_);

						// No holes allowed.
						if(bufferIdx == 0 )
							if(rtvIdx != fbs.numRTs_++)
								return ErrorCode::FAIL;

						D3D12Texture* texture = GetD3D12Texture(resource, bufferIdx);
						DBG_ASSERT(Core::ContainsAllFlags(texture->supportedStates_, D3D12_RESOURCE_STATE_RENDER_TARGET));
						rtvResource = texture;					

						rtvDesc.Format = GetFormat(rtv.format_);
						rtvDesc.ViewDimension = GetRTVDimension(rtv.dimension_);
						switch(rtv.dimension_)
						{
						case ViewDimension::BUFFER:
							return ErrorCode::UNSUPPORTED;
							break;
						case ViewDimension::TEX1D:
							rtvDesc.Texture1D.MipSlice = rtv.mipSlice_;
							break;
						case ViewDimension::TEX1D_ARRAY:
							rtvDesc.Texture1DArray.ArraySize = rtv.arraySize_;
							rtvDesc.Texture1DArray.FirstArraySlice = rtv.firstArraySlice_;
							rtvDesc.Texture1DArray.MipSlice = rtv.mipSlice_;
							break;
						case ViewDimension::TEX2D:
							rtvDesc.Texture2D.MipSlice = rtv.mipSlice_;
							rtvDesc.Texture2D.PlaneSlice = rtv.planeSlice_FirstWSlice_;
							break;
						case ViewDimension::TEX2D_ARRAY:
							rtvDesc.Texture2DArray.MipSlice = rtv.mipSlice_;
							rtvDesc.Texture2DArray.FirstArraySlice = rtv.firstArraySlice_;
							rtvDesc.Texture2DArray.ArraySize = rtv.arraySize_;
							rtvDesc.Texture2DArray.PlaneSlice = rtv.planeSlice_FirstWSlice_;
							break;
						case ViewDimension::TEX3D:
							rtvDesc.Texture3D.FirstWSlice = rtv.planeSlice_FirstWSlice_;
							rtvDesc.Texture3D.MipSlice = rtv.mipSlice_;
							rtvDesc.Texture3D.WSize = rtv.wSize_;
							break;
						default:
							DBG_BREAK;
							return ErrorCode::FAIL;
							break;
						}
					}
				}

				const auto& dsv = desc.dsv_;
				Handle resource = dsv.resource_;
				if(resource)
				{
					auto& dsvDesc = dsvDescs[bufferIdx];
					D3D12Resource*& dsvResource = fbs.dsvResources_[bufferIdx];
					DBG_ASSERT(resource.GetType() == ResourceType::TEXTURE);
					D3D12Texture* texture = GetD3D12Texture(resource);
					DBG_ASSERT(Core::ContainsAnyFlags(
						texture->supportedStates_, D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_DEPTH_READ));
					dsvResource = texture;

					dsvDesc.Format = GetFormat(dsv.format_);
					dsvDesc.ViewDimension = GetDSVDimension(dsv.dimension_);
					switch(dsv.dimension_)
					{
					case ViewDimension::BUFFER:
						return ErrorCode::UNSUPPORTED;
						break;
					case ViewDimension::TEX1D:
						dsvDesc.Texture1D.MipSlice = dsv.mipSlice_;
						break;
					case ViewDimension::TEX1D_ARRAY:
						dsvDesc.Texture1DArray.ArraySize = dsv.arraySize_;
						dsvDesc.Texture1DArray.FirstArraySlice = dsv.firstArraySlice_;
						dsvDesc.Texture1DArray.MipSlice = dsv.mipSlice_;
						break;
					case ViewDimension::TEX2D:
						dsvDesc.Texture2D.MipSlice = dsv.mipSlice_;
						break;
					case ViewDimension::TEX2D_ARRAY:
						dsvDesc.Texture2DArray.MipSlice = dsv.mipSlice_;
						dsvDesc.Texture2DArray.FirstArraySlice = dsv.firstArraySlice_;
						dsvDesc.Texture2DArray.ArraySize = dsv.arraySize_;
						break;
					default:
						DBG_BREAK;
						return ErrorCode::FAIL;
						break;
					}
				}
			}

			ErrorCode retVal;
			RETURN_ON_ERROR(retVal = device_->CreateFrameBindingSet(fbs, fbs.desc_, debugName));
			RETURN_ON_ERROR(retVal = device_->UpdateFrameBindingSet(fbs, rtvDescs.data(), dsvDescs.data()));
		}

		//
		Core::ScopedWriteLock lock(resLock_);
		frameBindingSets_[handle.GetIndex()] = fbs;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateCommandList(Handle handle, const char* debugName)
	{
		D3D12CommandList* commandList = new D3D12CommandList(*device_, 0x0, D3D12_COMMAND_LIST_TYPE_DIRECT);
		Core::ScopedWriteLock lock(resLock_);
		commandLists_[handle.GetIndex()] = commandList;
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::CreateFence(Handle handle, const char* debugName)
	{
		//
		return ErrorCode::UNIMPLEMENTED;
	}

	ErrorCode VLKBackend::DestroyResource(Handle handle)
	{
		Core::ScopedWriteLock lock(resLock_);
		switch(handle.GetType())
		{
		case ResourceType::SWAP_CHAIN:
			swapchainResources_[handle.GetIndex()] = VLKSwapChain();
			break;
		case ResourceType::BUFFER:
			bufferResources_[handle.GetIndex()] = VLKBuffer();
			break;
		case ResourceType::TEXTURE:
			textureResources_[handle.GetIndex()] = VLKTexture();
			break;
		case ResourceType::SHADER:
			delete[] shaders_[handle.GetIndex()].byteCode_;
			shaders_[handle.GetIndex()] = VLKShader();
			break;
		case ResourceType::SAMPLER_STATE:
			samplerStates_[handle.GetIndex()] = VLKSamplerState();
			break;
		case ResourceType::GRAPHICS_PIPELINE_STATE:
			graphicsPipelineStates_[handle.GetIndex()] = VLKGraphicsPipelineState();
			break;
		case ResourceType::COMPUTE_PIPELINE_STATE:
			computePipelineStates_[handle.GetIndex()] = VLKComputePipelineState();
			break;
		case ResourceType::PIPELINE_BINDING_SET:
			device_->DestroyPipelineBindingSet(pipelineBindingSets_[handle.GetIndex()]);
			pipelineBindingSets_[handle.GetIndex()] = VLKPipelineBindingSet();
			break;
		case ResourceType::DRAW_BINDING_SET:
			drawBindingSets_[handle.GetIndex()] = VLKDrawBindingSet();
			break;
		case ResourceType::FRAME_BINDING_SET:
			device_->DestroyFrameBindingSet(frameBindingSets_[handle.GetIndex()]);
			frameBindingSets_[handle.GetIndex()] = VLKFrameBindingSet();
			break;
		case ResourceType::COMMAND_LIST:
			delete commandLists_[handle.GetIndex()];
			commandLists_[handle.GetIndex()] = nullptr;
			break;
		}
		//
		return ErrorCode::UNIMPLEMENTED;
	}

	ErrorCode VLKBackend::CompileCommandList(Handle handle, const CommandList& commandList)
	{
		DBG_ASSERT(handle.GetIndex() < commandLists_.size());

		// Lock resources during command list compilation.
		Core::ScopedReadLock lock(resLock_);

		D3D12CommandList* outCommandList = commandLists_[handle.GetIndex()];
		D3D12CompileContext context(*this);
		return context.CompileCommandList(*outCommandList, commandList);
	}

	ErrorCode VLKBackend::SubmitCommandList(Handle handle)
	{
		D3D12CommandList* commandList = commandLists_[handle.GetIndex()];
		DBG_ASSERT(commandList);
		return device_->SubmitCommandList(*commandList);
	}

	ErrorCode VLKBackend::PresentSwapChain(Handle handle)
	{
		Core::ScopedWriteLock lock(resLock_);
		DBG_ASSERT(handle.GetIndex() < swapchainResources_.size());
		D3D12SwapChain& swapChain = swapchainResources_[handle.GetIndex()];

		HRESULT retVal = S_OK;
		CHECK_D3D(retVal = swapChain.swapChain_->Present(0, 0));
		if(FAILED(retVal))
			return ErrorCode::FAIL;
		swapChain.bbIdx_ = swapChain.swapChain_->GetCurrentBackBufferIndex();
		return ErrorCode::OK;
	}

	ErrorCode VLKBackend::ResizeSwapChain(Handle handle, i32 width, i32 height) { return ErrorCode::UNIMPLEMENTED; }

	void VLKBackend::NextFrame()
	{
		device_->NextFrame();
	}

	ErrorCode VLKBackend::LoadVLKInstanceFunctions()
	{
		// Loop over all instance functions via vkGetInstanceProcAddr
#define VK_IMPORT_INSTANCE_FUNC( func ) \
		if( !(func = (PFN_##func)vkGetInstanceProcAddr( vkInstance_, #func )) ) { \
			return ErrorCode::FAIL; \
		}
#define VK_INSTANCE_FUNC VK_IMPORT_INSTANCE_FUNC
		VK_INSTANCE_FUNCS
		VK_PLATFORM_FUNCS
#undef VK_INSTANCE_FUNC

		return ErrorCode::OK;
	}

	VLKBuffer* VLKBackend::GetVLKBuffer(Handle handle)
	{
		if(handle.GetType() != ResourceType::BUFFER)
			return nullptr;
		if(handle.GetIndex() >= bufferResources_.size())
			return nullptr;
		return &bufferResources_[handle.GetIndex()];
	}

	VLKTexture* VLKBackend::GetVLKTexture(Handle handle, i32 bufferIdx)
	{
		if(handle.GetType() == ResourceType::TEXTURE)
		{
			if(handle.GetIndex() >= textureResources_.size())
				return nullptr;
			return &textureResources_[handle.GetIndex()];
		}
		else if(handle.GetType() == ResourceType::SWAP_CHAIN)
		{
			if(handle.GetIndex() >= swapchainResources_.size())
				return nullptr;
			VLKSwapChain& swapChain = swapchainResources_[handle.GetIndex()];
			return &swapChain.textures_[bufferIdx >= 0 ? bufferIdx : swapChain.bbIdx_];
		}
		return nullptr;
	}

} // namespace GPU
