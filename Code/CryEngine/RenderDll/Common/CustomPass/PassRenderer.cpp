#include "StdAfx.h"
#include "PassRenderer.h"


void Cry::Renderer::RenderPass::UpdatePassData(CPrimitiveRenderPass& pass, const SPassParams& params)
{
	pass.SetViewport(params.viewPort);
	pass.SetTargetClearMask(params.clearMask);

	if (auto pTex = static_cast<CTexture*>(params.pColorTarget))
		pass.SetRenderTarget(0, pTex);

	if (auto pTex = static_cast<CTexture*>(params.pDepthsTarget))
		pass.SetDepthTarget(pTex);

	if (!params.scissorRect.IsZero())
		pass.SetScissor(true, { (long)params.scissorRect.x, (long)params.scissorRect.y, (long)params.scissorRect.z, (long)params.scissorRect.w });
	else
		pass.SetScissor(false, { 0,0,0,0 });

}

void Cry::Renderer::RenderPass::BeginPass(CPrimitiveRenderPass& pass)
{
	pass.BeginAddingPrimitives();
}

void Cry::Renderer::RenderPass::AddPrimitive(CPrimitiveRenderPass& pass, const SPrimitiveParams& primitiveParams, CRenderPrimitive& primitive)
{

}

