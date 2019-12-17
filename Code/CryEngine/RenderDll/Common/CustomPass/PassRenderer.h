#pragma once

namespace Cry
{
	namespace Renderer
	{
		namespace RenderPass
		{
			static void UpdatePassData(CPrimitiveRenderPass& pass, const SPassParams& params);

			static void BeginPass(CPrimitiveRenderPass& pass);

			static void BeginPass(CPrimitiveRenderPass& pass, const SPassParams& updateParams)
			{
				UpdatePassData(pass, updateParams);
				BeginPass(pass);
			}

			static void AddPrimitive(CPrimitiveRenderPass& pass, const SPrimitiveParams& primitiveParams, CRenderPrimitive& primitive);

		}
	}

}