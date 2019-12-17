#pragma once
#include "CryRenderer/CustomPass.h"
#include <CryRenderer/RenderStages/IStageResources.h>


namespace Cry
{
	namespace Renderer
	{
		template<class HeapType>
		class CUsedHeap
		{
		public:
			HeapType* GetUnused()
			{
				if (m_usedCount > m_primitives.size() || m_usedCount == 0)
					m_primitives.emplace_back();

				return m_primitives[m_usedCount++].get();
			}

			void FreeUsed() { m_usedCount = 0; }

		private:
			std::vector<std::unique_ptr<HeapType>>	m_primitives;
			size_t m_usedCount;
		};

		/// Stages
		struct SStageBase
		{
			SStageBase() {}

			SStageBase(const char* name, uint32 hash)
				: name(name)
				, hash(hash)
			{}

			SStageBase(string name, uint32 hash)
				: name(std::move(name))
				, hash(hash)
			{}

			string	name;
			uint32	hash;
			int		sortID = 0;
		};

		using TStageRenderCallback = std::function<void()>;
		using TStageCreationCallback = std::function<void(SStageBase & stage)>;
		using TStageDestructionCallback = std::function<void(SStageBase & stage)>;

		struct SStageCallbacks
		{
			TStageRenderCallback		renderCallback;
			TStageCreationCallback		creationCallback;
			TStageDestructionCallback	destructionCallback;
		};

		struct SStageDataStorage
		{
			CUsedHeap<CPrimitiveRenderPass>		dynamicPasses;
			std::vector<CPrimitiveRenderPass>	registeredPasses;

			CUsedHeap<CRenderPrimitive>			primitives; //TODO: This can probably be stored in a more clever way
		};

		struct SStage : public SStageBase
		{
			SStage() {}

			SStage(const char* name, uint32 hash)
				: SStageBase(name, hash)
			{}

			SStage(string name, uint32 hash)
				: SStageBase(std::move(name), hash)
			{}

			SStageCallbacks callbacks;

			SStageDataStorage dataStorage;

			CPrimitiveRenderPass* pActivePass = nullptr;
		};

		using TStagePtr = std::unique_ptr<SStage>;

		using TStageList = std::vector<TStagePtr>;
		///~Stages

		struct SRenderListEntry
		{
			SRenderListEntry(const char* name, uint32 hash, TStageRenderCallback callback, int sort = 0)
				: name(name)
				, hash(hash)
				, sort(sort)
				, renderCall(std::move(callback))
			{}

			const char* name;
			uint32		hash;
			int			sort;

			TStageRenderCallback renderCall; //TODO: Replace with small function object to store capturing render lambdas

			bool operator==(const SRenderListEntry& other) { return other.hash == hash; }
			bool operator==(const uint32& otherHash) { return otherHash == hash; }
		};

		using TRenderList = std::vector<SRenderListEntry>;

		class CStageResourceProvider;
		class CStageRenderer
		{
		public:
			static CStageRenderer* Get();
			static CStageRenderer* Create();
			static void			   Destroy();

			void ExecuteRenderThreadCommand(std::function<void()> command);
			void RegisterStage(const char* name, uint32 hash, SStageCallbacks renderCallback);
			
			IStageResourceProvider* GetResourceProvider();

			//Render Thread
			void RT_RegisterStage(string name, uint32 hash, SStageCallbacks renderCallback);
			void RT_RemoveStage(uint32 hash);

			void RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort = 0);
			void RT_RemoveRenderEntry(uint32 hash);

			size_t RT_AllocatePass(SStageBase& stage, const SPassCreationParams& params);

			void RT_BeginPass(SStageBase& stage, size_t passIDX);
			void RT_BeginNewPass(SStageBase& stage);

			void RT_BeginPass(SStageBase& stage, size_t passIDX, const SPassParams& params);
			void RT_BeginNewPass(SStageBase& stage, const SPassParams& params);

			void RT_AddPrimitives(SStageBase& stage, const TArray<SPrimitiveParams>& primitives);
			void RT_AddPrimitive(SStageBase& stage, const SPrimitiveParams& primitive);

			void RT_EndPass(SStageBase& stageBase, bool bExecute);

			void RT_UpdatePassData(SStageBase& stageBase, size_t passIDX, const SPassParams& params);
			//

			void RT_Render();
		protected:
			void RenderStage(SStage& stage);


		protected:
			TStageList	m_stageList;
			TRenderList m_renderList;

			std::unique_ptr<CStageResourceProvider> m_pResourceProvider;
		};
	}
}