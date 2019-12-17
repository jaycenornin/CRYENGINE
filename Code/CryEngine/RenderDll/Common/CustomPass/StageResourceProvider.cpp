#include "StdAfx.h"
#include "StageResourceProvider.h"

_smart_ptr<ITexture> Cry::Renderer::CStageResourceProvider::CreateRenderTarget(const SRTCreationParams& renderTargetParams) const
{
	_smart_ptr <CTexture> pRenderTarget = nullptr;
	auto createFunc = CTexture::GetOrCreateRenderTarget;

	if (renderTargetParams.flags & FT_USAGE_DEPTHSTENCIL)
		createFunc = CTexture::GetOrCreateDepthStencil;

	pRenderTarget = createFunc(renderTargetParams.rtName
		, renderTargetParams.width
		, renderTargetParams.height
		, renderTargetParams.clearColor
		, renderTargetParams.textureType
		, renderTargetParams.flags
		, renderTargetParams.format
		, renderTargetParams.customID
	);

	return pRenderTarget->IsTextureLoaded() ? pRenderTarget : nullptr;
}

std::unique_ptr<IDynTexture> Cry::Renderer::CStageResourceProvider::CreateDynamicRenderTarget(const SRTCreationParams& renderTargetParams) const
{
	uint32 flags = renderTargetParams.flags;
	if (!(flags & FT_USAGE_RENDERTARGET) && !(flags & FT_USAGE_DEPTHSTENCIL))
		flags |= FT_USAGE_RENDERTARGET;

	std::unique_ptr<IDynTexture> pDynRt = std::make_unique<SDynTexture>(
		renderTargetParams.width,
		renderTargetParams.height,
		renderTargetParams.clearColor,
		renderTargetParams.format,
		renderTargetParams.textureType,
		flags,
		renderTargetParams.rtName
		);

	pDynRt->Update(renderTargetParams.width, renderTargetParams.height);
	return std::move(pDynRt);
}

int Cry::Renderer::CStageResourceProvider::RegisterConstantName(const char* name)
{
	std::string constantName = name;

	for (uint32 i = 0; i < m_constantNameLookup.size(); ++i)
	{
		if (m_constantNameLookup[i].first == constantName)
		{
			return i;
		}
	}

	m_constantNameLookup.emplace_back();
	auto& last = m_constantNameLookup.back();
	last.first = constantName;
	last.second = CCryNameR(last.first.c_str());
	return m_constantNameLookup.size() - 1;
}

InputLayoutHandle Cry::Renderer::CStageResourceProvider::RegisterLayout(const SInputElementDescription* pDescriptions, size_t count)
{
	if (!count)
		return 0;

	//Do an inbetween copy to be save
	static_assert(sizeof(SInputElementDescription) == sizeof(D3D11_INPUT_ELEMENT_DESC), "Mismatch between input layout descriptions");
	std::vector<D3D11_INPUT_ELEMENT_DESC> descs(count);
	memcpy(&descs[0], pDescriptions, count * sizeof(SInputElementDescription));

	return CDeviceObjectFactory::CreateCustomVertexFormat(descs.size(), descs.data());
}

InputLayoutHandle Cry::Renderer::CStageResourceProvider::RegisterLayout(TArray<SInputElementDescription>& layoutDesc)
{
	if (layoutDesc.empty())
		return 0;

	//SDeviceObjectHelpers::THwShaderInfo shaderInfo;
	//SDeviceObjectHelpers::GetShaderInstanceInfo(shaderInfo, pCShader, techniqueCRCLC, 0, 0, 0, nullptr, false);

	static_assert(sizeof(SInputElementDescription) == sizeof(D3D11_INPUT_ELEMENT_DESC), "Mismatch between input layout descriptions");
	std::vector<D3D11_INPUT_ELEMENT_DESC> descs(layoutDesc.size());
	memcpy(&descs[0], &layoutDesc[0], layoutDesc.size() * sizeof(SInputElementDescription));

	return CDeviceObjectFactory::CreateCustomVertexFormat(descs.size(), descs.data());
}

uintptr_t Cry::Renderer::CStageResourceProvider::CreateConstantBuffer(size_t sizeInBytes)
{
	auto pConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeInBytes);

	//We should have some common constant buffer interface instead to enable lifetime management. This is a hack.
	auto pConstantBufferRaw = pConstantBuffer.ReleaseOwnership();
	return reinterpret_cast<uintptr_t>(pConstantBufferRaw);
}

void Cry::Renderer::CStageResourceProvider::FreeConstantBuffer(uintptr_t buffer)
{
	if (buffer == INVALID_BUFFER)
		return;

	CConstantBuffer* pBuffer = reinterpret_cast<CConstantBuffer*>(buffer);
	pBuffer->Release();
}

uintptr_t Cry::Renderer::CStageResourceProvider::CreateOrUpdateBuffer(const SBufferParams& params, uintptr_t bufferHandle /*= INVALID_BUFFER*/)
{
	if (!params.stride || !params.elementCount)
		return INVALID_BUFFER;

	buffer_handle_t handle = bufferHandle;
	if (handle == INVALID_BUFFER)
	{
		handle = gcpRendD3D->m_DevBufMan.Create((BUFFER_BIND_TYPE)params.type, (BUFFER_USAGE)params.usage, params.elementCount * params.stride);
	}
	else
	{
		auto bufferSize = gcpRendD3D->m_DevBufMan.Size(handle);
		auto newTotalSize = params.elementCount * params.stride;

		//The new buffer size is to big. We need a new one
		if (newTotalSize > bufferSize)
		{
			gcpRendD3D->m_DevBufMan.Destroy(handle);
			handle = gcpRendD3D->m_DevBufMan.Create((BUFFER_BIND_TYPE)params.type, (BUFFER_USAGE)params.usage, newTotalSize);
		}
	}

	if (params.pData)
	{
		gcpRendD3D->m_DevBufMan.UpdateBuffer(handle, params.pData, params.elementCount * params.stride);
	}

	if (handle == INVALID_BUFFER)
		return INVALID_BUFFER;

	return handle;
}

void Cry::Renderer::CStageResourceProvider::FreeBuffer(uintptr_t bufferHandle)
{
	gcpRendD3D->m_DevBufMan.Destroy(bufferHandle);
}

void* Cry::Renderer::CStageResourceProvider::BufferBeginWrite(uintptr_t handle)
{
	if (handle == INVALID_BUFFER)
		return nullptr;

	return gcpRendD3D->m_DevBufMan.BeginWrite(handle);
}

void Cry::Renderer::CStageResourceProvider::BufferEndWrite(uintptr_t handle)
{
	if (handle == INVALID_BUFFER)
		return;

	gcpRendD3D->m_DevBufMan.EndReadWrite(handle);
}