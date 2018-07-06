#include "gpu_tf/tf_backend.h"
#include "gpu_tf/tf_device.h"
//#include "gpu_tf/tf_linear_heap_allocator.h"
//#include "gpu_tf/tf_linear_descriptor_allocator.h"
#include "core/debug.h"
#include "core/string.h"

#include <utility>

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
#ifdef ENGINE_THEFORGE_VLK
			outPlugin->name_ = "VLK Backend";
			outPlugin->desc_ = "Vulkan backend.";
#elif ENGINE_THEFORGE_DX12
			outPlugin->name_ = "D3D12 Backend";
			outPlugin->desc_ = "DirectX 12 backend.";
#elif ENGINE_THEFORGE_MTL
			outPlugin->name_ = "MTL Backend";
			outPlugin->desc_ = "Metal 2 backend.";
#endif
		}
		retVal = true;
	}

	// Fill in plugin specific.
	if(uuid == GPU::BackendPlugin::GetUUID())
	{
		if(outPlugin)
		{
			auto* plugin = static_cast<GPU::BackendPlugin*>(outPlugin);
#ifdef ENGINE_THEFORGE_VLK
			plugin->api_ = "VLK";
#elif ENGINE_THEFORGE_DX12
			plugin->api_ = "DX12";
#elif ENGINE_THEFORGE_MTL
			plugin->api_ = "MTL";
#endif
			plugin->CreateBackend = [](
			    const GPU::SetupParams& setupParams) -> GPU::IBackend* { return new GPU::TFBackend(setupParams); };
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

//namespace GPU
//{
	GPU::TFBackend::TFBackend(const SetupParams& setupParams)
	    : setupParams_(setupParams)
	    , swapchainResources_("TFSwapChain")
	    , bufferResources_("TFBuffer")
	    , textureResources_("TFTexture")
	    , shaders_("TFShader")
	    , graphicsPipelineStates_("TFGraphicsPipelineState")
	    , computePipelineStates_("TFComputePipelineState")
	    , pipelineBindingSets_("TFPipelineBindingSet")
	    , drawBindingSets_("TFDrawBindingSet")
	    , frameBindingSets_("TFFrameBindingSet")
	    , commandLists_("TFCommandList")
		, fences_("TFFence")
		, semaphores_("TFSemaphore")
	{
#if !defined(_RELEASE)
#endif
	}

	GPU::TFBackend::~TFBackend()
	{
		::removeResourceLoaderInterface(renderer_);
		::removeRenderer(renderer_);
		delete renderer_;
	}

	i32 GPU::TFBackend::EnumerateAdapters(GPU::AdapterInfo* outAdapters, i32 maxAdapters)
	{
		if(renderer_ && adapterInfos_.size() == 0)
		{
			for(i32 idx = 0; idx < renderer_->mNumOfGPUs; ++idx)
			{
				AdapterInfo outAdapter;

				outAdapter.deviceIdx_ = idx;
				outAdapter.description_[512] = renderer_->mGpuSettings[idx].mGpuVendorPreset.mGpuName;
				outAdapter.dedicatedVideoMemory_ = 0;
				outAdapter.dedicatedSystemMemory_ = 0;
				outAdapter.sharedSystemMemory_ = 0;
#ifdef ENGINE_THEFORGE_VLK
				outAdapter.vendorId_ = renderer_->mVkGpuProperties[idx].vendorID;
				outAdapter.deviceId_ = renderer_->mVkGpuProperties[idx].deviceID;
				outAdapter.subSysId_ 0;
				outAdapter.revision_ = 0;
#elif ENGINE_THEFORGE_DX12
				//outAdapter.vendorId_ = renderer_->mVkGpuProperties[idx].vendorID;
				//outAdapter.deviceId_ = renderer_->mVkGpuProperties[idx].deviceID;
				outAdapter.subSysId_ 0;
				outAdapter.revision_ = 0;
#elif ENGINE_THEFORGE_MTL
				//outAdapter.vendorId_ = renderer_->mVkGpuProperties[idx].vendorID;
				//outAdapter.deviceId_ = renderer_->mVkGpuProperties[idx].deviceID;
				outAdapter.subSysId_ 0;
				outAdapter.revision_ = 0;
#endif
				outAdapter.dedicatedVideoMemory_ = 0;
				outAdapter.dedicatedSystemMemory_ = 0;
				outAdapter.sharedSystemMemory_ = 0;
			}
		}

		for(i32 idx = 0; idx < maxAdapters; ++idx)
		{
			outAdapters[idx] = adapterInfos_[idx];
		}

		return adapterInfos_.size();
	}

	bool GPU::TFBackend::IsInitialized() const { return !!renderer_; }

	GPU::ErrorCode GPU::TFBackend::Initialize(i32 adapterIdx)
	{
		RendererDesc settings = { 0 };
#ifdef ENGINE_THEFORGE_VLK
		settings.mApi = RENDERER_API_VULKAN;
#elif ENGINE_THEFORGE_DX12
		settings.mApi = RENDERER_API_D3D12;
#elif ENGINE_THEFORGE_MTL
		settings.mApi = RENDERER_API_METAL;
#endif
		settings.mGpuMode = GPU_MODE_SINGLE;
		::initRenderer("Engine", &settings, &renderer_);
		if(!renderer_)
		{
			renderer_ = nullptr;
			return ErrorCode::FAIL;
		}
		// Resource loader so that we can create buffers/textures/shaders
		::initResourceLoaderInterface(renderer_, DEFAULT_MEMORY_BUDGET, false);

		// Create static samplers
		CreateStaticSamplers();

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateSwapChain(Handle handle, const GPU::SwapChainDesc& desc, const char* debugName)
	{
		::WindowsDesc windowsDesc = {};
#if PLATFORM_WINDOWS
		windowsDesc.handle = (WindowHandle)desc.outputWindow_;
#elif PLATFORM_LINUX
		windowsDesc.xlib_window = desc.outputWindow_;
		windowsDesc.display = setupParams_.display_;
#elif PLATFORM_OSX
		windowsDesc.handle = (WindowHandle)desc.outputWindow_;
#elif PLATFORM_ANDROID
		windowsDesc.handle = (WindowHandle)desc.outputWindow_;
#else
#error "Platform not supported."
#endif
		auto swapChainResource = swapchainResources_.Write(handle);

		::SwapChainDesc swapChainDesc = {};
		swapChainDesc.pWindow = &windowsDesc;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue[0];
		swapChainDesc.mWidth = desc.width_;
		swapChainDesc.mHeight = desc.height_;
		swapChainDesc.mImageCount = desc.bufferCount_;
		swapChainDesc.mSampleCount = SAMPLE_COUNT_1;
		swapChainDesc.mColorFormat = GetImageFormat(desc.format_);
		swapChainDesc.mEnableVsync = false;

		TFSwapChain& swapChainRes = *swapChainResource;
		::SwapChain* swapChain;
		::addSwapChain(renderer_, &swapChainDesc, &swapChain);
		if(!swapChain)
			return ErrorCode::FAIL;

		swapChainRes.swapChain_ = swapChain;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateBuffer(
	    Handle handle, const BufferDesc& desc, const void* initialData, const char* debugName)
	{
		auto buffer = bufferResources_.Write(handle);
		buffer->desc_ = desc;
		::BufferLoadDesc loadDesc = {};
		loadDesc.ppBuffer = &buffer->buffer_;
		loadDesc.pData = initialData;
		loadDesc.mDesc = GetBufferDesc(desc, debugName);

		::addResource(&loadDesc, false);

		if(!buffer->buffer_)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateTexture(
	    Handle handle, const TextureDesc& desc, const TextureSubResourceData* initialData, const char* debugName)
	{
		auto texture = textureResources_.Write(handle);
		texture->desc_ = desc;

		if(Core::ContainsAllFlags(desc.bindFlags_, RENDER_TARGET))
		{
			::RenderTargetDesc tfRenderTargetDesc = GetRenderTargetDesc(desc, debugName);
			::addRenderTarget(renderer_, &tfRenderTargetDesc, &texture->renderTarget_);

			if(!texture->renderTarget_)
				return ErrorCode::FAIL;

			texture->texture_ = &(*Texture)texture->renderTarget_->pTexture;
		}
		else
		{
			::TextureDesc tfTextureDesc = GetTextureDesc(desc, debugName);
			::TextureLoadDesc loadDesc = {};
			loadDesc.ppTexture = &texture->texture_;
			loadDesc.pDesc = &tfTextureDesc;
			::addResource(&loadDesc, false);

			if(!texture->texture_)
				return ErrorCode::FAIL;
		}

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateShader(Handle handle, const ShaderDesc& desc, const char* debugName)
	{
		auto shader = shaders_.Write(handle);
		shader->byteCode_ = new u8[desc.dataSize_];
		shader->byteCodeSize_ = desc.dataSize_;
		memcpy(shader->byteCode_, desc.data_, desc.dataSize_);

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateGraphicsPipelineState(
	    Handle handle, const GraphicsPipelineStateDesc& desc, const char* debugName)
	{
		auto gps = graphicsPipelineStates_.Write(handle);
		::GraphicsPipelineDesc gpDesc = {};

		auto GetBinaryShaderDesc = [&](GraphicsPipelineStateDesc desc) {
			auto GetShaderStages = [&]() {
				::ShaderStage shaderStage = SHADER_STAGE_NONE;
				if(desc.shaders_[(i32)ShaderType::VS])
					shaderStage |= SHADER_STAGE_VERT;
				if(desc.shaders_[(i32)ShaderType::PS])
					shaderStage |= SHADER_STAGE_FRAG;
				if(desc.shaders_[(i32)ShaderType::GS])
					shaderStage |= SHADER_STAGE_GEOM;
				if(desc.shaders_[(i32)ShaderType::HS])
					shaderStage |= SHADER_STAGE_HULL;
				if(desc.shaders_[(i32)ShaderType::DS])
					shaderStage |= SHADER_STAGE_DOMN;
				return shaderStage;
			};

			auto GetShaderBytecode = [&](ShaderType shaderType) {
				::BinaryShaderStageDesc bssDesc = {};
				auto shaderHandle = desc.shaders_[(i32)shaderType];
				if(shaderHandle)
				{
					auto shader = shaders_.Read(shaderHandle);
					bssDesc.pByteCode = (char*)shader->byteCode_;
					bssDesc.mByteCodeSize = shader->byteCodeSize_;
				}
				return bssDesc;
			};

			::BinaryShaderDesc bsDesc = {};
			bsDesc.mStages = GetShaderStages();
			bsDesc.mVert = GetShaderBytecode(ShaderType::VS);
			bsDesc.mFrag = GetShaderBytecode(ShaderType::PS);
			bsDesc.mGeom = GetShaderBytecode(ShaderType::GS);
			bsDesc.mHull = GetShaderBytecode(ShaderType::HS);
			bsDesc.mDomain = GetShaderBytecode(ShaderType::DS);

			return bsDesc;
		};

		auto GetBlendState = [](RenderState renderState) {
			auto GetBlend = [](BlendType type) {
				switch(type)
				{
				case BlendType::ZERO:
					return BC_ZERO;
				case BlendType::ONE:
					return BC_ONE;
				case BlendType::SRC_COLOR:
					return BC_SRC_COLOR;
				case BlendType::INV_SRC_COLOR:
					return BC_ONE_MINUS_SRC_COLOR;
				case BlendType::SRC_ALPHA:
					return BC_SRC_ALPHA;
				case BlendType::INV_SRC_ALPHA:
					return BC_ONE_MINUS_SRC_ALPHA;
				case BlendType::DEST_COLOR:
					return BC_DST_COLOR;
				case BlendType::INV_DEST_COLOR:
					return BC_ONE_MINUS_DST_COLOR;
				case BlendType::DEST_ALPHA:
					return BC_DST_ALPHA;
				case BlendType::INV_DEST_ALPHA:
					return BC_ONE_MINUS_DST_ALPHA;
				default:
					DBG_ASSERT(false);
					return BC_ZERO;
				}
			};

			auto GetBlendOp = [](BlendFunc func) {
				switch(func)
				{
				case BlendFunc::ADD:
					return BM_ADD;
				case BlendFunc::SUBTRACT:
					return BM_SUBTRACT;
				case BlendFunc::REV_SUBTRACT:
					return BM_REVERSE_SUBTRACT;
				case BlendFunc::MINIMUM:
					return BM_MIN;
				case BlendFunc::MAXIMUM:
					return BM_MAX;
				default:
					DBG_ASSERT(false);
					return BM_ADD;
				}
			};

			auto GetRenderTargetMask = [](RenderState renderState) {
				::BlendStateTargets bsTargets;
				for(i32 i = 0; i < MAX_BOUND_RTVS; ++i)
				{
					if(renderState.blendStates_[i].enable_)
						bsTargets |= (BlendStateTargets)i;
				}
				return bsTargets;
			};

			auto blendState = renderState.blendStates_[0];

			::BlendStateDesc bsDesc;
			bsDesc.mSrcFactor = GetBlend(blendState.srcBlend_);
			bsDesc.mDstFactor = GetBlend(blendState.destBlend_);
			bsDesc.mSrcAlphaFactor = GetBlend(blendState.srcBlendAlpha_);
			bsDesc.mDstAlphaFactor = GetBlend(blendState.destBlendAlpha_);
			bsDesc.mBlendMode = GetBlendOp(blendState.blendOp_);
			bsDesc.mBlendAlphaMode = GetBlendOp(blendState.blendOpAlpha_);
			bsDesc.mMask = blendState.writeMask_;
			bsDesc.mRenderTargetMask = GetRenderTargetMask(renderState);
			bsDesc.mAlphaToCoverage = blendState.alphaToCoverage_;

			return bsDesc;
		};

		auto GetRasterizerState = [](const RenderState& renderState) {
			auto GetFillMode = [](FillMode fillMode) {
				switch(fillMode)
				{
				case FillMode::SOLID:
					return FILL_MODE_SOLID;
				case FillMode::WIREFRAME:
					return FILL_MODE_WIREFRAME;
				default:
					DBG_ASSERT(false);
					return FILL_MODE_SOLID;
				}
			};

			auto GetCullMode = [](CullMode cullMode) {
				switch(cullMode)
				{
				case CullMode::NONE:
					return CULL_MODE_NONE;
				case CullMode::CCW:
					return CULL_MODE_FRONT;
				case CullMode::CW:
					return CULL_MODE_BACK;
				default:
					DBG_ASSERT(false);
					return CULL_MODE_NONE;
				}
			};

			::RasterizerStateDesc rsDesc;
			rsDesc.mCullMode = GetCullMode(renderState.cullMode_);
			rsDesc.mDepthBias = (u32)(renderState.depthBias_ * 0xffffff); // TODO: Use depth format.
			rsDesc.mSlopeScaledDepthBias = renderState.slopeScaledDepthBias_;
			rsDesc.mFillMode = GetFillMode(renderState.fillMode_);
			rsDesc.mMultiSample = false; // TODO:
			rsDesc.mScissor = false;

			return rsDesc;
		};

		auto GetDepthStencilState = [](const RenderState& renderState) {
			auto GetCompareMode = [](CompareMode mode) {
				switch(mode)
				{
				case CompareMode::NEVER:
					return CMP_NEVER;
				case CompareMode::LESS:
					return CMP_LESS;
				case CompareMode::EQUAL:
					return CMP_EQUAL;
				case CompareMode::LESS_EQUAL:
					return CMP_LEQUAL;
				case CompareMode::GREATER:
					return CMP_GREATER;
				case CompareMode::NOT_EQUAL:
					return CMP_NOTEQUAL;
				case CompareMode::GREATER_EQUAL:
					return CMP_GEQUAL;
				case CompareMode::ALWAYS:
					return CMP_ALWAYS;
				default:
					DBG_ASSERT(false);
					return CMP_NEVER;
				}
			};

			auto GetStencilOp = [](StencilFunc func) {
				switch(func)
				{
				case StencilFunc::KEEP:
					return STENCIL_OP_KEEP;
				case StencilFunc::ZERO:
					return STENCIL_OP_SET_ZERO;
				case StencilFunc::REPLACE:
					return STENCIL_OP_REPLACE;
				case StencilFunc::INCR:
					return STENCIL_OP_INCR_SAT;
				case StencilFunc::INCR_WRAP:
					return STENCIL_OP_INCR;
				case StencilFunc::DECR:
					return STENCIL_OP_DECR_SAT;
				case StencilFunc::DECR_WRAP:
					return STENCIL_OP_DECR;
				case StencilFunc::INVERT:
					return STENCIL_OP_INVERT;
				default:
					DBG_ASSERT(false);
					return STENCIL_OP_KEEP;
				}
			};

			DepthStateDesc desc;

			desc.mDepthTest = renderState.depthEnable_;
			desc.mDepthWrite = renderState.depthWriteMask_;
			desc.mDepthFunc = GetCompareMode(renderState.depthFunc_);
			desc.mStencilTest = renderState.stencilEnable_;
			desc.mStencilReadMask = renderState.stencilRead_;
			desc.mStencilWriteMask = renderState.stencilWrite_;
			desc.mStencilFrontFunc = GetCompareMode(renderState.stencilFront_.func_);
			desc.mStencilFrontFail = GetStencilOp(renderState.stencilFront_.fail_);
			desc.mDepthFrontFail = GetStencilOp(renderState.stencilFront_.depthFail_);
			desc.mStencilFrontPass = GetStencilOp(renderState.stencilFront_.pass_);
			desc.mStencilBackFunc = GetCompareMode(renderState.stencilBack_.func_);
			desc.mStencilBackFail = GetStencilOp(renderState.stencilBack_.fail_);
			desc.mDepthBackFail = GetStencilOp(renderState.stencilBack_.depthFail_);
			desc.mStencilBackPass = GetStencilOp(renderState.stencilBack_.pass_);

			return desc;
		};

		auto GetTopologyType = [](TopologyType type) {
			switch(type)
			{
			case TopologyType::POINT:
				return PRIMITIVE_TOPO_POINT_LIST;
			case TopologyType::LINE:
				return PRIMITIVE_TOPO_LINE_LIST;
			case TopologyType::TRIANGLE:
				return PRIMITIVE_TOPO_TRI_LIST;
			case TopologyType::PATCH:
				return PRIMITIVE_TOPO_PATCH_LIST;
			default:
				DBG_ASSERT(false);
				return PRIMITIVE_TOPO_POINT_LIST;
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
				DBG_ASSERT(false);
				return "";
			}
		};

		auto GetShaderSemantic = [](VertexUsage usage, i32 fallbackIdx) {
			switch(usage)
			{
			case VertexUsage::POSITION:
				return SEMANTIC_POSITION;
			case VertexUsage::BLENDWEIGHTS:
				return SEMANTIC_TEXCOORD1;
			case VertexUsage::BLENDINDICES:
				return SEMANTIC_TEXCOORD2;
			case VertexUsage::NORMAL:
				return SEMANTIC_NORMAL;
			case VertexUsage::TEXCOORD:
				return SEMANTIC_TEXCOORD0;
			case VertexUsage::TANGENT:
				return SEMANTIC_TANGENT;
			case VertexUsage::BINORMAL:
				return SEMANTIC_BITANGENT;
			case VertexUsage::COLOR:
				return SEMANTIC_COLOR;
			default:
				//DBG_ASSERT(false);
				//return SEMANTIC_UNDEFINED;
				return (ShaderSemantic)fallbackIdx;
			}
		};

		// Add shader
		::BinaryShaderDesc bsDesc = GetBinaryShaderDesc(desc);
		::addShaderBinary(renderer_, &bsDesc, &gps->shader_);

		// Add root signature
		::RootSignatureDesc rsDesc = {};
		rsDesc.ppShaders = gps->shader_;
		rsDesc.mShaderCount = 1;
		rsDesc.ppStaticSamplers = &tfStaticSamplers_[0];
		rsDesc.mStaticSamplerCount = tfStaticSamplers_.size();

		//uint32_t			mMaxBindlessDescriptors[DESCRIPTOR_TYPE_COUNT];
		//const char**		ppStaticSamplerNames;
		::addRootSignature(renderer_, &rsDesc, &gps->rootSignature_);

		// Add vertex layout
		::VertexLayout vertexLayout = {};
		vertexLayout.mAttribCount = desc.numVertexElements_;
		for(i32 i = 0; i < desc.numVertexElements_; ++i)
		{
			const char* semanticName = GetSemanticName(desc.vertexElements_[i].usage_);
			vertexLayout.mAttribs[i].mSemantic = GetShaderSemantic(desc.vertexElements_[i].usage_, desc.vertexElements_[i].usageIdx_);
			//vertexLayout.mAttribs[i].mSemanticNameLength = sizeof(semanticName - 1);
			//strncpy(vertexLayout.mAttribs[i].mSemanticName, GetSemanticName(desc.vertexElements_[i].usage_), sizeof(semanticName) - 1);
			//vertexLayout.mAttribs[i].mSemanticName = strncpy(GetSemanticName(desc.vertexElements_[i].usage_);
			vertexLayout.mAttribs[i].mFormat = GetImageFormat(desc.vertexElements_[i].format_);
			vertexLayout.mAttribs[i].mBinding = desc.vertexElements_[i].streamIdx_;
			vertexLayout.mAttribs[i].mLocation = i;
			vertexLayout.mAttribs[i].mOffset = desc.vertexElements_[i].offset_;
		}

		// Add blend state
		::BlendStateDesc blendStateDesc = GetBlendState(desc.renderState_);
		::addBlendState(renderer_, &blendStateDesc, &gps->blendState_);

		// Add depth state
		::DepthStateDesc depthStateDesc = GetDepthStencilState(desc.renderState_);
		::addDepthState(renderer_, &depthStateDesc, &gps->depthState_);

		// Add rasterizer state
		::RasterizerStateDesc rasterStateDesc = GetRasterizerState(desc.renderState_);
		::addRasterizerState(renderer_, &rasterStateDesc, &gps->rasterizerState_);

		gpDesc.pShaderProgram = gps->shader_;
		gpDesc.pRootSignature = gps->rootSignature_;
		gpDesc.pVertexLayout = &vertexLayout;
		gpDesc.pBlendState = gps->blendState_;
		gpDesc.pDepthState = gps->depthState_;
		gpDesc.pRasterizerState = gps->rasterizerState_;
		::ImageFormat::Enum colorFormats[MAX_BOUND_RTVS] = {};
		bool srgbValues[MAX_BOUND_RTVS] = {};
		for(i32 i = 0; i < MAX_BOUND_RTVS; ++i)
		{
			colorFormats[i] = GetImageFormat(desc.rtvFormats_[i]);
			srgbValues[i] = GetSrgbFormat(desc.rtvFormats_[i]);
		}
		gpDesc.pColorFormats = colorFormats;
		gpDesc.pSrgbValues = srgbValues;
		gpDesc.mRenderTargetCount = desc.numRTs_;
		gpDesc.mSampleCount = SAMPLE_COUNT_1;
		gpDesc.mSampleQuality = 0;
		gpDesc.mDepthStencilFormat = GetImageFormat(desc.dsvFormat_);
		gpDesc.mPrimitiveTopo = GetTopologyType(desc.topology_);

		gps->stencilRef_ = desc.renderState_.stencilRef_;

		::addPipeline(renderer_, &gpDesc, &gps->pipeline_);

		if(!gps->pipeline_)
			return ErrorCode::FAIL;

		//gps->name_ = debugName;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateComputePipelineState(
	    Handle handle, const ComputePipelineStateDesc& desc, const char* debugName)
	{
		auto cps = computePipelineStates_.Write(handle);

		auto shader = shaders_.Read(desc.shader_);

		// Add shader
		::BinaryShaderStageDesc bssDesc = {};
		bssDesc.mByteCodeSize = shader->byteCodeSize_;
		bssDesc.pByteCode = (char*)shader->byteCode_;
		::addShaderBinary(renderer_, &bssDesc, &cps->shader_);
		if(!cps->shader_)
			return ErrorCode::FAIL;

		// Add root signature
		::RootSignatureDesc rsDesc = {};
		rsDesc.ppShaders = cps->shader_;
		rsDesc.mShaderCount = 1;
		::addRootSignature(renderer_, &rsDesc, &cps->rootSignature_);
		if(!cps->rootSignature_)
			return ErrorCode::FAIL;

		// Add pipeline
		::ComputePipelineDesc cpDesc = {};
		cpDesc.pShaderProgram = cps->shader_;
		cpDesc.pRootSignature = cps->rootSignature_;
		::addComputePipeline(renderer_, &cpDesc, &cps->pipeline_);
		if(!cps->pipeline_)
			return ErrorCode::FAIL;

		//cps->name_ = debugName;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreatePipelineBindingSet(
	    Handle handle, const PipelineBindingSetDesc& desc, const char* debugName)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		outPipelineBindingSet.cbvTransitions_.resize(desc.numCBVs_);
		outPipelineBindingSet.srvTransitions_.resize(desc.numSRVs_);
		outPipelineBindingSet.uavTransitions_.resize(desc.numUAVs_);

		outPipelineBindingSet.shaderVisible_ = desc.shaderVisible_;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateDrawBindingSet(Handle handle, const DrawBindingSetDesc& desc, const char* debugName)
	{
		auto dbs = drawBindingSets_.Write(handle);

		memset(&dbs->ib_, 0, sizeof(dbs->ib_));
		memset(dbs->vbs_.data(), 0, sizeof(dbs->vbs_));

		dbs->desc_ = desc;

		{
			if(desc.ib_.resource_)
			{
				auto buffer = GetTFBuffer(desc.ib_.resource_);
				DBG_ASSERT(buffer);

				DBG_ASSERT(Core::ContainsAllFlags(buffer->supportedStates_, RESOURCE_STATE_INDEX_BUFFER));
				dbs->ibResource_ = &(*buffer);
			}

			i32 idx = 0;
			for(const auto& vb : desc.vbs_)
			{
				if(vb.resource_)
				{
					auto buffer = GetTFBuffer(vb.resource_);
					DBG_ASSERT(buffer);
					DBG_ASSERT(Core::ContainsAllFlags(
					    buffer->supportedStates_, RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
					dbs->vbResources_[idx] = &(*buffer);
				}
				++idx;
			}
		}

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateFrameBindingSet(Handle handle, const FrameBindingSetDesc& desc, const char* debugName)
	{
		auto fbs = frameBindingSets_.Write(handle);

		fbs->desc_ = desc;
		{
			Core::Vector<::RenderTargetDesc> rtvDescs;
			::RenderTargetDesc dsvDesc;

			// Check if we're using a swapchain.
			{
				const auto& rtv = desc.rtvs_[0];
				Handle resource = rtv.resource_;
				if(resource.GetType() == ResourceType::SWAP_CHAIN)
				{
					auto swapChain = swapchainResources_.Read(resource);
					fbs->numBuffers_ = swapChain->swapChain_->mDesc.mImageCount;
					fbs->swapChain_ = &(*swapChain);
				}
			}

			// Resize to support number of buffers.
			rtvDescs.resize(MAX_BOUND_RTVS * fbs->numBuffers_);
			memset(rtvDescs.data(), 0, sizeof(::RenderTargetDesc) * rtvDescs.size());
			memset(&dsvDesc, 0, sizeof(::RenderTargetDesc));
			fbs->renderTargets_.resize(MAX_BOUND_RTVS * fbs->numBuffers_);

			for(i32 bufferIdx = 0; bufferIdx < fbs->numBuffers_; ++bufferIdx)
			{
				for(i32 rtvIdx = 0; rtvIdx < MAX_BOUND_RTVS; ++rtvIdx)
				{
					auto& rtvDesc = rtvDescs[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
							//Core::Vector<::RenderTarget*> renderTargets_;
		//::RenderTarget* dsRenderTarget_;
					//D3D12SubresourceRange& rtvResource = fbs->renderTargets_[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
					//RenderTarget* renderTarget = fbs->renderTargets_[rtvIdx + bufferIdx * MAX_BOUND_RTVS];
					const auto& rtv = desc.rtvs_[rtvIdx];
					Handle resource = rtv.resource_;
					if(resource)
					{
						// Only first element can be a swap chain, and only one RTV can be set if using a swap chain.
						DBG_ASSERT(rtvIdx == 0 || resource.GetType() == ResourceType::TEXTURE);
						DBG_ASSERT(rtvIdx == 0 || !fbs->swapChain_);

						// No holes allowed.
						if(bufferIdx == 0)
							if(rtvIdx != fbs->numRTs_++)
								return ErrorCode::FAIL;

						// If it's a Swapchain, we can get the created texture directly.
						if(fbs->swapChain_)
						{
							rtvDesc = swapChain_->swapChain_->ppSwapchainRenderTargets[swapChain_->bbIdx_].mDesc;
							//renderTarget = swapChain_->swapChain_->ppSwapchainRenderTargets[swapChain_->bbIdx_];
						}

						switch(rtv.dimension_)
						{
						case ViewDimension::BUFFER:
							return ErrorCode::UNSUPPORTED;
							break;
						case ViewDimension::TEX1D:
						case ViewDimension::TEX1D_ARRAY:
						case ViewDimension::TEX2D:
						case ViewDimension::TEX2D_ARRAY:
						case ViewDimension::TEX3D:
							break;
						default:
							DBG_ASSERT(false);
							return ErrorCode::FAIL;
							break;
						}
					}
				}
			}

			{
				const auto& dsv = desc.dsv_;
				Handle resource = dsv.resource_;
				if(resource)
				{
					TFSubresourceRange& dsvResource = fbs->dsvResource_;
					DBG_ASSERT(resource.GetType() == ResourceType::TEXTURE);
					auto texture = GetTFTexture(resource);
					DBG_ASSERT(Core::ContainsAnyFlags(
					    texture->supportedStates_, RESOURCE_STATE_DEPTH_WRITE | RESOURCE_STATE_DEPTH_READ));
					dsvResource.resource_ = &(*texture);
					dsvResource.firstSubRsc_ = 0;
					dsvResource.numSubRsc_ = texture->numSubResources_;

					switch(dsv.dimension_)
					{
					case ViewDimension::BUFFER:
						return ErrorCode::UNSUPPORTED;
						break;
					case ViewDimension::TEX1D:
					case ViewDimension::TEX1D_ARRAY:
					case ViewDimension::TEX2D:
					case ViewDimension::TEX2D_ARRAY:
						break;
					default:
						DBG_ASSERT(false);
						return ErrorCode::FAIL;
						break;
					}
				}
			}
		}

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateCommandList(Handle handle, const char* debugName)
	{
		auto commandList = commandLists_.Write(handle);

		*commandList = new TFCommandList(*device_, 0x0, D3D12_COMMAND_LIST_TYPE_DIRECT, debugName);

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateFence(Handle handle, const char* debugName)
	{
		auto fence = fences_.Write(handle);
		::addFence(renderer_, &fence->fence_);
		//fence->name_ = debugName;

		if(!fence->fence_)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CreateSemaphore(Handle handle, const char* debugName)
	{
		auto semaphore = semaphores_.Write(handle);
		::addSemaphore(renderer_, &semaphore->semaphore_);
		//fence->name_ = debugName;

		if(!semaphore->semaphore_)
			return ErrorCode::FAIL;

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::DestroyResource(Handle handle)
	{
		switch(handle.GetType())
		{
		case ResourceType::SWAP_CHAIN:
			if(auto swapChain = swapchainResources_.Write(handle))
			{
				removeSwapChain(renderer_, swapChain->swapChain_);
				*swapChain = TFSwapChain();
			}
			break;
		case ResourceType::BUFFER:
			if(auto buffer = bufferResources_.Write(handle))
			{
				removeResource(renderer_, buffer->buffer_);
				*buffer = TFBuffer();
			}
			break;
		case ResourceType::TEXTURE:
			if(auto texture = bufferResources_.Write(handle))
			{
				removeResource(renderer_, texture->texture_);
				removeResource(renderer_, texture->renderTarget_);
				*texture = TFTexture();
			}
			break;
		case ResourceType::SHADER:
			if(auto shader = shaders_.Write(handle))
			{
				delete[] shader->byteCode_;
				*shader = TFShader();
			}
			break;
		case ResourceType::GRAPHICS_PIPELINE_STATE:
			if(auto gps = graphicsPipelineStates_.Write(handle))
			{
				removePipeline(renderer_, gps->pipeline_);
				removeRootSignature(renderer_, gps->rootSignature_);
				removeShader(renderer_, gps->shader_);
				removeBlendState(renderer_, gps->blendState_);
				removeDepthState(renderer_, gps->depthState_);
				removeRasterizerState(renderer_, gps->rasterizerState_);
				*gps = TFGraphicsPipelineState();
			}
			break;
		case ResourceType::COMPUTE_PIPELINE_STATE:
			if(auto cps = computePipelineStates_.Write(handle))
			{
				removePipeline(renderer_, cps->pipeline_);
				removeRootSignature(renderer_, cps->rootSignature_);
				removeShader(renderer_, cps->shader_);
				*cps = TFComputePipelineState();
			}
			break;
		case ResourceType::PIPELINE_BINDING_SET:
			*pipelineBindingSets_.Write(handle) = TFPipelineBindingSet();
			break;
		case ResourceType::DRAW_BINDING_SET:
			*drawBindingSets_.Write(handle) = TFDrawBindingSet();
			break;
		case ResourceType::FRAME_BINDING_SET:
			*frameBindingSets_.Write(handle) = TFFrameBindingSet();
			break;
		case ResourceType::COMMAND_LIST:
			if(auto commandList = commandLists_.Write(handle))
			{
				delete *commandList;
				*commandList = nullptr;
			}
			break;
		case ResourceType::FENCE:
			if(auto fence = fences_.Write(handle))
			{
				removeFence(renderer_, fence->fence_);
				*fence = TFFence();
			}
			break;
		case ResourceType::SEMAPHORE:
			if(auto semaphore = semaphores_.Write(handle))
			{
				removeSemaphore(renderer_, semaphore->semaphore_);
				*semaphore = TFSemaphore();
			}
			break;
		default:
			return ErrorCode::UNIMPLEMENTED;
		}
		//
		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::AllocTemporaryPipelineBindingSet(Handle handle, const PipelineBindingSetDesc& desc)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		// Setup descriptors.
		auto& samplerAllocator = device_->GetSamplerDescriptorAllocator();
		auto& cbvSubAllocator = device_->GetCBVSubAllocator();
		auto& srvSubAllocator = device_->GetSRVSubAllocator();
		auto& uavSubAllocator = device_->GetUAVSubAllocator();

		pbs->samplers_ = samplerAllocator.Alloc(desc.numSamplers_, GPU::DescriptorHeapSubType::SAMPLER);
		pbs->cbvs_ = cbvSubAllocator.Alloc(desc.numCBVs_, MAX_CBV_BINDINGS);
		pbs->srvs_ = srvSubAllocator.Alloc(desc.numSRVs_, MAX_SRV_BINDINGS);
		pbs->uavs_ = uavSubAllocator.Alloc(desc.numUAVs_, MAX_UAV_BINDINGS);

		pbs->temporary_ = true;
		pbs->shaderVisible_ = true;

		DBG_ASSERT(pbs->samplers_.size_ >= desc.numSamplers_);
		DBG_ASSERT(pbs->cbvs_.size_ >= desc.numCBVs_);
		DBG_ASSERT(pbs->srvs_.size_ >= desc.numSRVs_);
		DBG_ASSERT(pbs->uavs_.size_ >= desc.numUAVs_);

		pbs->cbvTransitions_.resize(desc.numCBVs_);
		pbs->srvTransitions_.resize(desc.numSRVs_);
		pbs->uavTransitions_.resize(desc.numUAVs_);

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingCBV> descs)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		Core::Array<D3D12_CONSTANT_BUFFER_VIEW_DESC, MAX_CBV_BINDINGS> cbvDescs = {};

		i32 bindingIdx = base;
		for(i32 i = 0; i < descs.size(); ++i, ++bindingIdx)
		{
			auto cbvHandle = descs[i].resource_;
			DBG_ASSERT(cbvHandle.IsValid());
			DBG_ASSERT(cbvHandle.GetType() == ResourceType::BUFFER);

			auto resource = GetD3D12Resource(cbvHandle);
			DBG_ASSERT(resource);

			// Setup transition info.
			pbs->cbvTransitions_[bindingIdx].resource_ = &(*resource);
			pbs->cbvTransitions_[bindingIdx].firstSubRsc_ = 0;
			pbs->cbvTransitions_[bindingIdx].numSubRsc_ = 1;

			// Setup the D3D12 descriptor.
			const auto& cbv = descs[i];
			auto& cbvDesc = cbvDescs[bindingIdx];
			auto buf = bufferResources_.Read(cbvHandle);
			cbvDesc.BufferLocation = buf->resource_->GetGPUVirtualAddress() + cbv.offset_;
			cbvDesc.SizeInBytes = Core::PotRoundUp(cbv.size_, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		}

		return device_->UpdateCBVs(
		    *pbs, base, descs.size(), pbs->cbvTransitions_.data() + base, cbvDescs.data() + base);
	}

	GPU::ErrorCode GPU::TFBackend::UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingSRV> descs)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		Core::Array<D3D12_SHADER_RESOURCE_VIEW_DESC, MAX_SRV_BINDINGS> srvDescs = {};

		i32 bindingIdx = base;
		for(i32 i = 0; i < descs.size(); ++i, ++bindingIdx)
		{
			auto srvHandle = descs[i].resource_;
			DBG_ASSERT(srvHandle.IsValid());
			DBG_ASSERT(srvHandle.GetType() == ResourceType::BUFFER || srvHandle.GetType() == ResourceType::TEXTURE);

			ResourceRead<D3D12Buffer> buffer;
			ResourceRead<D3D12Texture> texture;
			if(srvHandle.GetType() == ResourceType::BUFFER)
				buffer = bufferResources_.Read(srvHandle);
			else
				texture = textureResources_.Read(srvHandle);

			i32 firstSubRsc = 0;
			i32 numSubRsc = 0;

			const auto& srv = descs[i];

			i32 mipLevels = srv.mipLevels_NumElements_;
			if(texture)
			{
				if(mipLevels == -1)
					mipLevels = texture->desc_.levels_;
				DBG_ASSERT(mipLevels > 0);
			}

			auto& srvDesc = srvDescs[bindingIdx];

			srvDesc.Format = GetFormat(srv.format_);
			srvDesc.ViewDimension = GetSRVDimension(srv.dimension_);
			srvDesc.Shader4ComponentMapping =
			    D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
			        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
			        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
			        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
			switch(srv.dimension_)
			{
			case ViewDimension::BUFFER:
				srvDesc.Buffer.FirstElement = srv.mostDetailedMip_FirstElement_;
				srvDesc.Buffer.NumElements = mipLevels;
				srvDesc.Buffer.StructureByteStride = srv.structureByteStride_;
				if(srv.structureByteStride_ == 0)
					srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
				else
					srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
				firstSubRsc = 0;
				numSubRsc = 1;
				break;
			case ViewDimension::TEX1D:
				srvDesc.Texture1D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.Texture1D.MipLevels = mipLevels;
				srvDesc.Texture1D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.mostDetailedMip_FirstElement_;
				numSubRsc = mipLevels;
				break;
			case ViewDimension::TEX1D_ARRAY:
				srvDesc.Texture1DArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.Texture1DArray.MipLevels = mipLevels;
				srvDesc.Texture1DArray.ArraySize = srv.arraySize_;
				srvDesc.Texture1DArray.FirstArraySlice = srv.firstArraySlice_;
				srvDesc.Texture1DArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.mostDetailedMip_FirstElement_ + (srv.firstArraySlice_ * texture->desc_.levels_);
				numSubRsc = mipLevels;
				break;
			case ViewDimension::TEX2D:
				srvDesc.Texture2D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.Texture2D.MipLevels = mipLevels;
				srvDesc.Texture2D.PlaneSlice = srv.planeSlice_;
				srvDesc.Texture2D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.mostDetailedMip_FirstElement_;
				numSubRsc = mipLevels;
				break;
			case ViewDimension::TEX2D_ARRAY:
				srvDesc.Texture2DArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.Texture2DArray.MipLevels = mipLevels;
				srvDesc.Texture2DArray.ArraySize = srv.arraySize_;
				srvDesc.Texture2DArray.FirstArraySlice = srv.firstArraySlice_;
				srvDesc.Texture2DArray.PlaneSlice = srv.planeSlice_;
				srvDesc.Texture2DArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.mostDetailedMip_FirstElement_ + (srv.firstArraySlice_ * texture->desc_.levels_);
				numSubRsc = mipLevels;
				break;
			case ViewDimension::TEX3D:
				srvDesc.Texture3D.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.Texture3D.MipLevels = mipLevels;
				srvDesc.Texture3D.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.mostDetailedMip_FirstElement_;
				numSubRsc = mipLevels;
				break;
			case ViewDimension::TEXCUBE:
				srvDesc.TextureCube.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.TextureCube.MipLevels = mipLevels;
				srvDesc.TextureCube.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = 0;
				numSubRsc = texture->desc_.levels_ * 6;
				break;
			case ViewDimension::TEXCUBE_ARRAY:
				srvDesc.TextureCubeArray.MostDetailedMip = srv.mostDetailedMip_FirstElement_;
				srvDesc.TextureCubeArray.MipLevels = mipLevels;
				srvDesc.TextureCubeArray.NumCubes = srv.arraySize_;
				srvDesc.TextureCubeArray.First2DArrayFace = srv.firstArraySlice_;
				srvDesc.TextureCubeArray.ResourceMinLODClamp = srv.resourceMinLODClamp_;
				firstSubRsc = srv.firstArraySlice_ * 6;
				numSubRsc = (texture->desc_.levels_ * 6) * srv.arraySize_;
				break;
			default:
				DBG_ASSERT(false);
				return ErrorCode::FAIL;
				break;
			}

			auto resource = GetD3D12Resource(srvHandle);
			DBG_ASSERT(resource);
			pbs->srvTransitions_[bindingIdx].resource_ = &(*resource);
			pbs->srvTransitions_[bindingIdx].firstSubRsc_ = firstSubRsc;
			pbs->srvTransitions_[bindingIdx].numSubRsc_ = numSubRsc;
		}

		return device_->UpdateSRVs(
		    *pbs, base, descs.size(), pbs->srvTransitions_.data() + base, srvDescs.data() + base);
	}

	GPU::ErrorCode GPU::TFBackend::UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const BindingUAV> descs)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		Core::Array<D3D12_UNORDERED_ACCESS_VIEW_DESC, MAX_UAV_BINDINGS> uavDescs = {};

		i32 bindingIdx = base;
		for(i32 i = 0; i < descs.size(); ++i, ++bindingIdx)
		{
			auto uavHandle = descs[i].resource_;
			DBG_ASSERT(uavHandle.IsValid());
			DBG_ASSERT(uavHandle.GetType() == ResourceType::BUFFER || uavHandle.GetType() == ResourceType::TEXTURE);

			ResourceRead<D3D12Buffer> buffer;
			ResourceRead<D3D12Texture> texture;
			if(uavHandle.GetType() == ResourceType::BUFFER)
				buffer = bufferResources_.Read(uavHandle);
			else
				texture = textureResources_.Read(uavHandle);

			const auto& uav = descs[i];

			i32 firstSubRsc = 0;
			i32 numSubRsc = 0;

			auto& uavDesc = uavDescs[bindingIdx];
			uavDesc.Format = GetFormat(uav.format_);
			uavDesc.ViewDimension = GetUAVDimension(uav.dimension_);
			switch(uav.dimension_)
			{
			case ViewDimension::BUFFER:
				uavDesc.Buffer.FirstElement = uav.mipSlice_FirstElement_;
				uavDesc.Buffer.NumElements = uav.firstArraySlice_FirstWSlice_NumElements_;
				uavDesc.Buffer.StructureByteStride = uav.structureByteStride_;
				if(uavDesc.Format == DXGI_FORMAT_R32_TYPELESS && uav.structureByteStride_ == 0)
					uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
				else
					uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				firstSubRsc = 0;
				numSubRsc = 1;
				break;
			case ViewDimension::TEX1D:
				uavDesc.Texture1D.MipSlice = uav.mipSlice_FirstElement_;
				firstSubRsc = uav.mipSlice_FirstElement_;
				numSubRsc = 1;
				break;
			case ViewDimension::TEX1D_ARRAY:
				uavDesc.Texture1DArray.MipSlice = uav.mipSlice_FirstElement_;
				uavDesc.Texture1DArray.ArraySize = uav.arraySize_WSize_;
				uavDesc.Texture1DArray.FirstArraySlice = uav.firstArraySlice_FirstWSlice_NumElements_;
				firstSubRsc = uav.mipSlice_FirstElement_ +
				              (uav.firstArraySlice_FirstWSlice_NumElements_ * texture->desc_.levels_);
				numSubRsc = uav.arraySize_WSize_;
				break;
			case ViewDimension::TEX2D:
				uavDesc.Texture2D.MipSlice = uav.mipSlice_FirstElement_;
				uavDesc.Texture2D.PlaneSlice = uav.planeSlice_;
				firstSubRsc = uav.mipSlice_FirstElement_;
				numSubRsc = 1;
				break;
			case ViewDimension::TEX2D_ARRAY:
				uavDesc.Texture2DArray.MipSlice = uav.mipSlice_FirstElement_;
				uavDesc.Texture2DArray.ArraySize = uav.arraySize_WSize_;
				uavDesc.Texture2DArray.FirstArraySlice = uav.firstArraySlice_FirstWSlice_NumElements_;
				uavDesc.Texture2DArray.PlaneSlice = uav.planeSlice_;
				firstSubRsc = uav.mipSlice_FirstElement_ +
				              (uav.firstArraySlice_FirstWSlice_NumElements_ * texture->desc_.levels_);
				numSubRsc = texture->desc_.levels_ * uav.arraySize_WSize_;
				break;
			case ViewDimension::TEX3D:
				uavDesc.Texture3D.MipSlice = uav.mipSlice_FirstElement_;
				uavDesc.Texture3D.FirstWSlice = uav.firstArraySlice_FirstWSlice_NumElements_;
				uavDesc.Texture3D.WSize = uav.arraySize_WSize_;
				firstSubRsc = uav.mipSlice_FirstElement_;
				numSubRsc = 1;
				break;
			default:
				DBG_ASSERT(false);
				return ErrorCode::FAIL;
				break;
			}

			auto resource = GetD3D12Resource(uavHandle);
			DBG_ASSERT(resource);
			pbs->uavTransitions_[bindingIdx].resource_ = &(*resource);
			pbs->uavTransitions_[bindingIdx].firstSubRsc_ = firstSubRsc;
			pbs->uavTransitions_[bindingIdx].numSubRsc_ = numSubRsc;
		}

		return device_->UpdateUAVs(
		    *pbs, base, descs.size(), pbs->uavTransitions_.data() + base, uavDescs.data() + base);
	}

	GPU::ErrorCode GPU::TFBackend::UpdatePipelineBindings(Handle handle, i32 base, Core::ArrayView<const SamplerState> descs)
	{
		auto pbs = pipelineBindingSets_.Write(handle);

		Core::Array<D3D12_SAMPLER_DESC, MAX_SAMPLER_BINDINGS> samplerDescs;

		i32 bindingIdx = base;
		for(i32 i = 0; i < descs.size(); ++i, ++bindingIdx)
		{
			samplerDescs[bindingIdx] = GetSampler(descs[i]);
		}

		return device_->UpdateSamplers(*pbs, base, descs.size(), samplerDescs.data() + base);
	}

	GPU::ErrorCode GPU::TFBackend::CopyPipelineBindings(
	    Core::ArrayView<const PipelineBinding> dst, Core::ArrayView<const PipelineBinding> src)
	{
		auto* d3dDevice = device_->d3dDevice_.Get();
		i32 viewIncr = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		i32 samplerIncr = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		for(i32 i = 0; i < dst.size(); ++i)
		{
			DBG_ASSERT(dst[i].pbs_ != src[i].pbs_);
			auto dstPBS = pipelineBindingSets_.Write(dst[i].pbs_);
			auto srcPBS = pipelineBindingSets_.Read(src[i].pbs_);

			auto CopyRange = [d3dDevice](D3D12DescriptorAllocation& dstAlloc,
			    Core::Vector<D3D12SubresourceRange>& dstTransitions, i32 dstOffset,
			    const D3D12DescriptorAllocation& srcAlloc, const Core::Vector<D3D12SubresourceRange>& srcTransitions,
			    i32 srcOffset, i32 num, D3D12_DESCRIPTOR_HEAP_TYPE type, i32 incr, DescriptorHeapSubType subType) {
				D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = dstAlloc.GetCPUHandle(dstOffset);
				D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = srcAlloc.GetCPUHandle(srcOffset);
				DBG_ASSERT(dstHandle.ptr);
				DBG_ASSERT(srcHandle.ptr);
				DBG_ASSERT(dstOffset < dstAlloc.size_);
				DBG_ASSERT(srcOffset < srcAlloc.size_);
				DBG_ASSERT((dstOffset + num) <= dstAlloc.size_);
				DBG_ASSERT((srcOffset + num) <= srcAlloc.size_);

#if ENABLE_DESCRIPTOR_DEBUG_DATA
				for(i32 i = 0; i < num; ++i)
				{
					auto& dstDebug = dstAlloc.GetDebugData(dstOffset + i);
					const auto& srcDebug = srcAlloc.GetDebugData(srcOffset + i);
					DBG_ASSERT(srcDebug.subType_ == subType);
					if(srcDebug.subType_ != DescriptorHeapSubType::SAMPLER)
					{
						DBG_ASSERT(srcDebug.resource_);
					}
					else
					{
						DBG_ASSERT(!srcDebug.resource_);
					}
					dstDebug = srcDebug;
				}
#endif

				d3dDevice->CopyDescriptorsSimple(num, dstHandle, srcHandle, type);

				if(srcTransitions.size() > 0)
					for(i32 i = 0; i < num; ++i)
						dstTransitions[i + dstOffset] = srcTransitions[i + srcOffset];
			};

			if(dst[i].cbvs_.num_ > 0)
				CopyRange(dstPBS->cbvs_, dstPBS->cbvTransitions_, dst[i].cbvs_.dstOffset_, srcPBS->cbvs_,
				    srcPBS->cbvTransitions_, src[i].cbvs_.srcOffset_, dst[i].cbvs_.num_,
				    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, viewIncr, DescriptorHeapSubType::CBV);
			if(dst[i].srvs_.num_ > 0)
				CopyRange(dstPBS->srvs_, dstPBS->srvTransitions_, dst[i].srvs_.dstOffset_, srcPBS->srvs_,
				    srcPBS->srvTransitions_, src[i].srvs_.srcOffset_, dst[i].srvs_.num_,
				    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, viewIncr, DescriptorHeapSubType::SRV);
			if(dst[i].uavs_.num_ > 0)
				CopyRange(dstPBS->uavs_, dstPBS->uavTransitions_, dst[i].uavs_.dstOffset_, srcPBS->uavs_,
				    srcPBS->uavTransitions_, src[i].uavs_.srcOffset_, dst[i].uavs_.num_,
				    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, viewIncr, DescriptorHeapSubType::UAV);

			static Core::Vector<D3D12SubresourceRange> emptyTransitions;
			if(dst[i].samplers_.num_ > 0)
				CopyRange(dstPBS->samplers_, emptyTransitions, dst[i].samplers_.dstOffset_, srcPBS->samplers_,
				    emptyTransitions, src[i].samplers_.dstOffset_, dst[i].samplers_.num_,
				    D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, samplerIncr, DescriptorHeapSubType::SAMPLER);
		}

		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::ValidatePipelineBindings(Core::ArrayView<const PipelineBinding> pb)
	{
#if ENABLE_DESCRIPTOR_DEBUG_DATA
		auto LogDescriptors = [this](const char* name, const PipelineBinding& pb) {
			const auto& pbs = pipelineBindingSets_[pb.pbs_.GetIndex()];

			Core::Log("Descriptor (%d):\n", pb.pbs_.GetIndex());
			Core::Log("- CBV Base: %d, %d\n", pbs.cbvs_.offset_, pb.cbvs_.dstOffset_);
			Core::Log("- SRV Base: %d, %d\n", pbs.srvs_.offset_, pb.srvs_.dstOffset_);
			Core::Log("- UAV Base: %d, %d\n", pbs.uavs_.offset_, pb.uavs_.dstOffset_);
			Core::Log("- Sampler Base: %d, %d\n", pbs.samplers_.offset_, pb.samplers_.dstOffset_);

			Core::Log("- CBVs: %d\n", pbs.cbvs_.size_);
			for(i32 i = 0; i < pbs.cbvs_.size_; ++i)
			{
				const auto& debugData = pbs.cbvs_.GetDebugData(i);
				Core::Log("- - %d: %d, %s\n", i, debugData.subType_, debugData.name_);
			}

			Core::Log("- SRVs: %d\n", pbs.srvs_.size_);
			for(i32 i = 0; i < pbs.srvs_.size_; ++i)
			{
				const auto& debugData = pbs.srvs_.GetDebugData(i);
				Core::Log("- - %d: %d, %s\n", i, debugData.subType_, debugData.name_);
			}

			Core::Log("- UAVs: %d\n", pbs.uavs_.size_);
			for(i32 i = 0; i < pbs.uavs_.size_; ++i)
			{
				const auto& debugData = pbs.cbvs_.GetDebugData(i);
				Core::Log("- - %d: %d, %s\n", i, debugData.subType_, debugData.name_);
			}

			Core::Log("- Samplers: %d\n", pbs.samplers_.size_);
			for(i32 i = 0; i < pbs.samplers_.size_; ++i)
			{
				const auto& debugData = pbs.samplers_.GetDebugData(i);
				Core::Log("- - %d: %d, %s\n", i, debugData.subType_, debugData.name_);
			}
		};

		for(auto singlePb : pb)
		{
			LogDescriptors("desc", singlePb);
			const auto& pbs = pipelineBindingSets_[singlePb.pbs_.GetIndex()];
			{
				for(i32 i = 0; i < pb[0].samplers_.num_; ++i)
				{
					DBG_ASSERT(pbs.samplers_.GetDebugData(i).subType_ == DescriptorHeapSubType::SAMPLER);
				}

				for(i32 i = 0; i < pb[0].cbvs_.num_; ++i)
				{
					DBG_ASSERT(pbs.cbvs_.GetDebugData(i).subType_ == DescriptorHeapSubType::CBV);
					DBG_ASSERT(pbs.cbvs_.GetDebugData(i).resource_);
				}

				for(i32 i = 0; i < pb[0].srvs_.num_; ++i)
				{
					DBG_ASSERT(pbs.srvs_.GetDebugData(i).subType_ == DescriptorHeapSubType::SRV);
				}

				for(i32 i = 0; i < pb[0].uavs_.num_; ++i)
				{
					DBG_ASSERT(pbs.uavs_.GetDebugData(i).subType_ == DescriptorHeapSubType::UAV);
					DBG_ASSERT(pbs.uavs_.GetDebugData(i).resource_);
				}
			}
		}
#endif // ENABLE_DESCRIPTOR_DEBUG_DATA
		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::CompileCommandList(Handle handle, const CommandList& commandList)
	{
		DBG_ASSERT(handle.GetIndex() < commandLists_.size());

		auto outCommandList = commandLists_.Write(handle);
		D3D12CompileContext context(*this);
		return context.CompileCommandList(**outCommandList, commandList);
	}

	GPU::ErrorCode GPU::TFBackend::SubmitCommandLists(Core::ArrayView<Handle> handles)
	{
		Core::Array<D3D12CommandList*, COMMAND_LIST_BATCH_SIZE> commandLists;
		i32 numBatches = (handles.size() + (COMMAND_LIST_BATCH_SIZE - 1)) / COMMAND_LIST_BATCH_SIZE;
		for(i32 batch = 0; batch < numBatches; ++batch)
		{
			const i32 baseHandle = batch * COMMAND_LIST_BATCH_SIZE;
			const i32 numHandles = Core::Min(COMMAND_LIST_BATCH_SIZE, handles.size() - baseHandle);
			for(i32 i = 0; i < numHandles; ++i)
			{
				commandLists[i] = *commandLists_.Read(handles[i]);
				DBG_ASSERT(commandLists[i]);
			}

			auto retVal =
			    device_->SubmitCommandLists(Core::ArrayView<D3D12CommandList*>(commandLists.data(), numHandles));
			if(retVal != ErrorCode::OK)
				return retVal;
		}
		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::PresentSwapChain(Handle handle)
	{
		auto swapChain = swapchainResources_.Write(handle);

		HRESULT retVal = S_OK;
		CHECK_D3D(retVal = swapChain->swapChain_->Present(0, 0));
		if(FAILED(retVal))
			return ErrorCode::FAIL;
		swapChain->bbIdx_ = swapChain->swapChain_->GetCurrentBackBufferIndex();
		return ErrorCode::OK;
	}

	GPU::ErrorCode GPU::TFBackend::ResizeSwapChain(Handle handle, i32 width, i32 height)
	{
		auto swapChain = swapchainResources_.Write(handle);

		ErrorCode retVal = device_->ResizeSwapChain(*swapChain, width, height);
		if(retVal != ErrorCode::OK)
			return retVal;
		swapChain->bbIdx_ = swapChain->swapChain_->GetCurrentBackBufferIndex();
		return ErrorCode::OK;
	}

	void GPU::TFBackend::NextFrame()
	{
		if(device_)
			device_->NextFrame();
	}

	void GPU::TFBackend::CreateCommandQueues()
	{
		::QueueDesc directQueueDesc = {};
		directQueueDesc.mFlag = QUEUE_FLAG_NONE;
		directQueueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
		directQueueDesc.mType = CMD_POOL_DIRECT;
		directQueueDesc.mNodeIndex = 0;

		::addQueue(renderer_, &directQueueDesc, &tfDirectQueue_);

		::QueueDesc asyncComputeQueueDesc = {};
		asyncComputeQueueDesc.mFlag = QUEUE_FLAG_NONE;
		asyncComputeQueueDesc.mPriority = QUEUE_PRIORITY_NORMAL;
		asyncComputeQueueDesc.mType = CMD_POOL_COMPUTE;
		asyncComputeQueueDesc.mNodeIndex = 0;

		::addQueue(renderer_, &asyncComputeQueueDesc, &tfAsyncComputeQueue_);
	}

	void GPU::TFBackend::CreateStaticSamplers()
	{
		const auto& defaultSamplers = GetDefaultSamplerStates();
		tfStaticSamplers_ = Core::Vector(defaultSamplers.size());
		for(i32 idx = 0; idx != defaultSamplers.size(); ++idx)
			::addSampler(renderer_, &GetSampler(defaultSamplers[idx]), &tfStaticSamplers_[idx]);
	}

	void GPU::TFDevice::CreateCommandSignatures()
	{
		::IndirectArgumentDescriptor drawArg = {};
		drawArg.mType = IndirectArgumentType::INDIRECT_DRAW;
		drawArg.mRootParameterIndex = 0;
		drawArg.mCount = 0;
		drawArg.mDivisor = 0;

		::IndirectArgumentDescriptor drawIndexedArg = {};
		drawIndexedArg.mType = IndirectArgumentType::INDIRECT_DRAW_INDEX;
		drawArg.mRootParameterIndex = 0;
		drawArg.mCount = 0;
		drawArg.mDivisor = 0;

		::IndirectArgumentDescriptor dispatchArg = {};
		dispatchArg.mType = IndirectArgumentType::INDIRECT_DISPATCH;
		drawArg.mRootParameterIndex = 0;
		drawArg.mCount = 0;
		drawArg.mDivisor = 0;

		::CommandSignatureDesc drawDesc = {};
		//drawDesc.pCmdPool =;
		drawDesc.pRootSignature = nullptr; // Unused, but asserts on The-Forge (make a PR)
		drawDesc.mIndirectArgCount = 1;
		drawDesc.pArgDescs = &drawArg;

		::CommandSignatureDesc drawIndexedDesc = {};
		//drawIndexedDesc.pCmdPool =;
		drawIndexedDesc.pRootSignature = nullptr; // Unused, but asserts on The-Forge (make a PR)
		drawIndexedDesc.mIndirectArgCount = 1;
		drawIndexedDesc.pArgDescs = &drawIndexedArg;

		::CommandSignatureDesc dispatchDesc = {};
		//dispatchDesc.pCmdPool =;
		dispatchDesc.pRootSignature = nullptr; // Unused, but asserts on The-Forge (make a PR)
		dispatchDesc.mIndirectArgCount = 1;
		dispatchDesc.pArgDescs = &dispatchArg;

		::addIndirectCommandSignature(renderer_, &drawDesc, &tfDrawCmdSig_);
		::addIndirectCommandSignature(renderer_, &drawIndexedDesc, &tfDrawIndexedCmdSig_);
		::addIndirectCommandSignature(renderer_, &dispatchDesc, &tfDispatchCmdSig_);
	}

	GPU::ResourceRead<TFResource> GPU::TFBackend::GetTFResource(Handle handle)
	{
		if(handle.GetType() == ResourceType::BUFFER)
		{
			auto buffer = bufferResources_.Read(handle);
			return ResourceRead<TFResource>(std::move(buffer), *buffer);
		}
		else if(handle.GetType() == ResourceType::TEXTURE)
		{
			auto texture = textureResources_.Read(handle);
			return ResourceRead<TFResource>(std::move(texture), *texture);
		}
		else if(handle.GetType() == ResourceType::SWAP_CHAIN)
		{
			auto swapChain = swapchainResources_.Read(handle);
			return ResourceRead<TFResource>(std::move(swapChain), *swapChain);
		}
		return ResourceRead<TFResource>();
	}

	GPU::ResourceRead<TFBuffer> GPU::TFBackend::GetTFBuffer(Handle handle)
	{
		if(handle.GetType() != ResourceType::BUFFER)
			return ResourceRead<TFBuffer>();
		if(handle.GetIndex() >= bufferResources_.size())
			return ResourceRead<TFBuffer>();
		return bufferResources_.Read(handle);
	}

	GPU::ResourceRead<TFTexture> GPU::TFBackend::GetTFTexture(Handle handle, i32 bufferIdx)
	{
		if(handle.GetType() == ResourceType::TEXTURE)
		{
			return textureResources_.Read(handle);
		}
		else if(handle.GetType() == ResourceType::SWAP_CHAIN)
		{
			auto swapChain = swapchainResources_.Read(handle);
			return ResourceRead<TFTexture>(std::move(swapChain), *swapChain);
		}
		return ResourceRead<TFTexture>();
	}

//} // namespace GPU
