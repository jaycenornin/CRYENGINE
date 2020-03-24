#include "StdAfx.h"
#include "CustomPipeline.h"
#include "PassRenderer.h"
#include "StageResourceProvider.h"

using namespace Cry;
using namespace Cry::Renderer;
using namespace Cry::Renderer::Pipeline;

static CCustomPipeline* g_pStageRenderer = nullptr;
static ICustomPipelinePtr g_pIPipelineUnknown;

CCustomPipeline* CCustomPipeline::Get()
{
	return g_pStageRenderer;
}

CCustomPipeline* CCustomPipeline::Create()
{
	if (g_pStageRenderer)
		return g_pStageRenderer;

	g_pIPipelineUnknown = GetOrCreateCustomPipeline();
	if (!g_pIPipelineUnknown)
		return nullptr;

	g_pStageRenderer = static_cast<CCustomPipeline*>(g_pIPipelineUnknown.get());
	return g_pStageRenderer;
}

void CCustomPipeline::Destroy()
{
	g_pStageRenderer->Shutdown();

	g_pIPipelineUnknown.reset();
	g_pStageRenderer = nullptr;
}


CCustomPipeline::CCustomPipeline()
{
	CryLogAlways("Creating custom pipeline");
	m_pResourceProvider = std::make_unique<Renderer::CStageResourceProvider>();
}

void CCustomPipeline::Shutdown()
{
	for (auto& pStage : m_stageList)
	{
		StageDestructionsArguments args{ pStage,true };
		pStage->callbacks.destructionCallback(args);
	}
	m_stageList.clear();
	m_renderList.clear();
}


void CCustomPipeline::ExecuteRenderThreadCommand(TRenderThreadCommand command)
{
	gcpRendD3D->ExecuteRenderThreadCommand(std::forward<TRenderThreadCommand>(command));
}

void CCustomPipeline::CreateRenderStage(const char* name, uint32 hash, SStageCallbacks callbacks)
{
	assert(callbacks.creationCallback && callbacks.destructionCallback, "Failed to create render stage. No creation and destruction callbacks were provided.");

	ExecuteRenderThreadCommand([pThis = this, cName = string(name), hash, cCb = std::move(callbacks)]() {
		pThis->RT_CreateRenderStage(std::move(cName), hash, std::move(cCb));
	});
}

_smart_ptr<SStageBase> CCustomPipeline::RT_CreateRenderStage(string name, uint32 hash, SStageCallbacks renderCallback)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return nullptr;

	_smart_ptr<SStage> pStage = new SStage(std::move(name), hash);
	pStage->callbacks = std::move(renderCallback);

	auto stage = pStage.get();

	m_stageList.push_back(pStage);

	StageCreationArguments args{ pStage };

	if (stage->callbacks.creationCallback)
		stage->callbacks.creationCallback(args);

	if (stage->callbacks.renderCallback)
		RT_AddRenderEntry(stage->name, hash, [this, stage](StageRenderArguments& args) { RenderStage(*stage); });

	return pStage;
}

void CCustomPipeline::RT_RemoveStage(uint32 hash)
{
	auto stageEntry = std::find_if(m_stageList.begin(), m_stageList.end(), [hash](auto& entry) { return hash == entry->hash; });
	if (stageEntry != m_stageList.end())
		return;

	RT_RemoveRenderEntry(hash);

	auto pStage = std::move(*stageEntry._Ptr);
	m_stageList.erase(stageEntry);

	StageDestructionsArguments args{ pStage };

	pStage->callbacks.destructionCallback(args);
}

void CCustomPipeline::RT_Render()
{
	for (auto& renderEntry : m_renderList)
	{
		PROFILE_LABEL_SCOPE_DYNAMIC(renderEntry.name, "CustomStage");

		StageRenderArguments args{};
		renderEntry.renderCall(args);
	}
}

void CCustomPipeline::RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort /*= 0*/)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry != m_renderList.end())
		return;

	m_renderList.emplace_back(name, hash, std::move(renderCallback));
}

void CCustomPipeline::RT_RemoveRenderEntry(uint32 hash)
{
	auto entry = std::find(m_renderList.begin(), m_renderList.end(), hash);
	if (entry != m_renderList.end())
		m_renderList.erase(entry);
}

uint32 CCustomPipeline::RT_AllocatePass(SStageBase& stageBase, const Pass::SPassCreationParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);

	auto pPass = std::make_unique<CPrimitiveRenderPass>();
	pPass->SetLabel(params.passName);
	RenderPass::UpdatePassData(*pPass, params);

	stage.dataStorage.registeredPasses.emplace_back();

	return stage.dataStorage.registeredPasses.size() - 1;
}

IStageResourceProvider* CCustomPipeline::GetResourceProvider() const
{
	return m_pResourceProvider.get();
}

void CCustomPipeline::RT_BeginPass(SStageBase& stageBase, uint32 passIDX)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginNewPass(SStageBase& stageBase, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUnused();
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginNewPass(SStageBase& stageBase)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass)
		return;

	stage.pActivePass = stage.dataStorage.dynamicPasses.GetUnused();
	RenderPass::BeginPass(*stage.pActivePass);
}

void CCustomPipeline::RT_BeginPass(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (stage.pActivePass || passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	stage.pActivePass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*stage.pActivePass, params);
	RenderPass::BeginPass(*stage.pActivePass);

}

void CCustomPipeline::RT_AddPrimitives(SStageBase& stageBase, const TArray<Pass::SPrimitiveParams>& params)
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

void CCustomPipeline::RT_AddPrimitive(SStageBase& stageBase, const Pass::SPrimitiveParams& param)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	auto pRimitive = stage.dataStorage.primitives.GetUnused();
	if (!pRimitive)
		return;

	RenderPass::AddPrimitive(*stage.pActivePass, param, *pRimitive);
}

void CCustomPipeline::RT_EndPass(SStageBase& stageBase, bool bExecute)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (!stage.pActivePass)
		return;

	if (bExecute)
		stage.pActivePass->Execute();
	
	stage.pActivePass = nullptr;
}

void CCustomPipeline::RT_UpdatePassData(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params)
{
	SStage& stage = static_cast<SStage&>(stageBase);
	if (passIDX >= stage.dataStorage.registeredPasses.size())
		return;

	auto pass = &stage.dataStorage.registeredPasses[passIDX];
	RenderPass::UpdatePassData(*pass, params);
}

void CCustomPipeline::RenderStage(SStage& stage)
{
	StageRenderArguments args{};
	stage.callbacks.renderCallback(args);
	RT_ResetDynamicStageData(stage);
}

void CCustomPipeline::RT_ResetDynamicStageData(SStageBase& stageBase)
{
	auto& stage = static_cast<SStage&>(stageBase);

	stage.dataStorage.dynamicPasses.FreeUsed();
	stage.dataStorage.primitives.FreeUsed();
}



CRYREGISTER_SINGLETON_CLASS(Cry::Renderer::Pipeline::CCustomPipeline);