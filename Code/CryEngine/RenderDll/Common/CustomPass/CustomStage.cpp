#include "StdAfx.h"
#include "CustomStage.h"
#include "CryCore/Assert/CryAssert.h"

using namespace Cry;
using namespace Renderer;
using namespace CustomPass;

void CCustomStage::Execute()
{
	PROFILE_LABEL_SCOPE_DYNAMIC(m_debugData.name, "CustomStage");
}

CCustomStage::CCustomStage(const char* stageName, size_t hintPrimitivePasses, size_t hintFullscreenPasses)
	: m_debugData({stageName})
{
	m_primitivePasses.dynamicPasses.resize(hintPrimitivePasses);
	m_fullscreenPasses.dynamicPasses.resize(hintFullscreenPasses);
}



void CCustomStage::UpdatePassData(IPassWrapper& pPass, const SPassData& passData)
{
	if (pPass.type != passData.type)
		return;

	switch (pPass.type)
	{
	case EPassType::Primitive: 
		UpdatePass(
			UnwrapPass<CPrimitiveRenderPass>(pPass),
			static_cast<const SPrimitivePassData&>(passData)
		);
		break;
	case EPassType::Fullscreen: 
		UpdatePass(
			UnwrapPass<CFullscreenPass>(pPass),
			static_cast<const SFullscreenPassData&>(passData)
		);
		break;
	}

}

void CCustomStage::UpdatePass(CPrimitiveRenderPass& pass, const SPrimitivePassData& primPassData)
{

}

void CCustomStage::UpdatePass(CFullscreenPass& pass, const SFullscreenPassData& screenPass)
{
	
}


void CCustomStage::BeginPass(const SPassData& passData, IPassWrapper* pPass)
{
	switch (passData.type)
	{
	case EPassType::Primitive: BeginPass(static_cast<const SPrimitivePassData&>(passData), pPass); break;
	case EPassType::Fullscreen: BeginPass(static_cast<const SFullscreenPassData&>(passData), pPass); break;
	}


}

void CCustomStage::BeginPass(const SPrimitivePassData& primPass, IPassWrapper* pPass)
{
	auto& pass = GetProvidedOrNextPass(pPass, m_primitivePasses);

	if(!pPass)
		UpdatePass(pass.pass, primPass);

	m_currentPass = &pass;
}

void CCustomStage::BeginPass(const SFullscreenPassData& screenPass, IPassWrapper* pPass)
{
	auto& pass = GetProvidedOrNextPass(pPass, m_fullscreenPasses);

	if (!pPass)
		UpdatePass(pass.pass, screenPass);

	m_currentPass = &pass;
}

void CCustomStage::EndPass()
{
	bool bHavePass = m_currentPass == nullptr ? false : (m_currentPass->type != EPassType::None);
	CRY_ASSERT_MESSAGE(bHavePass, "No Render pass currently active");
	if (bHavePass)
	{
		//No current pass selected.
		return;
	}

	switch (m_currentPass->type)
	{
	case EPassType::Primitive: UnwrapPass<CPrimitiveRenderPass>(*m_currentPass).Execute(); break;
	case EPassType::Fullscreen: UnwrapPass<CFullscreenPass>(*m_currentPass).Execute();; break;
	}

	m_currentPass = nullptr;
}


void CCustomStage::DrawPrimitive(const SPrimitiveParams& primitiveParms)
{

}

void CCustomStage::DrawPrimitive(const TArray<SPrimitiveParams>& primitives)
{
	for (auto& primitive : primitives)
		DrawPrimitive(primitive);
}

void CCustomStage::DrawPrimitive(const TArray<SPrimitiveParams*>& primitives)
{
	for (auto primitive : primitives)
		DrawPrimitive(*primitive);
}

