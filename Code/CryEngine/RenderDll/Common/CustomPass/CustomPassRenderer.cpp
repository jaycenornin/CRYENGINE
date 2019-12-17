#include "StdAfx.h"
#include "CustomPassRenderer.h"

using namespace Cry::Renderer::CustomPass;

static std::unique_ptr<CCustomRenderer> g_pCustomRenderer = std::make_unique<CCustomRenderer>();

#pragma region CCustomRenderer

CCustomRenderer* CCustomRenderer::GetCustomPassRenderer()
{
	return g_pCustomRenderer.get();
}

CCustomRenderer::~CCustomRenderer()
{
	
	m_constantNameLookup.resize(0);
}

void CCustomRenderer::Shutdown()
{
	for (auto instance : m_instances)
	{
		instance->GetAssignedImplementation()->RT_Shutdown();
	}

	g_pCustomRenderer.reset();
}

uintptr_t CCustomRenderer::RT_CreateConstantBuffer(size_t sizeInBytes)
{
	auto pConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeInBytes);

	//We should have some common constant buffer interface instead to enable lifetime management. This is a hack.
	auto pConstantBufferRaw = pConstantBuffer.ReleaseOwnership();
	return reinterpret_cast<uintptr_t>(pConstantBufferRaw);
}

void CCustomRenderer::RT_FreeConstantBuffer(uintptr_t buffer)
{
	if (buffer == INVALID_BUFFER)
		return;

	CConstantBuffer* pBuffer = reinterpret_cast<CConstantBuffer*>(buffer);
	pBuffer->Release();
}

void CCustomRenderer::RT_ClearRenderTarget(ITexture* pTarget, ColorF clearColor) const
{
	CClearSurfacePass::Execute(static_cast<CTexture*>(pTarget), clearColor);
}

void CCustomRenderer::CreateRendererInstance(const char* name, ICustomRendererImplementation* pImpl, size_t maxConstantSize, uint32 customID)
{
	uint32 id = customID;
	if (customID == ~0u)
		id = CryStringUtils::CalculateHash(name);

	string nameString(name);
	g_pCustomRenderer->ExecuteRTCommand([=, nameString = std::move(nameString)]()
	{
		std::unique_ptr<ICustomRendererInstance>  pInstance = std::make_unique<CCustomRendererInstance>(nameString, id, pImpl, maxConstantSize);
		m_instances.emplace_back(static_cast<CCustomRendererInstance*>(pInstance.get()));
		pImpl->RT_Initalize(std::move(pInstance));
	});

	return;
}

void CCustomRenderer::RT_FreeBuffer(uintptr_t bufferHandle)
{
	gcpRendD3D->m_DevBufMan.Destroy(bufferHandle);
}

uintptr_t CCustomRenderer::RT_CreateOrUpdateBuffer(const SBufferParams &params, uintptr_t bufferHandle)
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


void* Cry::Renderer::CustomPass::CCustomRenderer::RT_BufferBeginWrite(uintptr_t handle)
{
	if (handle == INVALID_BUFFER)
		return nullptr;

	return gcpRendD3D->m_DevBufMan.BeginWrite(handle);
}

void Cry::Renderer::CustomPass::CCustomRenderer::RT_BufferEndWrite(uintptr_t handle)
{
	if (handle == INVALID_BUFFER)
		return;

	gcpRendD3D->m_DevBufMan.EndReadWrite(handle);
}


std::unique_ptr<IDynTexture> CCustomRenderer::CreateDynamicRenderTarget(const SRTCreationParams &renderTargetParams) const
{
	uint32 flags = renderTargetParams.flags;
	if (!(flags & FT_USAGE_RENDERTARGET) && !(flags & FT_USAGE_DEPTHSTENCIL))
		flags |= FT_USAGE_RENDERTARGET;

	std::unique_ptr<SDynTexture> pDynRt = stl::make_unique<SDynTexture>(
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

void CCustomRenderer::Init()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "UIRenderer");
}


