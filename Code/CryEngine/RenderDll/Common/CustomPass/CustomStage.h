#pragma once	
#include <CryRenderer/CustomPass.h>
#include "GraphicsPipeline/Common/FullscreenPass.h"
#include "GraphicsPipeline/Common/PrimitiveRenderPass.h"
#include "GraphicsPipeline/Common/UtilityPasses.h"


namespace Cry
{
namespace Renderer
{
namespace CustomPass
{

	enum class EPassType : uint8
	{
		None,
		Primitive,
		Fullscreen
	};

	template<class PassType>
	constexpr EPassType GetPassID() { return EPassType::None; };
	template<>
	constexpr EPassType GetPassID<CPrimitiveRenderPass>() { return EPassType::Primitive; };
	template<>
	constexpr EPassType GetPassID<CFullscreenPass>() { return EPassType::Fullscreen; };

	//Wrapper class to expose more persistant passes
	struct IPassWrapper
	{
		EPassType type = EPassType::None;
	};

	template<class PassType>
	struct SPassWrapper : IPassWrapper
	{
		SPassWrapper() : type(GetPassID<EPassType>()) {}

		template<class ... Args>
		SPassWrapper(Args ...args)
			: IPassWrapper({GetPassID<EPassType>()})
			, pass(std::forward<Args>(args)...)
		{}

		PassType pass;
	};

	struct SPassData
	{
		EPassType type = EPassType::None;

		ITexture* pColorTarget = nullptr;
		ITexture* pDepthsTarget = nullptr;

		const char* label = nullptr;
	};

	struct SFullscreenPassData : SPassData
	{
		SFullscreenPassData(SShaderParams& shaderParams, SConstantParams& constantParameters) 
			: SPassData{ EPassType::Fullscreen }
			, shaderParams(shaderParams)
			, constantParameters(constantParameters)		
		{}

		SShaderParams&		shaderParams;
		SConstantParams&	constantParameters;
	};

	struct SPrimitivePassData : SPassData
	{
		SPrimitivePassData() : SPassData{ EPassType::Primitive } {}
	};

	struct SDebugData
	{
		string name;
	};

	template<class PassType>
	struct SPassStorage
	{
		std::vector<std::unique_ptr<SPassWrapper<PassType>>> dynamicPasses;
		size_t firstFree = 0;

		std::vector<std::unique_ptr<SPassWrapper<PassType>>> registeredPasses;
	};

	class CCustomStage
	{
	public:
		CCustomStage(const char* stageName, size_t hintPrimitivePasses, size_t hintFullscreenPasses);

		void UpdatePassData(IPassWrapper& pPass, const SPassData& passData);

		void BeginPass(const SPassData& passData, IPassWrapper* pPass = nullptr);
		void EndPass();

		void DrawPrimitive(const TArray<SPrimitiveParams*>& primitives);
		void DrawPrimitive(const TArray<SPrimitiveParams>& primitives);
		void DrawPrimitive(const SPrimitiveParams& primitiveParms);

		void Execute();
	protected:
		void BeginPass(const SPrimitivePassData& primPass, IPassWrapper* pPass = nullptr);
		void BeginPass(const SFullscreenPassData& screenPass, IPassWrapper* pPass = nullptr);


		void UpdatePass(CPrimitiveRenderPass& pass, const SPrimitivePassData& primPassData);
		void UpdatePass(CFullscreenPass& pass, const SFullscreenPassData& screenPass);

		SDebugData m_debugData;

		SPassStorage<CFullscreenPass>		m_fullscreenPasses;
		SPassStorage<CPrimitiveRenderPass>	m_primitivePasses;

		IPassWrapper*						 m_currentPass;
	};


	template<class PassType>
	static SPassWrapper<PassType>* ConvertWrapper(IPassWrapper* wrapper) { return static_cast<SPassWrapper<PassType>*>(wrapper); }
	template<class PassType>
	static SPassWrapper<PassType>& ConvertWrapper(IPassWrapper& wrapper) { return static_cast<SPassWrapper<PassType>&>(wrapper); }


	template<class PassType>
	static PassType* UnwrapPass(IPassWrapper* wrapper) { return &static_cast<SPassWrapper<PassType>*>(wrapper).pass; }
	template<class PassType>
	static PassType& UnwrapPass(IPassWrapper& wrapper) { return static_cast<SPassWrapper<PassType>&>(wrapper).pass; }

	template<class PassType>
	static SPassWrapper<PassType>& GetNextPass(SPassStorage<PassType>& passStorage)
	{
		if (passStorage.firstFree >= passStorage.dynamicPasses.size())
			passStorage.dynamicPasses.emplace_back();

		return *passStorage.dynamicPasses[passStorage.firstFree++].get();
	}

	template<class PassType>
	static SPassWrapper<PassType>& GetProvidedOrNextPass(IPassWrapper* pPass, SPassStorage<PassType>& passStorage)
	{
		return pPass ? *ConvertWrapper<PassType>(pPass) : GetNextPass(passStorage);
	}
}
}
}