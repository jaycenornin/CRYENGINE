#include "StdAfx.h"
#include "StageRenderer.h"
#include "PassRenderer.h"
#include "StageResourceProvider.h"

using namespace Cry;
using namespace Cry::Renderer;

static std::unique_ptr<CStageRenderer> g_pStageRenderer;

CStageRenderer* CStageRenderer::Get()
{
	return g_pStageRenderer.get();
}

CStageRenderer* CStageRenderer::Create()
{
	if (g_pStageRenderer)
		return g_pStageRenderer.get();

	g_pStageRenderer = std::make_unique<CStageRenderer>();
	return g_pStageRenderer.get();
}

void CStageRenderer::Destroy()
{
	g_pStageRenderer.reset();
}

void CStageRenderer::ExecuteRenderThreadCommand(std::function<void()> command)
{
	gcpRendD3D->ExecuteRenderThreadCommand(std::forward<std::function<void()>>(command));
}

void CStageRenderer::RegisterStage(const char* name, uint32 hash, SStageCallbacks callbacks)
{
	ExecuteRenderThreadCommand([pThis = this, cName = string(name), hash, cCb = std::move(callbacks)]() {
		pThis->RT_RegisterStage(std::move(cName), hash, std::move(cCb));
	});
}

void CStageRenderer::RT_RegisterStage(string name, uint32 hash, SStageCallbacks renderCallback)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return;

	auto pStage = std::make_unique<SStage>(std::move(name), hash);
	pStage->callbacks = std::move(renderCallback);

	auto stage = pStage.get();

	m_stageList.push_back(std::move(pStage));
	stage->callbacks.creationCallback(*stage);

	RT_AddRenderEntry(stage->name, hash, [this, stage]() { RenderStage(*stage); });
}

void CStageRenderer::RT_RemoveStage(uint32 hash)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return;

	RT_RemoveRenderEntry(hash);

	auto pStage = std::move(*stageEntry._Ptr);
	m_stageList.erase(stageEntry);

	pStage->callbacks.destructionCallback(*pStage);
}

void CStageRenderer::RT_Render()
{
	for (auto& renderEntry : m_renderList)
	{
		PROFILE_LABEL_SCOPE_DYNAMIC(renderEntry.name, "CustomStage");
		renderEntry.renderCall();
	}
}

void CStageRenderer::RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort /*= 0*/)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry == m_renderList.end())
		return;

	m_renderList.emplace_back(name, hash, std::move(renderCallback));
}

void CStageRenderer::RT_RemoveRenderEntry(uint32 hash)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry != m_renderList.end())
		m_renderList.erase(entry);
}

size_t CStageRenderer::RT_AllocatePass(SStageBase& stageBase, const SPassCreationParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);

	auto pPass = std::make_unique<CPrimitiveRenderPass>();
	pPass->SetLabel(params.passName);
	RenderPass::UpdatePassData(*pPass, params);

	stage.dataStorage.registeredPasses.emplace_back();

	return stage.dataStorage.registeredPasses.size() - 1;
}

Cry::Renderer::IStageResourceProvider* Cry::Renderer::CStageRenderer::GetResourceProvider()
{
	return &*m_pResourceProvider;
}

void Cry::Renderer::CStageRenderer::RT_BeginPass(SStageBase& stageBase, size_t passIDX)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::BeginPass(*stage.pActivePass);
}

void Cry::Renderer::CStageRenderer::RT_BeginNewPass(SStageBase& stageBase, const SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUnused();
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);
}

void Cry::Renderer::CStageRenderer::RT_BeginNewPass(SStageBase& stageBase)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUnused();
	RenderPass::BeginPass(*stage.pActivePass);
}

void Cry::Renderer::CStageRenderer::RT_BeginPass(SStageBase& stageBase, size_t passIDX, const SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);

}

void Cry::Renderer::CStageRenderer::RT_AddPrimitives(SStageBase& stageBase, const TArray<SPrimitiveParams>& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	for (auto& param : params)
	{
		auto pRimitive = stage.dataStorage.primitives.GetUnused();
		RenderPass::AddPrimitive(*stage.pActivePass, param, *pRimitive);
	}
}

void Cry::Renderer::CStageRenderer::RT_AddPrimitive(SStageBase& stageBase, const SPrimitiveParams& param)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	auto pRimitive = stage.dataStorage.primitives.GetUnused();
	RenderPass::AddPrimitive(*stage.pActivePass, param, *pRimitive);
}

void Cry::Renderer::CStageRenderer::RT_EndPass(SStageBase& stageBase, bool bExecute)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	if (bExecute)
		stage.pActivePass->Execute();
	
	stage.pActivePass = nullptr;
}

void Cry::Renderer::CStageRenderer::RT_UpdatePassData(SStageBase& stageBase, size_t passIDX, const SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	auto pass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*pass, params);
}

void Cry::Renderer::CStageRenderer::RenderStage(SStage& stage)
{
	stage.callbacks.renderCallback();
	stage.dataStorage.dynamicPasses.FreeUsed();
	stage.dataStorage.primitives.FreeUsed();
}