void CCustomRenderer::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	//Patented header incompatibility avoidance!
	if (event == 9001 && wparam != 0)
	{
		ICustomPassRenderer** ppRend = reinterpret_cast<ICustomPassRenderer**>(wparam);
		*ppRend = this;
	}
	else if (event == ESYSTEM_EVENT_FAST_SHUTDOWN || event == ESYSTEM_EVENT_FULL_SHUTDOWN)
	{
		gcpRendD3D->ExecuteRenderThreadCommand([=]() {this->Shutdown(); });
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

void CCustomRenderer::ExecuteRTCommand(std::function<void()> command) const
{
	gcpRendD3D->ExecuteRenderThreadCommand(std::forward<std::function<void()>>(command));
}

void CCustomRenderer::UnregisterRendererInstance(uint32 id)
{
	CryAutoWriteLock<CryRWLock> section(m_instanceLock);
	auto instanceIter = std::find_if(m_instances.begin(), m_instances.end(),
		[id](const CCustomRendererInstance* instance) -> bool { return id == instance->GetID(); }
	);
	if (instanceIter == m_instances.end())
		return;

	m_instances.erase(instanceIter);
}

ICustomRendererImplementation* CCustomRenderer::GetRendererImplementation(uint32 id)
{
	CryAutoReadLock<CryRWLock> section(m_instanceLock);
	auto instanceIter = std::find_if(m_instances.begin(), m_instances.end(),
		[id](const CCustomRendererInstance* instance) -> bool { return id == instance->GetID(); }
	);
	if (instanceIter == m_instances.end())
		return nullptr;

	return (*instanceIter)->GetAssignedImplementation();
}


ITexture* CCustomRenderer::CreateRenderTarget(const SRTCreationParams &renderTargetParams) const
{
	CTexture* pRenderTarget = nullptr;
	auto createFunc = CTexture::GetOrCreateRenderTarget;

	if (renderTargetParams.flags & FT_USAGE_DEPTHSTENCIL)
	{
		createFunc = CTexture::GetOrCreateDepthStencil;
	}

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

InputLayoutHandle Cry::Renderer::CustomPass::CCustomRenderer::RT_RegisterLayout(const SInputElementDescription* pDescriptions, size_t count)
{
	if (!count)
		return 0;

	//Do an inbetween copy to be save
	static_assert(sizeof(SInputElementDescription) == sizeof(D3D11_INPUT_ELEMENT_DESC), "Mismatch between input layout descriptions");
	std::vector<D3D11_INPUT_ELEMENT_DESC> descs(count);
	memcpy(&descs[0], pDescriptions, count * sizeof(SInputElementDescription));

	return CDeviceObjectFactory::CreateCustomVertexFormat(descs.size(), descs.data());
}



InputLayoutHandle CCustomRenderer::RT_RegisterLayout(TArray<SInputElementDescription> &layoutDesc)
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

int CCustomRenderer::RT_RegisterConstantName(const char* name)
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
	auto &last = m_constantNameLookup.back();
	last.first = constantName;
	last.second = CCryNameR(last.first.c_str());
	return m_constantNameLookup.size() - 1;
}

void CCustomRenderer::ExecuteCustomPasses()
{
	PROFILE_LABEL_SCOPE("CUSTOM_PASSES");
	CryAutoReadLock<CryRWLock>  lock(m_instanceLock);

	for (auto &instance : m_instances)
	{
		PROFILE_LABEL_SCOPE_DYNAMIC(instance->GetName(), "CustomPass");
		instance->Execute();
	}
}

#pragma endregion

#pragma region CCustomRendererInstance
CCustomRendererInstance::~CCustomRendererInstance()
{
	g_pCustomRenderer->UnregisterRendererInstance(m_id);
}

void CCustomRendererInstance::CreateCustomPass(const SPassCreationParams &params, uint32 customID /*= ~0u*/)
{
	uint32 id = customID;
	if (id == ~0u)
		id = CryStringUtils::CalculateHash(params.passName.c_str());

	SPassParams passParams = params;

	//Do the actual creation on the render thread
	gcpRendD3D->ExecuteRenderThreadCommand([=]()
	{
		if (auto pPass = RT_GetCustomPass(id))
			pPass->RT_UpdatePassData(passParams);
		else
		{
			TPassEntry  entry(id, std::make_unique<CCustomPass>(params.passName, customID, this));
			entry.second->RT_UpdatePassData(passParams);
			m_pImpl->RT_OnPassCreated(id, entry.second.get());
			m_customPasses.emplace_back(std::move(entry));
		}
	});
}


void CCustomRendererInstance::RemoveCustomPass(uint32 id)
{
	//Deferr actual deletion to the render thread
	gcpRendD3D->ExecuteRenderThreadCommand([=]()
	{
		auto passIter = std::find_if(m_customPasses.begin(), m_customPasses.end(),
			[id](const auto& pass) -> bool { return id == pass.first; }
		);
		if (passIter == m_customPasses.end())
			return;

		m_customPasses.erase(passIter);
	});
}

CCustomPass* CCustomRendererInstance::RT_GetCustomPass(uint32 id) const
{
	auto passIter = std::find_if(m_customPasses.begin(), m_customPasses.end(),
		[id](const auto& pass) -> bool { return id == pass.first; }
	);
	if (passIter == m_customPasses.end())
		return nullptr;
	return passIter->second.get();
}

SPassUpdateScope CCustomRendererInstance::RT_BeginScopedPass(uint32 passID)
{
	if (m_pCurrentPass)
		return SPassUpdateScope();

	auto pPass = RT_GetCustomPass(passID);
	if (!pPass)
		return SPassUpdateScope();

	m_pCurrentPass = pPass;
	m_pCurrentPass->RT_BeginAddingPrimitives(false);
	SPassUpdateScope scope;
	scope.Assign(this);
	return std::move(scope);
}

SPassUpdateScope CCustomRendererInstance::RT_BeginScopedPass(uint32 passID, const SPassParams &updateParams)
{
	auto pass = RT_BeginScopedPass(passID);
	if (pass.IsValid())
		m_pCurrentPass->RT_UpdatePassData(updateParams);

	return std::move(pass);
}

void CCustomRendererInstance::RT_EndScopedPass()
{
	if (!m_pCurrentPass)
		return;

	m_pCurrentPass->RT_ExecutePass();
	m_pCurrentPass = nullptr;
}

void CCustomRendererInstance::RT_ScopedAddPrimitives(const TArray<SPrimitiveParams> &primitives)
{
	if (!m_pCurrentPass)
		return;

	m_pCurrentPass->RT_AddPrimitives(primitives);
}

void CCustomRendererInstance::RT_ScopedAddPrimitive(const SPrimitiveParams &primitive)
{
	if (!m_pCurrentPass)
		return;

	m_pCurrentPass->RT_AddPrimitive(primitive);
}

void CCustomRendererInstance::Execute()
{
	m_pImpl->RT_Render();
	m_constantHeap.FreeUsed();
	//m_primitiveHeap.FreeUsed();
}

CConstantBuffer* CCustomRendererInstance::GetFreeConstantBuffer(size_t bufferSize)
{
	return m_constantHeap.GetUnusedConstantBuffer(bufferSize);
}

CRenderPrimitive* CCustomRendererInstance::GetFreePrimitive()
{
	return nullptr;//m_primitiveHeap.GetUnusedPrimitive();
}

ICustomRendererImplementation* CCustomRendererInstance::GetAssignedImplementation() const
{
	return m_pImpl;
}





#pragma endregion

#pragma region CCustomPass
void CCustomPass::RT_BeginAddingPrimitives(bool bExecutePrevious /*= false*/)
{
	if (bExecutePrevious)
		RT_ExecutePass();

	m_pPass->BeginAddingPrimitives(true);
}

void CCustomPass::RT_UpdatePassData(const SPassParams &params)
{
	m_pPass->SetViewport(params.viewPort);
	m_pPass->SetTargetClearMask(params.clearMask);

	if (params.pColorTarget)
		m_pPass->SetRenderTarget(0, static_cast<CTexture*>(params.pColorTarget));

	if (params.pDepthsTarget)
		m_pPass->SetDepthTarget(static_cast<CTexture*>(params.pDepthsTarget));

	if (!params.scissorRect.IsZero())
	{
		m_pPass->SetScissor(true, { (long)params.scissorRect.x, (long)params.scissorRect.y, (long)params.scissorRect.z, (long)params.scissorRect.w });
	}
}

void CCustomPass::RT_AddPrimitives(const TArray<SPrimitiveParams> &primitives)
{
	for (auto &primitive : primitives)
	{
		RT_AddPrimitive(primitive);
	}
}

void CCustomPass::RT_AddPrimitive(const SPrimitiveParams &primitiveParms)
{
	auto pPrimitive = m_pInstance->GetFreePrimitive();

	CRenderPrimitive::EPrimitiveFlags flags = (CRenderPrimitive::EPrimitiveFlags)primitiveParms.shaderParams.flags;
	pPrimitive->SetFlags(flags);
	pPrimitive->SetPrimitiveType((CRenderPrimitive::EPrimitiveType)primitiveParms.type);
	pPrimitive->SetRenderState(primitiveParms.renderStateFlags);
	pPrimitive->SetStencilState(primitiveParms.stencilState, primitiveParms.stencilRef, primitiveParms.stencilReadMask, primitiveParms.stencilWriteMask);
	pPrimitive->SetCullMode(primitiveParms.cullMode);

	D3D11_RECT rect = { primitiveParms.scissorRect.x, primitiveParms.scissorRect.y, primitiveParms.scissorRect.z, primitiveParms.scissorRect.w };
	pPrimitive->SetScissorRect(rect);

	SetDrawParams(primitiveParms.drawParams, *pPrimitive);
	SetShaderParams(primitiveParms.shaderParams, *pPrimitive);

	SetInlineConstantParams(primitiveParms.constantParameters, *pPrimitive);
	pPrimitive->Compile(*m_pPass.get());
	SetNamedConstantParams(primitiveParms.constantParameters, *pPrimitive);

	m_pPass->AddPrimitive(pPrimitive);
}

void CCustomPass::RT_ExecutePass()
{
	m_pPass->Execute();
}

void CCustomPass::SetDrawParams(const SDrawParams& params, CRenderPrimitive &primitive)
{
	if (params.type == EDrawParamTypes::ExternalBuffers)
	{
		const SDrawParamsExternalBuffers *drawParams = static_cast<const SDrawParamsExternalBuffers*>(&params);
		primitive.SetCustomVertexStream(drawParams->inputBuffer, drawParams->inputLayout, drawParams->inputStride);
		primitive.SetCustomIndexStream(drawParams->idxBuffer, drawParams->isIDX32 ? Index32 : Index16);

		primitive.SetDrawInfo((ERenderPrimitiveType)params.drawType, drawParams->inputOffset, drawParams->idxOffset, drawParams->idxSize);
	}
	else if (params.type == EDrawParamTypes::RenderMesh)
	{
		const SDrawParamsRenderMesh *drawParams = static_cast<const SDrawParamsRenderMesh*>(&params);

		auto* pRenderMesh = static_cast<CRenderMesh*>(drawParams->pRenderMesh);
		if (pRenderMesh)
		{
			pRenderMesh->RT_CheckUpdate(pRenderMesh->_GetVertexContainer(), pRenderMesh->_GetVertexFormat(), 0);

			buffer_handle_t hVertexStream = pRenderMesh->_GetVBStream(VSF_GENERAL);
			buffer_handle_t hIndexStream = pRenderMesh->_GetIBStream();

			if (hVertexStream != ~0u && hIndexStream != ~0u)
			{
				const auto primType = pRenderMesh->_GetPrimitiveType();
				const auto numIndex = pRenderMesh->_GetNumInds();

				primitive.SetCustomVertexStream(hVertexStream, pRenderMesh->_GetVertexFormat(), pRenderMesh->GetStreamStride(VSF_GENERAL));
				primitive.SetCustomIndexStream(hIndexStream, (sizeof(vtx_idx) == 2 ? Index16 : Index32));
				primitive.SetDrawInfo(primType, drawParams->vtxOffset, drawParams->idxOffset, drawParams->idxCount);
			}
		}
	}
}

void CCustomPass::SetShaderParams(const SShaderParams& params, CRenderPrimitive &primitive)
{
	auto pShader = static_cast<CShader*>(params.pShader);
	primitive.SetTechnique(pShader, params.techniqueLCCRC, params.rtMask);

	for (auto &texture : params.textures)
	{
		CTexture* pCTexture = static_cast<CTexture*>(texture.pTexture);
		primitive.SetTexture(texture.slot, pCTexture);
	}

	for (auto &samplerState : params.samplerStates)
	{
		primitive.SetSampler(samplerState.slot, samplerState.handle);
	}
}

void CCustomPass::SetNamedConstantParams(const SConstantParams& paramsConstParams, CRenderPrimitive &primitive)
{
	if (paramsConstParams.type != EConstantParamsType::NamedConstants)
		return;

	auto &constantManager = primitive.GetConstantManager();
	const SNamedConstantsParams& params = static_cast<const SNamedConstantsParams&>(paramsConstParams);
	constantManager.BeginNamedConstantUpdate();

	for (auto &constant : params.namedConstants)
	{
		CCryNameR constantName = g_pCustomRenderer->GetConstantName(constant.constantIDX);
		EHWShaderClass shaderClass = static_cast<EHWShaderClass>(constant.shaderClass);
		constantManager.SetNamedConstant(constantName, constant.value, shaderClass);
	}

	for (auto &constant : params.namedConstantArrays)
	{
		CCryNameR constantName = g_pCustomRenderer->GetConstantName(constant.constantIDX);
		EHWShaderClass shaderClass = static_cast<EHWShaderClass>(constant.shaderClass);
		constantManager.SetNamedConstantArray(constantName, constant.values.Data(), constant.values.size(), shaderClass);
	}

	constantManager.EndNamedConstantUpdate(&m_pPass->GetViewport(), gcpRendD3D->GetActiveGraphicsPipeline()->GetCurrentRenderView());


}
void CCustomPass::SetInlineConstantParams(const SConstantParams& paramsConstParams, CRenderPrimitive &primitive)
{
	if (paramsConstParams.type != EConstantParamsType::ConstantBuffer)
		return;

	//auto &constantManager = primitive.GetConstantManager();
	const SInlineConstantParams& params = static_cast<const SInlineConstantParams&>(paramsConstParams);
	for (auto &buffer : params.buffers)
	{
		CConstantBuffer* pBuffer = nullptr;
		if (buffer.externalBuffer != INVALID_BUFFER)
		{
			pBuffer = reinterpret_cast<CConstantBuffer*>(buffer.externalBuffer);
		}
		else
		{
			pBuffer = m_pInstance->GetFreeConstantBuffer(buffer.dataSize);
		}

		if (buffer.isDirty)
		{
			pBuffer->UpdateBuffer(buffer.newData, buffer.dataSize);
		}

		primitive.SetInlineConstantBuffer((EConstantBufferShaderSlot)buffer.slot, pBuffer, (EShaderStage)buffer.stages);
	}
}

CCustomPass::CCustomPass(string name, uint32 id, CCustomRendererInstance* pInstance)
	: m_name(name)
	, m_id(id)
	, m_pInstance(pInstance)
{
	m_pPass = std::make_unique<CPrimitiveRenderPass>();
	m_pPass->SetLabel(name.c_str());
}


ICustomRendererInstance* CCustomPass::GetOwner() const
{
	return static_cast<ICustomRendererInstance*>(m_pInstance);
}

CCustomPass::CCustomPass(CCustomPass &&other)
	: m_name(std::move(other.m_name))
	, m_id(other.m_id)
	, m_pInstance(other.m_pInstance)
{
	m_pPass = std::move(other.m_pPass);
}

CCustomPass& CCustomPass::operator=(CCustomPass &&other)
{
	m_name = std::move(other.m_name);
	m_id = other.m_id;
	m_pInstance = other.m_pInstance;
	m_pPass = std::move(other.m_pPass);

	return *this;
}


CCustomPass::~CCustomPass()
{
}



#pragma endregion

#pragma region CConstantBuffer

CConstantBuffer* CSizeSortedConstantHeap::GetUnusedConstantBuffer(size_t bufferSize)
{
	if (maxSize)
		return GetUnusedConstantBuffer();
	else if (bufferSize == 0)
		return nullptr;

	CConstantBuffer* pReturn = nullptr;
	if (m_freeSet.empty())
	{
		auto pBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(maxSize);
		m_usedSet.emplace(bufferSize, std::move(pBuffer));
	}
	else
	{
		auto element = m_freeSet.lower_bound({ bufferSize, nullptr });
		pReturn = element->second.get();
		m_usedSet.insert(std::move(*element));
		m_freeSet.erase(element);

	}
	return pReturn;
}



CConstantBuffer* CSizeSortedConstantHeap::GetUnusedConstantBuffer()
{
	if (m_freeList.empty())
	{
		CConstantBufferPtr pConstantBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(maxSize);
		m_usedList.emplace_back(maxSize, std::move(pConstantBuffer));
	}
	else
	{
		auto pPrim = std::move(m_freeList.back());
		m_freeList.pop_back();
		m_usedList.emplace_back(std::move(pPrim));
	}

	return m_usedList.back().second.get();
}


void CSizeSortedConstantHeap::FreeUsed()
{
	if (!maxSize)
		return FreeUsedSets();

	if (m_freeList.empty())
	{
		m_freeList = std::move(m_usedList);
	}
	else
	{
		m_freeList.reserve(m_freeList.size() + m_usedList.size());
		std::move(std::begin(m_usedList), std::end(m_usedList), std::back_inserter(m_freeList));
	}
	m_usedList.clear();
}

void CSizeSortedConstantHeap::FreeUsedSets()
{
	if (m_freeSet.empty())
	{
		m_freeSet = std::move(m_usedSet);
	}
	else
	{
		m_freeSet.insert(m_usedSet.begin(), m_usedSet.end());
	}
	m_usedSet.clear();
}
#pragma endregion
