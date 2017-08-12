#include "gpu_vlk/vlk_device.h"
#include "gpu_vlk/vlk_command_list.h"
#include "gpu_vlk/vlk_linear_heap_allocator.h"
#include "gpu_vlk/vlk_descriptor_heap_allocator.h"
#include "gpu_vlk/private/shaders/default_cs.h"
#include "gpu_vlk/private/shaders/default_vs.h"

#include "core/debug.h"

#define VK_IMPORT_DEVICE_FUNC( func ) \
		if( !(func = (PFN_##func)vkGetDeviceProcAddr( vkDevice_, #func )) ) { \
			return ErrorCode::FAIL; \
		}

static bool hasRequiredDeviceExtensions(const Core::Vector<VkExtensionProperties> &extensions)
{
	for (size_t i = 0; i < requiredDeviceExtensionCount; ++i)
	{
		if (!hasExtension(requiredDeviceExtensionNames[i], extensions))
			return false;
	}

	return true;
}

namespace GPU
{
	VLKDevice::VLKDevice(VkInstance instance, VkPhysicalDevice physicalDevice)
	    : vkInstance_(instance), vkPhysicalDevice_(physicalDevice)
	{
		Core::Vector<const char *> deviceLayers;
		Core::Vector<const char *> deviceExtensions;

		VkResult error;

		u32 deviceLayerCount;
		CHECK_VLK(vkEnumerateDeviceLayerProperties(vkPhysicalDevice_, &deviceLayerCount, nullptr));

		deviceLayerProperties_.resize(deviceLayerCount);
		CHECK_VLK(vkEnumerateDeviceLayerProperties(vkPhysicalDevice_, &deviceLayerCount, deviceLayerProperties.data()));

		u32 deviceExtensionCount;
		CHECK_VLK(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &deviceExtensionCount, nullptr));

		deviceExtensionProperties_.resize(deviceExtensionCount);
		CHECK_VLK(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &deviceExtensionCount, deviceExtensionProperties.data()));

		if (!hasRequiredDeviceExtensions(deviceExtensionProperties_))
		{
			DBG_LOG("The device is missing some required extensions.");
		}

		// Enable the required device extensions.
		for (i32 i = 0; i < requiredDeviceExtensionCount; ++i)
			deviceExtensions.push_back(requiredDeviceExtensionNames[i]);

		u32 queueFamilyCount;
		CHECK_VLK(vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &queueFamilyCount, nullptr));
		if (queueFamilyCount == 0)
		{
			DBG_LOG("Cannot find any queues on device!");
		}

		queueProperties_.resize(queueFamilyCount);
		CHECK_VLK(vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice_, &queueFamilyCount, queueProperties_.data()));

		// Open all the availables queues.
		Core::Vector<VkDeviceQueueCreateInfo> createQueueInfos;

		u32 maxQueueCount = 0;
		for (i32 i = 0; i < queueFamilyCount; ++i)
		    maxQueueCount = Core::Max(maxQueueCount, queueProperties[i].queueCount);

		Core::Vector<float> queuePriorities(maxQueueCount);
		for (i32 i = 0; i < queueFamilyCount; ++i)
		{
		    auto &queueProps = queueProperties[i];
		    VkDeviceQueueCreateInfo createQueueInfo;
		    memset(&createQueueInfo, 0, sizeof(createQueueInfo));

		    createQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		    createQueueInfo.queueFamilyIndex = i;
		    createQueueInfo.queueCount = queueProps.queueCount;
		    createQueueInfo.pQueuePriorities = &queuePriorities[0];
		    createQueueInfos.push_back(createQueueInfo);
		}

		VkDeviceCreateInfo deviceCreateInfo;
		memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = createQueueInfos.size();
		deviceCreateInfo.pQueueCreateInfos = createQueueInfos.data();
		if (hasValidationLayers(deviceLayerProperties))
		{
		    for (i32 i = 0; i < validationLayerCount; ++i)
		        deviceLayers.push_back(validationLayerNames[i]);
		}

		// Set the device layers and extensions
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();

		if (!deviceLayers.empty())
		{
		    deviceCreateInfo.ppEnabledLayerNames = deviceLayers.data();
		    deviceCreateInfo.enabledLayerCount = deviceLayers.size();
		}

		CHECK_VLK(error = vkCreateDevice(vkPhysicalDevice_, &createInfo, nullptr, &vkDevice_));
		if (error)
		{
			DBG_LOG("Cannot create Device!");
		}

		// Load the device functions which are dependent on a device.
		auto retVal = LoadVLKDeviceFunctions();
		DBG_ASSERT(retVal == ErrorCode::OK);

		// device created, setup command queues.
		CreateCommandQueues();

		// setup root signatures.
		CreateRootSignatures();

		// setup default PSOs.
		CreateDefaultPSOs();

		// setup upload allocator.
		CreateUploadAllocators();

		// setup descriptor heap allocators.
		CreateDescriptorHeapAllocators();

		// Frame fence.
		d3dDevice_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)d3dFrameFence_.GetAddressOf());
		frameFenceEvent_ = ::CreateEvent(nullptr, FALSE, FALSE, "Frame fence");
	}

	VLKDevice::~VLKDevice()
	{ //
		::CloseHandle(frameFenceEvent_);
		::CloseHandle(uploadFenceEvent_);
		delete uploadCommandList_;
		for(auto& d3dUploadAllocator : uploadAllocators_)
			delete d3dUploadAllocator;

		delete cbvSrvUavAllocator_;
		delete samplerAllocator_;
		delete rtvAllocator_;
		delete dsvAllocator_;
	}

	ErrorCode VLKDevice::LoadVLKDeviceFunctions()
	{
		// Loop over all device functions via vkGetDeviceProcAddr
#define VK_DEVICE_FUNC VK_IMPORT_DEVICE_FUNC
		VK_DEVICE_FUNCS
#undef VK_DEVICE_FUNC

		return ErrorCode::OK;
	}

	void VLKDevice::CreateCommandQueues()
	{
		// Get the queues.
		for (i32 i = 0; i < queueProperties_.size(); ++i)
		{
		    auto &familyProperties = queueProperties[i];
		    auto isDirect = (familyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
		    auto isAsyncCompute = (familyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
		    auto isCopy = (familyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;
		    if (!isGraphics && !isCompute && !isTransfer)
		        continue;

		    for (i32 j = 0; j < familyProperties.queueCount; ++j)
		    {
				VkQueue queue;
				CHECK_VLK(vkGetDeviceQueue(vkDevice_, i, j, &queue));

				if (isDirect)
					vkDirectQueues_.push_back(queue);
				else if(isCopy)
					vkCopyQueues_.push_back(queue);
				else
					vkAsyncComputeQueues_.push_back(queue);
				}
			}
		}
#if !defined(FINAL)
		if (vkDirectQueues_.size() > 0)
			SetObjectName(VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, (u64)vkDirectQueues_[0], "Direct Command Queue");
		if (vkCopyQueues_.size() > 0)
			SetObjectName(VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, (u64)vkCopyQueues_[0], "Copy Command Queue");
		if (vkAsyncComputeQueues_.size() > 0)
			SetObjectName(VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, (u64)vkAsyncComputeQueues_[0], "Async Compute Command Queue");
#endif
	}

	void VLKDevice::CreateRootSignatures()
	{
		HRESULT hr = S_OK;
		ComPtr<ID3DBlob> outBlob, errorBlob;

		// Setup descriptor ranges.
		D3D12_DESCRIPTOR_RANGE descriptorRanges[4];
		descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		descriptorRanges[0].NumDescriptors = MAX_SAMPLER_BINDINGS;
		descriptorRanges[0].BaseShaderRegister = 0;
		descriptorRanges[0].RegisterSpace = 0;
		descriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

		descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descriptorRanges[1].NumDescriptors = MAX_CBV_BINDINGS;
		descriptorRanges[1].BaseShaderRegister = 0;
		descriptorRanges[1].RegisterSpace = 0;
		descriptorRanges[1].OffsetInDescriptorsFromTableStart = 0;

		descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRanges[2].NumDescriptors = MAX_SRV_BINDINGS;
		descriptorRanges[2].BaseShaderRegister = 0;
		descriptorRanges[2].RegisterSpace = 0;
		descriptorRanges[2].OffsetInDescriptorsFromTableStart = 0;

		descriptorRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		descriptorRanges[3].NumDescriptors = MAX_UAV_BINDINGS;
		descriptorRanges[3].BaseShaderRegister = 0;
		descriptorRanges[3].RegisterSpace = 0;
		descriptorRanges[3].OffsetInDescriptorsFromTableStart = 0;

		d3dRootSignatures_.resize((i32)RootSignatureType::MAX);

		auto CreateRootSignature = [&](D3D12_ROOT_SIGNATURE_DESC& desc, RootSignatureType type) {
			hr = D3D12SerializeRootSignatureFn(
			    &desc, D3D_ROOT_SIGNATURE_VERSION_1, outBlob.GetAddressOf(), errorBlob.GetAddressOf());
			if(FAILED(hr))
			{
				const void* bufferData = errorBlob->GetBufferPointer();
				Core::Log(reinterpret_cast<const char*>(bufferData));
			}
			CHECK_D3D(hr = d3dDevice_->CreateRootSignature(0, outBlob->GetBufferPointer(), outBlob->GetBufferSize(),
			              IID_PPV_ARGS(d3dRootSignatures_[(i32)type].GetAddressOf())));
		};

		D3D12_ROOT_PARAMETER parameters[16];
		auto SetupParams = [&](i32 base, D3D12_SHADER_VISIBILITY visibility) {
			parameters[base].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[base].DescriptorTable.NumDescriptorRanges = 1;
			parameters[base].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
			parameters[base].ShaderVisibility = visibility;

			parameters[base + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[base + 1].DescriptorTable.NumDescriptorRanges = 1;
			parameters[base + 1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
			parameters[base + 1].ShaderVisibility = visibility;

			parameters[base + 2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[base + 2].DescriptorTable.NumDescriptorRanges = 1;
			parameters[base + 2].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
			parameters[base + 2].ShaderVisibility = visibility;
		};

		// GRAPHICS
		{
			// Setup sampler, srv, and cbv for all stages.
			SetupParams(0, D3D12_SHADER_VISIBILITY_ALL);

			// Now shared UAV for all.
			parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[3].DescriptorTable.NumDescriptorRanges = 1;
			parameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[3];
			parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.NumParameters = 4;
			rootSignatureDesc.NumStaticSamplers = 0;
			rootSignatureDesc.pParameters = parameters;
			rootSignatureDesc.pStaticSamplers = nullptr;
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			CreateRootSignature(rootSignatureDesc, RootSignatureType::GRAPHICS);
		}

		// COMPUTE
		{
			// Setup sampler, srv, and cbv for all stages.
			SetupParams(0, D3D12_SHADER_VISIBILITY_ALL);

			// Now shared UAV for all.
			parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[3].DescriptorTable.NumDescriptorRanges = 1;
			parameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[3];
			parameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.NumParameters = 4;
			rootSignatureDesc.NumStaticSamplers = 0;
			rootSignatureDesc.pParameters = parameters;
			rootSignatureDesc.pStaticSamplers = nullptr;
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			                          D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			                          D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			                          D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			                          D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

			CreateRootSignature(rootSignatureDesc, RootSignatureType::COMPUTE);
		}
	}

	void VLKDevice::CreateDefaultPSOs()
	{
		HRESULT hr = S_OK;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPSO = {};
		memset(&defaultPSO, 0, sizeof(defaultPSO));

		auto CreateGraphicsPSO = [&](RootSignatureType type) {
			D3D12_INPUT_ELEMENT_DESC InputElementDescs[] = {
			    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			};
			D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPSO = {};
			defaultPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			defaultPSO.InputLayout.NumElements = ARRAYSIZE(InputElementDescs);
			defaultPSO.InputLayout.pInputElementDescs = InputElementDescs;
			defaultPSO.VS.pShaderBytecode = g_VShader;
			defaultPSO.VS.BytecodeLength = ARRAYSIZE(g_VShader);
			defaultPSO.pRootSignature = d3dRootSignatures_[(i32)type].Get();
			defaultPSO.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			defaultPSO.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			defaultPSO.NumRenderTargets = 1;
			defaultPSO.SampleDesc.Count = 1;
			defaultPSO.SampleDesc.Quality = 0;
			defaultPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

			ComPtr<ID3D12PipelineState> pipelineState;
			CHECK_D3D(hr = d3dDevice_->CreateGraphicsPipelineState(
			              &defaultPSO, IID_ID3D12PipelineState, (void**)pipelineState.GetAddressOf()));
			d3dDefaultPSOs_.push_back(pipelineState);
		};

		auto CreateComputePSO = [&](RootSignatureType type) {
			D3D12_COMPUTE_PIPELINE_STATE_DESC defaultPSO = {};
			defaultPSO.CS.pShaderBytecode = g_CShader;
			defaultPSO.CS.BytecodeLength = ARRAYSIZE(g_CShader);
			defaultPSO.pRootSignature = d3dRootSignatures_[(i32)type].Get();

			ComPtr<ID3D12PipelineState> pipelineState;
			CHECK_D3D(hr = d3dDevice_->CreateComputePipelineState(
			              &defaultPSO, IID_ID3D12PipelineState, (void**)pipelineState.GetAddressOf()));
			d3dDefaultPSOs_.push_back(pipelineState);
		};

		CreateGraphicsPSO(RootSignatureType::GRAPHICS);
		CreateComputePSO(RootSignatureType::COMPUTE);
	}

	void VLKDevice::CreateUploadAllocators()
	{
		for(auto& d3dUploadAllocator : uploadAllocators_)
			d3dUploadAllocator = new D3D12LinearHeapAllocator(d3dDevice_.Get(), D3D12_HEAP_TYPE_UPLOAD, 1024 * 1024);
		uploadCommandList_ = new D3D12CommandList(*this, 0, D3D12_COMMAND_LIST_TYPE_COPY);

		d3dDevice_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_ID3D12Fence, (void**)d3dUploadFence_.GetAddressOf());
		uploadFenceEvent_ = ::CreateEvent(nullptr, FALSE, FALSE, "Upload fence");
	}

	void VLKDevice::CreateDescriptorHeapAllocators()
	{
		cbvSrvUavAllocator_ = new D3D12DescriptorHeapAllocator(d3dDevice_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, Core::Min(32768, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1),
		    "CBV, SRV, and UAV Descriptor Heap");
		samplerAllocator_ = new D3D12DescriptorHeapAllocator(d3dDevice_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
		    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE,
		    "Sampler Descriptor Heap");
		rtvAllocator_ = new D3D12DescriptorHeapAllocator(d3dDevice_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		    D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1024, "RTV Descriptor Heap");
		dsvAllocator_ = new D3D12DescriptorHeapAllocator(d3dDevice_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		    D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1024, "DSV Descriptor Heap");
	}

	void VLKDevice::NextFrame()
	{
		if((frameIdx_ - d3dFrameFence_->GetCompletedValue()) >= MAX_GPU_FRAMES)
		{
			d3dFrameFence_->SetEventOnCompletion(frameIdx_ - MAX_GPU_FRAMES, frameFenceEvent_);
			::WaitForSingleObject(frameFenceEvent_, INFINITE);
		}

		frameIdx_++;

		// Reset upload allocators as we go along.
		GetUploadAllocator().Reset();
		d3dDirectQueue_->Signal(d3dFrameFence_.Get(), frameIdx_);
	}

	ErrorCode VLKDevice::CreateSwapChain(
	    VLKSwapChain& outResource, const SwapChainDesc& desc, const char* debugName)
	{

		VkResult error = VK_SUCCESS;
		swapChainWidth = desc.width_;
		swapChainHeight = desc.height_;

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		CHECK_VLK(error = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, surface, &surfaceCapabilities));
		if (error)
			return ErrorCode::FAIL;

		uint32_t presentModeCount;
		CHECK_VLK(error = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, surface, &presentModeCount, nullptr));
		if (error)
			return ErrorCode::FAIL;

		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		CHECK_VLK(error = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, surface, &presentModeCount, &presentModes[0]));
		if (error)
			return ErrorCode::FAIL;


		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		memset(&swapChainDesc, 0, sizeof(swapChainDesc));
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Width = desc.width_;
		swapChainDesc.Height = desc.height_;
		swapChainDesc.Format = GetFormat(desc.format_);
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.BufferCount = desc.bufferCount_;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;


		VkExtent2D swapchainExtent;
		if (surfaceCapabilities.currentExtent.width == (uint32_t)-1)
		{
			swapchainExtent.width = swapChainWidth;
			swapchainExtent.height = swapChainHeight;
		}
		else
		{
			swapchainExtent = surfaceCapabilities.currentExtent;
			swapChainWidth = swapchainExtent.width;
			swapChainHeight = swapchainExtent.height;
		}

		VkPresentModeKHR swapchainPresentMode = presentModes[0];
		for (i32 i = 0; i < presentModeCount; i++) {
			if (presentModes[i] == VK_PRESENT_MODE_FIFO_KHR) {
				swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
			}

			if (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
				swapchainPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
				break;
			}
		}

	u32 desiredNumberOfSwapchainImages = Core::Max(surfaceCapabilities.minImageCount, desc.bufferCount_);
	if ((surfaceCapabilities.maxImageCount > 0) &&
	    (desiredNumberOfSwapchainImages > surfaceCapabilities.maxImageCount))
	    desiredNumberOfSwapchainImages = surfaceCapabilities.maxImageCount;
    VkSurfaceTransformFlagsKHR preTransform;
    if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    memset(&swapchainInfo, 0, sizeof(swapchainInfo));
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = desiredNumberOfSwapchainImages;
    swapchainInfo.imageFormat = GetFormat(desc.format_);
    swapchainInfo.imageColorSpace = colorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.presentMode = swapchainPresentMode;
    swapchainInfo.oldSwapchain = handle;

    error = CHECK_VLK(vkCreateSwapchainKHR(vkDevice_, &swapchainInfo, nullptr, &outResource.swapChain_));
    if (error)
		return ErrorCode::FAIL;

	u32 imageCount = 0;
    error = CHECK_VLK(vkGetSwapchainImagesKHR(vkDevice_, outResource.swapChain_, &imageCount, nullptr));
    if (error)
		return ErrorCode::FAIL;

	//Core::Vector<VkImage> swapChainImages(imageCount);
	outResource.images_.resize(imageCount);
	error = CHECK_VLK(vkGetSwapchainImagesKHR(vkDevice_, outResource.swapChain_, &imageCount, outResource.images_.data()));
	if (error)
		return ErrorCode::FAIL;

    // Color buffers descriptions
    agpu_texture_description colorDesc;
    memset(&colorDesc, 0, sizeof(colorDesc));
    colorDesc.type = AGPU_TEXTURE_2D;
    colorDesc.width = createInfo->width;
    colorDesc.height = createInfo->height;
    colorDesc.depthOrArraySize = 1;
    colorDesc.format = agpuFormat;
    colorDesc.flags = agpu_texture_flags(AGPU_TEXTURE_FLAG_RENDER_TARGET | AGPU_TEXTURE_FLAG_RENDERBUFFER_ONLY);
    colorDesc.miplevels = 1;
    colorDesc.sample_count = 1;

    // Depth stencil buffer descriptions.
    agpu_texture_description depthStencilDesc;
    memset(&depthStencilDesc, 0, sizeof(depthStencilDesc));
    depthStencilDesc.type = AGPU_TEXTURE_2D;
    depthStencilDesc.width = createInfo->width;
    depthStencilDesc.height = createInfo->height;
    depthStencilDesc.depthOrArraySize = 1;
    depthStencilDesc.format = createInfo->depth_stencil_format;
    depthStencilDesc.flags = agpu_texture_flags(AGPU_TEXTURE_FLAG_RENDERBUFFER_ONLY);
    depthStencilDesc.miplevels = 1;

    bool hasDepth = hasDepthComponent(createInfo->depth_stencil_format);
    bool hasStencil = hasStencilComponent(createInfo->depth_stencil_format);
    if (hasDepth)
        depthStencilDesc.flags = agpu_texture_flags(depthStencilDesc.flags | AGPU_TEXTURE_FLAG_DEPTH);
    if (hasStencil)
        depthStencilDesc.flags = agpu_texture_flags(depthStencilDesc.flags | AGPU_TEXTURE_FLAG_STENCIL);

    agpu_texture_view_description colorViewDesc;
    agpu_texture_view_description depthStencilViewDesc;
    auto depthStencilViewPointer = &depthStencilViewDesc;
    if (!hasDepth && !hasStencil)
        depthStencilViewPointer = nullptr;

    // Create the semaphores
    VkSemaphoreCreateInfo semaphoreInfo;
    memset(&semaphoreInfo, 0, sizeof(semaphoreInfo));
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphores.resize(imageCount, VK_NULL_HANDLE);
    currentSemaphoreIndex = 0;
    for (size_t i = 0; i < imageCount; ++i)
    {
        error = vkCreateSemaphore(device->device, &semaphoreInfo, nullptr, &semaphores[i]);
        if(error)
            return false;
    }

    VkClearColorValue clearColor;
    memset(&clearColor, 0, sizeof(clearColor));
    framebuffers.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i)
    {
        auto colorImage = swapChainImages[i];
        VkImageSubresourceRange range;
        memset(&range, 0, sizeof(range));
        range.layerCount = 1;
        range.levelCount = 1;
        if (!device->clearImageWithColor(colorImage, range, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VkAccessFlagBits(0), &clearColor))
            return false;

        agpu_texture *colorBuffer = nullptr;
        agpu_texture *depthStencilBuffer = nullptr;

        {
            colorBuffer = agpu_texture::createFromImage(device, &colorDesc, colorImage);
            if (!colorBuffer)
                return false;

            // Get the color buffer view description.
            colorBuffer->getFullViewDescription(&colorViewDesc);
        }

        {
            depthStencilBuffer = agpu_texture::create(device, &depthStencilDesc);
            if (!depthStencilBuffer)
            {
                colorBuffer->release();
                return false;
            }

            // Get the depth stencil buffer view description.
            depthStencilBuffer->getFullViewDescription(&depthStencilViewDesc);
        }

        auto framebuffer = agpu_framebuffer::create(device, swapChainWidth, swapChainHeight, 1, &colorViewDesc, depthStencilViewPointer);
        framebuffer->swapChainFramebuffer = true;
        framebuffers[i] = framebuffer;

        // Release the references to the buffers.
        colorBuffer->release();
        depthStencilBuffer->release();

        if (!framebuffer)
            return false;
    }

    if (!getNextBackBufferIndex())
        return false;

    return true;



		DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
		memset(&swapChainDesc, 0, sizeof(swapChainDesc));
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Width = desc.width_;
		swapChainDesc.Height = desc.height_;
		swapChainDesc.Format = GetFormat(desc.format_);
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.BufferCount = desc.bufferCount_;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;

		HRESULT hr = S_OK;
		D3D12SwapChain swapChainRes;
		ComPtr<IDXGISwapChain1> swapChain;
		CHECK_D3D(hr = dxgiFactory_->CreateSwapChainForHwnd(d3dDirectQueue_.Get(), (HWND)desc.outputWindow_,
		              &swapChainDesc, nullptr, nullptr, swapChain.GetAddressOf()));
		if(hr != S_OK)
			return ErrorCode::FAIL;

		swapChain.As(&swapChainRes.swapChain_);
		swapChainRes.textures_.resize(swapChainDesc.BufferCount);

		// Setup swapchain resources.
		TextureDesc texDesc;
		texDesc.type_ = TextureType::TEX2D;
		texDesc.bindFlags_ = BindFlags::RENDER_TARGET | BindFlags::PRESENT;
		texDesc.format_ = desc.format_;
		texDesc.width_ = desc.width_;
		texDesc.height_ = desc.height_;
		texDesc.depth_ = 1;
		texDesc.elements_ = 1;
		texDesc.levels_ = 1;

		for(i32 i = 0; i < swapChainRes.textures_.size(); ++i)
		{
			auto& texResource = swapChainRes.textures_[i];

			// Get buffer from swapchain.
			CHECK_D3D(hr = swapChainRes.swapChain_->GetBuffer(
			              i, IID_ID3D12Resource, (void**)texResource.resource_.ReleaseAndGetAddressOf()));

			// Setup states.
			texResource.supportedStates_ = GetResourceStates(texDesc.bindFlags_);
			texResource.defaultState_ = GetDefaultResourceState(texDesc.bindFlags_);

			// Setup texture desc.
			texResource.desc_ = texDesc;
		}

		outResource = swapChainRes;

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::CreateBuffer(
	    D3D12Resource& outResource, const BufferDesc& desc, const void* initialData, const char* debugName)
	{
		ErrorCode errorCode = ErrorCode::OK;
		outResource.supportedStates_ = GetResourceStates(desc.bindFlags_);
		outResource.defaultState_ = GetDefaultResourceState(desc.bindFlags_);

		// Add on copy src/dest flags to supported flags, copy dest to default.
		outResource.supportedStates_ |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		outResource.supportedStates_ |= D3D12_RESOURCE_STATE_COPY_DEST;

		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0x0;
		heapProperties.VisibleNodeMask = 0x0;

		D3D12_RESOURCE_DESC resourceDesc = GetResourceDesc(desc);
		ComPtr<ID3D12Resource> d3dResource;
		HRESULT hr = S_OK;
		CHECK_D3D(hr = d3dDevice_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		              outResource.defaultState_, nullptr, IID_PPV_ARGS(d3dResource.GetAddressOf())));
		if(FAILED(hr))
			return ErrorCode::FAIL;

		outResource.resource_ = d3dResource;
		SetObjectName(d3dResource.Get(), debugName);

		// Use copy queue to upload resource initial data.
		if(initialData)
		{
			auto& uploadAllocator = GetUploadAllocator();
			auto resAlloc = uploadAllocator.Alloc(resourceDesc.Width);
			memcpy(resAlloc.address_, initialData, desc.size_);

			// Do upload.
			Core::ScopedMutex lock(uploadMutex_);
			if(auto* d3dCommandList = uploadCommandList_->Open())
			{
				D3D12ScopedResourceBarrier copyBarrier(
				    d3dCommandList, d3dResource.Get(), 0, outResource.defaultState_, D3D12_RESOURCE_STATE_COPY_DEST);
				d3dCommandList->CopyBufferRegion(d3dResource.Get(), 0, resAlloc.baseResource_.Get(),
				    resAlloc.offsetInBaseResource_, resourceDesc.Width);
			}
			else
			{
				errorCode = ErrorCode::FAIL;
				DBG_BREAK;
			}

			CHECK_ERRORCODE(errorCode = uploadCommandList_->Close());
			CHECK_ERRORCODE(errorCode = uploadCommandList_->Submit(d3dCopyQueue_.Get()));

			d3dCopyQueue_->Signal(d3dUploadFence_.Get(), Core::AtomicInc(&uploadFenceIdx_));

			return errorCode;
		}

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::CreateTexture(D3D12Resource& outResource, const TextureDesc& desc,
	    const TextureSubResourceData* initialData, const char* debugName)
	{
		ErrorCode errorCode = ErrorCode::OK;
		outResource.supportedStates_ = GetResourceStates(desc.bindFlags_);
		outResource.defaultState_ = GetDefaultResourceState(desc.bindFlags_);

		// Add on copy src/dest flags to supported flags, copy dest to default.
		outResource.supportedStates_ |= D3D12_RESOURCE_STATE_COPY_SOURCE;
		outResource.supportedStates_ |= D3D12_RESOURCE_STATE_COPY_DEST;

		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL::D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0x0;
		heapProperties.VisibleNodeMask = 0x0;

		D3D12_RESOURCE_DESC resourceDesc = GetResourceDesc(desc);

		// Set default clear.
		D3D12_CLEAR_VALUE clearValue;
		D3D12_CLEAR_VALUE* setClearValue = nullptr;
		memset(&clearValue, 0, sizeof(clearValue));

		// Setup initial bind type to be whatever is likely what it will be used as first.
		if(Core::ContainsAllFlags(desc.bindFlags_, BindFlags::RENDER_TARGET))
		{
			clearValue.Format = GetFormat(desc.format_);
			setClearValue = &clearValue;
		}
		else if(Core::ContainsAllFlags(desc.bindFlags_, BindFlags::DEPTH_STENCIL))
		{
			clearValue.Format = GetFormat(desc.format_);
			clearValue.DepthStencil.Depth = 0.0f;
			clearValue.DepthStencil.Stencil = 0;
			setClearValue = &clearValue;
		}

		ComPtr<ID3D12Resource> d3dResource;
		HRESULT hr = S_OK;
		CHECK_D3D(hr = d3dDevice_->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		              outResource.defaultState_, setClearValue, IID_PPV_ARGS(d3dResource.GetAddressOf())));
		if(FAILED(hr))
			return ErrorCode::FAIL;

		outResource.resource_ = d3dResource;
		SetObjectName(d3dResource.Get(), debugName);

		// Use copy queue to upload resource initial data.
		if(initialData)
		{
			i32 numSubRsc = desc.levels_ * desc.elements_;
			if(desc.type_ == TextureType::TEXCUBE)
			{
				numSubRsc *= 6;
			}

			Core::Vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
			Core::Vector<i32> numRows;
			Core::Vector<i64> rowSizeInBytes;
			i64 totalBytes = 0;

			layouts.resize(numSubRsc);
			numRows.resize(numSubRsc);
			rowSizeInBytes.resize(numSubRsc);

			d3dDevice_->GetCopyableFootprints(&resourceDesc, 0, numSubRsc, 0, layouts.data(), (u32*)numRows.data(),
			    (u64*)rowSizeInBytes.data(), (u64*)&totalBytes);


			auto& uploadAllocator = GetUploadAllocator();
			auto resAlloc = uploadAllocator.Alloc(totalBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
			u8* dstData = (u8*)resAlloc.address_;
			for(i32 i = 0; i < numSubRsc; ++i)
			{
				auto& srcLayout = initialData[i];
				auto& dstLayout = layouts[i];
				u8* srcData = (u8*)srcLayout.data_;

				DBG_ASSERT(srcLayout.rowPitch_ <= rowSizeInBytes[i]);
				dstData = (u8*)resAlloc.address_ + dstLayout.Offset;
				for(i32 slice = 0; slice < desc.depth_; ++slice)
				{
					u8* rowSrcData = srcData;
					for(i32 row = 0; row < numRows[i]; ++row)
					{
						memcpy(dstData, srcData, srcLayout.rowPitch_);
						dstData += rowSizeInBytes[i];
						srcData += srcLayout.rowPitch_;
					}
					rowSrcData += srcLayout.slicePitch_;
				}
			}

			// Do upload.
			Core::ScopedMutex lock(uploadMutex_);
			if(auto* d3dCommandList = uploadCommandList_->Open())
			{
				D3D12ScopedResourceBarrier copyBarrier(
				    d3dCommandList, d3dResource.Get(), 0, outResource.defaultState_, D3D12_RESOURCE_STATE_COPY_DEST);

				for(i32 i = 0; i < numSubRsc; ++i)
				{
					D3D12_TEXTURE_COPY_LOCATION dst;
					dst.pResource = d3dResource.Get();
					dst.SubresourceIndex = i;
					dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

					D3D12_TEXTURE_COPY_LOCATION src;
					src.pResource = resAlloc.baseResource_.Get();
					src.PlacedFootprint = layouts[i];
					src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

					d3dCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
				}
			}
			else
			{
				errorCode = ErrorCode::FAIL;
				DBG_BREAK;
			}

			CHECK_ERRORCODE(errorCode = uploadCommandList_->Close());
			CHECK_ERRORCODE(errorCode = uploadCommandList_->Submit(d3dCopyQueue_.Get()));

			d3dCopyQueue_->Signal(d3dUploadFence_.Get(), Core::AtomicInc(&uploadFenceIdx_));

			return errorCode;
		}

		return errorCode;
	}

	ErrorCode VLKDevice::CreateShader(VLKShader& outShader, const ShaderDesc& desc, const char* debugName)
	{
		VkShaderModuleCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = 0;
		info.codeSize = desc.dataSize_;
		info.pCode_ = &shader.byteCode_;

		VkResult vkr = VK_SUCCESS;
		CHECK_VLK(vkr = vkCreateShaderModule(vkDevice_, &info, &outShader.shaderModule_));
		if(vkr != VK_SUCCESS)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::CreateGraphicsPipelineState(
	    VLKGraphicsPipelineState& outGps, VkGraphicsPipelineCreateInfo& info, const char* debugName)
	{
		//desc.pRootSignature = d3dRootSignatures_[(i32)RootSignatureType::GRAPHICS].Get();

		VkResult vkr = VK_SUCCESS;
		CHECK_VLK(vkr = vkCreateGraphicsPipelines(vkDevice_, VK_NULL_HANDLE, 1, &info, nullptr, &outGps.pipelineState_));

		if(vkr != VK_SUCCESS)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::CreateComputePipelineState(
	    D3D12ComputePipelineState& outCps, D3D12_COMPUTE_PIPELINE_STATE_DESC desc, const char* debugName)
	{
		//desc.pRootSignature = d3dRootSignatures_[(i32)RootSignatureType::COMPUTE].Get();

		HRESULT hr = S_OK;
		CHECK_D3D(hr = d3dDevice_->CreateComputePipelineState(
		              &desc, IID_ID3D12PipelineState, (void**)outCps.pipelineState_.ReleaseAndGetAddressOf()));
		if(vkr != VK_SUCCESS)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::CreatePipelineBindingSet(
	    D3D12PipelineBindingSet& outPipelineBindingSet, const PipelineBindingSetDesc& desc, const char* debugName)
	{
		auto allocCbvSrvUav = cbvSrvUavAllocator_->Alloc(desc.numCBVs_, desc.numSRVs_, desc.numUAVs_);

		i32 incr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		outPipelineBindingSet.cbvs_ = allocCbvSrvUav;
		allocCbvSrvUav.cpuDescHandle_.ptr += incr * MAX_CBV_BINDINGS;
		allocCbvSrvUav.gpuDescHandle_.ptr += incr * MAX_CBV_BINDINGS;
		allocCbvSrvUav.offset_ += MAX_CBV_BINDINGS;

		outPipelineBindingSet.srvs_ = allocCbvSrvUav;
		allocCbvSrvUav.cpuDescHandle_.ptr += incr * MAX_SRV_BINDINGS;
		allocCbvSrvUav.gpuDescHandle_.ptr += incr * MAX_SRV_BINDINGS;
		allocCbvSrvUav.offset_ += MAX_SRV_BINDINGS;

		outPipelineBindingSet.uavs_ = allocCbvSrvUav;
		allocCbvSrvUav.cpuDescHandle_.ptr += incr * MAX_UAV_BINDINGS;
		allocCbvSrvUav.gpuDescHandle_.ptr += incr * MAX_UAV_BINDINGS;
		allocCbvSrvUav.offset_ += MAX_UAV_BINDINGS;

		outPipelineBindingSet.samplers_ = samplerAllocator_->Alloc(desc.numSamplers_);

		return ErrorCode::OK;
	}

	void VLKDevice::DestroyPipelineBindingSet(D3D12PipelineBindingSet& pipelineBindingSet)
	{
		cbvSrvUavAllocator_->Free(pipelineBindingSet.cbvs_);
		// cbvAllocator_->Free(pipelineBindingSet.srvs_);
		// uavAllocator_->Free(pipelineBindingSet.uavs_);
		samplerAllocator_->Free(pipelineBindingSet.samplers_);
	}

	ErrorCode VLKDevice::CreateFrameBindingSet(
	    D3D12FrameBindingSet& outFrameBindingSet, const FrameBindingSetDesc& desc, const char* debugName)
	{
		outFrameBindingSet.rtvs_ = rtvAllocator_->Alloc(MAX_BOUND_RTVS * outFrameBindingSet.numBuffers_);
		outFrameBindingSet.dsv_ = dsvAllocator_->Alloc(outFrameBindingSet.numBuffers_);
		return ErrorCode::OK;
	}

	void VLKDevice::DestroyFrameBindingSet(D3D12FrameBindingSet& frameBindingSet)
	{
		rtvAllocator_->Free(frameBindingSet.rtvs_);
		dsvAllocator_->Free(frameBindingSet.dsv_);
	}

	ErrorCode VLKDevice::UpdateSRVs(D3D12PipelineBindingSet& pipelineBindingSet, i32 first, i32 num,
	    ID3D12Resource** resources, const D3D12_SHADER_RESOURCE_VIEW_DESC* descs)
	{
		i32 incr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = pipelineBindingSet.srvs_.cpuDescHandle_;
		handle.ptr += first * incr;
		for(i32 i = 0; i < num; ++i)
		{
			d3dDevice_->CreateShaderResourceView(resources[i], &descs[i], handle);
			handle.ptr += incr;
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::UpdateUAVs(D3D12PipelineBindingSet& pipelineBindingSet, i32 first, i32 num,
	    ID3D12Resource** resources, const D3D12_UNORDERED_ACCESS_VIEW_DESC* descs)
	{
		i32 incr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = pipelineBindingSet.uavs_.cpuDescHandle_;
		handle.ptr += first * incr;
		for(i32 i = 0; i < num; ++i)
		{
			d3dDevice_->CreateUnorderedAccessView(resources[i], nullptr, &descs[i], handle);
			handle.ptr += incr;
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::UpdateCBVs(
	    D3D12PipelineBindingSet& pipelineBindingSet, i32 first, i32 num, const D3D12_CONSTANT_BUFFER_VIEW_DESC* descs)
	{
		i32 incr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = pipelineBindingSet.cbvs_.cpuDescHandle_;
		handle.ptr += first * incr;
		for(i32 i = 0; i < num; ++i)
		{
			d3dDevice_->CreateConstantBufferView(&descs[i], handle);
			handle.ptr += incr;
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::UpdateSamplers(
	    D3D12PipelineBindingSet& pipelineBindingSet, i32 first, i32 num, const D3D12_SAMPLER_DESC* descs)
	{
		i32 incr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE handle = pipelineBindingSet.samplers_.cpuDescHandle_;
		handle.ptr += first * incr;
		for(i32 i = 0; i < num; ++i)
		{
			d3dDevice_->CreateSampler(&descs[i], handle);
			handle.ptr += incr;
		}
		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::UpdateFrameBindingSet(D3D12FrameBindingSet& inOutFbs,
	    const D3D12_RENDER_TARGET_VIEW_DESC* rtvDescs, const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDescs)
	{
		i32 rtvIncr = d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = inOutFbs.rtvs_.cpuDescHandle_;

		for(i32 bufferIdx = 0; bufferIdx < inOutFbs.numBuffers_; ++bufferIdx)
		{
			for(i32 rtvIdx = 0; rtvIdx < MAX_BOUND_RTVS; ++rtvIdx)
			{
				D3D12Resource* rtvResource = inOutFbs.rtvResources_[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
				if(rtvResource)
				{
					d3dDevice_->CreateRenderTargetView(
					    rtvResource->resource_.Get(), &rtvDescs[rtvIdx + bufferIdx * MAX_BOUND_RTVS], rtvHandle);
				}
				rtvHandle.ptr += rtvIncr;
			}

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = inOutFbs.dsv_.cpuDescHandle_;
			D3D12Resource* dsvResource = inOutFbs.dsvResources_[bufferIdx];
			if(dsvResource)
			{
				d3dDevice_->CreateDepthStencilView(dsvResource->resource_.Get(), &dsvDescs[bufferIdx], dsvHandle);
			}
		}

		return ErrorCode::OK;
	}

	ErrorCode VLKDevice::SubmitCommandList(D3D12CommandList& commandList)
	{
		d3dDirectQueue_->Wait(d3dUploadFence_.Get(), uploadFenceIdx_);
		return commandList.Submit(d3dDirectQueue_.Get());
	}

} // namespace GPU
