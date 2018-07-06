#pragma once

#include "core/array.h"
#include "core/types.h"
#include "core/library.h"
#include "core/os.h"
#include "gpu/resources.h"
#include "gpu/types.h"

// The-Forge uses these to set the API
#ifdef ENGINE_THEFORGE_VLK
#define VULKAN
#elif ENGINE_THEFORGE_DX12
#define DIRECT3D12
#elif ENGINE_THEFORGE_MTL
#define METAL
#endif

// The-Forge main header
#include <Renderer/IRenderer.h>
// The-Forge resource loader header
#include <Renderer/ResourceLoader.h>

namespace GPU
{
	/**
	 * Conversion.
	 */
	::BufferUsage GetBufferUsage(GPU::BindFlags bindFlags);
	::ResourceState GetResourceStates(GPU::BindFlags bindFlags);
	::ResourceMemoryUsage GetResourceMemoryUsage(GPU::MemoryUsage usage);
	::BufferCreationFlags GetBufferCreationFlags(GPU::BindFlags bindFlags);
	::TextureType GetTextureType(GPU::TextureType type);
	::TextureUsage GetTextureUsage(GPU::BindFlags bindFlags);
	::TextureCreationFlags GetTextureCreationFlags(GPU::BindFlags bindFlags);
	::ImageFormat::Enum GetImageFormat(GPU::Format format);
	bool GetSrgbFormat(GPU::Format format);
	::PrimitiveTopology GetPrimitiveTopology(GPU::PrimitiveTopology topology);
	::BufferDesc GetBufferDesc(const GPU::BufferDesc& desc, const char* debugName);
	::TextureDesc GetTextureDesc(const GPU::TextureDesc& desc, const char* debugName);
	::RenderTargetDesc GetRenderTargetDesc(const GPU::TextureDesc& desc, const char* debugName);

	/**
	 * Sampler.
	 */
	::AddressMode GetAddressingMode(GPU::AddressingMode addressMode);
	::FilterType GetFilterType(GPU::FilteringMode filterMode, u32 anisotropy);
	::MipMapMode GetMipMapMode(GPU::FilteringMode filterMode);
	::SamplerDesc GetSampler(const GPU::SamplerState& state);

	static const i32 COMMAND_LIST_BATCH_SIZE = 32;
	static const i64 UPLOAD_AUTO_FLUSH_COMMANDS = 30;
	static const i64 UPLOAD_AUTO_FLUSH_BYTES = 8 * 1024 * 1024;

} // namespace GPU
