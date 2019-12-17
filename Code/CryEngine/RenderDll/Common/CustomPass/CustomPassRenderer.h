#pragma once	
#include <CryRenderer/CustomPass.h>


namespace Cry
{
namespace Renderer
{
namespace CustomPass
{
class CCustomRendererInstance;
class CCustomPass : public ICustomPass
{
public:
	CCustomPass(string name, uint32 id, CCustomRendererInstance* pInstance);
	CCustomPass(CCustomPass &&other);
	CCustomPass& operator=(CCustomPass &&other);

	virtual ~CCustomPass();

	virtual const char* GetName() const override { return m_name.c_str(); }
	virtual ICustomRendererInstance* GetOwner() const override final;
	virtual uint32 GetID() const override { return m_id; }


	virtual void RT_BeginAddingPrimitives(bool bExecutePrevious = false) override;
	virtual void RT_UpdatePassData(const SPassParams &params) override;
	virtual void RT_AddPrimitives(const TArray<SPrimitiveParams> &primitives) override;
	virtual void RT_AddPrimitive(const SPrimitiveParams &primitiveParms) override;
	virtual void RT_ExecutePass() override;
private:
	void SetDrawParams(const SDrawParams& params, CRenderPrimitive &primitive);
	void SetShaderParams(const SShaderParams& params, CRenderPrimitive &primitive);
	void SetInlineConstantParams(const SConstantParams& params, CRenderPrimitive &primitive);
	void SetNamedConstantParams(const SConstantParams& params, CRenderPrimitive &primitive);
private:
	CCustomRendererInstance* m_pInstance;
	string m_name;
	uint32 m_id;

	std::unique_ptr<CPrimitiveRenderPass> m_pPass;
};

class CSizeSortedConstantHeap
{
public:
	CSizeSortedConstantHeap(size_t maxConstantSize) : maxSize(maxConstantSize) {}

	using THeapEntry = std::pair<size_t, CConstantBufferPtr>;
	struct HeapCompare
	{
		bool operator()(const THeapEntry &l, const THeapEntry &r) const{
			return l.first < r.first;
		}
	};

	CConstantBuffer*	GetUnusedConstantBuffer(size_t bufferSize);
	void				FreeUsed();


private:
	const size_t maxSize;
	void				FreeUsedSets();
	CConstantBuffer*	GetUnusedConstantBuffer();

	std::vector<THeapEntry> m_usedList;
	std::vector<THeapEntry> m_freeList;

	std::multiset<THeapEntry, HeapCompare> m_usedSet;
	std::multiset<THeapEntry, HeapCompare> m_freeSet;
};

class CCustomRendererInstance : public ICustomRendererInstance
{
public:
	CCustomRendererInstance(string name, uint32 id, ICustomRendererImplementation* implementation, size_t maxConstantSize)
		: m_name(name)
		, m_id(id)
		, m_pImpl(implementation)
		, m_constantHeap(maxConstantSize)
		, m_pCurrentPass(nullptr)
	{}

	

	virtual ~CCustomRendererInstance();

	virtual uint32 GetID() const { return m_id; }
	virtual const char* GetName() const { return m_name.c_str(); }


	virtual void			CreateCustomPass(const SPassCreationParams &params, uint32 customID = ~0u) override;
	virtual void			RemoveCustomPass(uint32 id) override;
	virtual CCustomPass*	RT_GetCustomPass(uint32 id) const override;

	virtual SPassUpdateScope RT_BeginScopedPass(uint32 passID) override;
	virtual SPassUpdateScope RT_BeginScopedPass(uint32 passID, const SPassParams &updateParams) override;
	virtual void RT_ScopedAddPrimitives(const TArray<SPrimitiveParams> &primitives) override;
	virtual void RT_ScopedAddPrimitive(const SPrimitiveParams &primitive) override;
	void Execute();

	virtual CRenderPrimitive*	GetFreePrimitive();
	virtual CConstantBuffer*	GetFreeConstantBuffer(size_t bufferSize);

	virtual ICustomRendererImplementation* GetAssignedImplementation() const override;
protected:
	virtual void RT_EndScopedPass() override;

private:

private:
	CSizeSortedConstantHeap			m_constantHeap;
	//CPrimitiveHeap					m_primitiveHeap;

	const string m_name;
	const uint32 m_id;
	ICustomRendererImplementation* const m_pImpl;

	CryRWLock m_passesLock;

	using TPassEntry = std::pair<uint32, std::unique_ptr<CCustomPass>>;
	std::vector<TPassEntry>		m_customPasses;
	CCustomPass*				m_pCurrentPass = nullptr;
};

//Primary access point to easily create render passes for UI purposes from the main thread.
//Using the direct callback interface is preferred 
class CCustomRenderer : public ICustomPassRenderer , public ISystemEventListener
{  
public:
	~CCustomRenderer();

	void Shutdown();

	void Init();

	static CCustomRenderer* GetCustomPassRenderer();

	virtual void	ExecuteRTCommand(std::function<void()> command) const override;

	virtual ITexture*						CreateRenderTarget(const SRTCreationParams &renderTargetParams) const override final;
	virtual std::unique_ptr<IDynTexture>	CreateDynamicRenderTarget(const SRTCreationParams &renderTargetParams) const override final;

	virtual void	CreateRendererInstance(const char* name, ICustomRendererImplementation* pImpl, size_t maxConstantSize = 0, uint32 customID = ~0u) override final;
	virtual ICustomRendererImplementation*				GetRendererImplementation(uint32 id) override final;

	virtual void										UnregisterRendererInstance(uint32 id);

	virtual int					RT_RegisterConstantName(const char* name)  override final;

	virtual InputLayoutHandle	RT_RegisterLayout(const SInputElementDescription* pDescriptions, size_t count) override final;
	virtual InputLayoutHandle	RT_RegisterLayout(TArray<SInputElementDescription> &layoutDesc)  override final;
	virtual void				RT_RegisterSamplers()  override final {};
	
	virtual uintptr_t			RT_CreateConstantBuffer(size_t sizeInBytes) override final;
	virtual void				RT_FreeConstantBuffer(uintptr_t buffer) override final;

	virtual uintptr_t			RT_CreateOrUpdateBuffer(const SBufferParams &params, uintptr_t bufferHandle = INVALID_BUFFER) override;
	virtual void				RT_FreeBuffer(uintptr_t bufferHandle);

	virtual void*				RT_BufferBeginWrite(uintptr_t handle) override final;
	virtual void				RT_BufferEndWrite(uintptr_t handle) override final;

	virtual void				RT_ClearRenderTarget(ITexture* pTarget, ColorF clearColor)const  override ;

	void ExecuteCustomPasses();

	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

	CCryNameR GetConstantName(int idx) { return m_constantNameLookup[idx].second; }

private:

	std::vector<std::pair<std::string,CCryNameR>>			m_constantNameLookup;


	CryCriticalSection								m_implLock;

	CryRWLock										m_instanceLock;
	std::vector<CCustomRendererInstance*>			m_instances;
};
}
}
}
