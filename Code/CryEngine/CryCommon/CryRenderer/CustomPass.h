#pragma once

namespace Cry {
	namespace Renderer
	{
		namespace CustomPass
		{
			#define INVALID_BUFFER ~0u

			//Defines that should live somewhere else.
			//Stencil flags
			#define CLEAR_ZBUFFER           0x00000001l  /* Clear target z buffer, equals D3D11_CLEAR_DEPTH */
			#define CLEAR_STENCIL           0x00000002l  /* Clear stencil planes, equals D3D11_CLEAR_STENCIL */
			#define CLEAR_RTARGET           0x00000004l  /* Clear target surface */
			//Stencil operations
			//==================================================================
			// StencilStates

			//Note: If these are altered, g_StencilFuncLookup and g_StencilOpLookup arrays
			//			need to be updated in turn
			enum EStencilStateFunction
			{
				FSS_STENCFUNC_ALWAYS = 0x0,
				FSS_STENCFUNC_NEVER = 0x1,
				FSS_STENCFUNC_LESS = 0x2,
				FSS_STENCFUNC_LEQUAL = 0x3,
				FSS_STENCFUNC_GREATER = 0x4,
				FSS_STENCFUNC_GEQUAL = 0x5,
				FSS_STENCFUNC_EQUAL = 0x6,
				FSS_STENCFUNC_NOTEQUAL = 0x7,
				FSS_STENCFUNC_MASK = 0x7
			};

			#define FSS_STENCIL_TWOSIDED   0x8

			#define FSS_CCW_SHIFT          16

			enum EStencilStateOp
			{
				FSS_STENCOP_KEEP = 0x0,
				FSS_STENCOP_REPLACE = 0x1,
				FSS_STENCOP_INCR = 0x2,
				FSS_STENCOP_DECR = 0x3,
				FSS_STENCOP_ZERO = 0x4,
				FSS_STENCOP_INCR_WRAP = 0x5,
				FSS_STENCOP_DECR_WRAP = 0x6,
				FSS_STENCOP_INVERT = 0x7
			};

			#define FSS_STENCFAIL_SHIFT    4
			#define FSS_STENCFAIL_MASK     (0x7 << FSS_STENCFAIL_SHIFT)

			#define FSS_STENCZFAIL_SHIFT   8
			#define FSS_STENCZFAIL_MASK    (0x7 << FSS_STENCZFAIL_SHIFT)

			#define FSS_STENCPASS_SHIFT    12
			#define FSS_STENCPASS_MASK     (0x7 << FSS_STENCPASS_SHIFT)

			#define STENC_FUNC(op)        (op)
			#define STENC_CCW_FUNC(op)    (op << FSS_CCW_SHIFT)
			#define STENCOP_FAIL(op)      (op << FSS_STENCFAIL_SHIFT)
			#define STENCOP_ZFAIL(op)     (op << FSS_STENCZFAIL_SHIFT)
			#define STENCOP_PASS(op)      (op << FSS_STENCPASS_SHIFT)
			#define STENCOP_CCW_FAIL(op)  (op << (FSS_STENCFAIL_SHIFT + FSS_CCW_SHIFT))
			#define STENCOP_CCW_ZFAIL(op) (op << (FSS_STENCZFAIL_SHIFT + FSS_CCW_SHIFT))
			#define STENCOP_CCW_PASS(op)  (op << (FSS_STENCPASS_SHIFT + FSS_CCW_SHIFT))

			//Stencil masks
			#define BIT_STENCIL_RESERVED          0x80
			#define BIT_STENCIL_INSIDE_CLIPVOLUME 0x40
			#define STENCIL_VALUE_OUTDOORS        0x0

			#define STENC_VALID_BITS_NUM          7
			#define STENC_MAX_REF                 ((1 << STENC_VALID_BITS_NUM) - 1)

			enum class EBufferBindType : uint8
			{
				Vertex,
				Index,
				Constant
			};

			enum class EBufferUsage : uint8
			{
				Immutable,					// For data that never, ever changes
				Static,                    // For long-lived data that changes infrequently (every n-frames)
				Dynamic,                   // For short-lived data that changes frequently (every frame)
				Transient,                 // For very short-lived data that can be considered garbage after first usage
				Transient_RT,              // For very short-lived data that can be considered garbage after first usage
				When_Loadingthread_Active, // yes we can ... because renderloadingthread frames not synced with mainthread frames
			};

			enum EPrimitiveType
			{
				ePrim_Triangle,
				ePrim_ProceduralTriangle,     // Triangle generated procedurally on GPU (no vertex stream)
				ePrim_ProceduralQuad,         // Quad generated procedurally on GPU (no vertex stream)
				ePrim_UnitBox,                // axis aligned box ( 0, 0, 0) - (1,1,1)
				ePrim_CenteredBox,            // axis aligned box (-1,-1,-1) - (1,1,1)
				ePrim_Projector,              // pyramid shape with sparsely tessellated ground plane
				ePrim_Projector1,             // pyramid shape with semi densely tessellated ground plane
				ePrim_Projector2,             // pyramid shape with densely tessellated ground plane
				ePrim_ClipProjector,          // same as ePrim_Projector  but with even denser tessellation
				ePrim_ClipProjector1,         // same as ePrim_Projector1 but with even denser tessellation
				ePrim_ClipProjector2,         // same as ePrim_Projector2 but with even denser tessellation
				ePrim_FullscreenQuad,         // fullscreen quad             ( 0  0, 0) - ( 1  1, 0)
				ePrim_FullscreenQuadCentered, // fullscreen quad             (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
				ePrim_FullscreenQuadTess,     // tessellated fullscreen quad (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
				ePrim_Custom,

				ePrim_Count,
				ePrim_First = ePrim_Triangle
			};

			struct SBufferParams
			{
				SBufferParams(size_t elementCount, size_t stride, EBufferBindType type = EBufferBindType::Vertex, EBufferUsage usage = EBufferUsage::Dynamic)
					: elementCount(elementCount), stride(stride), type(type), usage(usage)
				{}

				EBufferBindType type = EBufferBindType::Vertex;
				EBufferUsage usage = EBufferUsage::Transient;
				void*		pData = nullptr;
				size_t		stride = 0;
				size_t      elementCount = 0;
			};

			enum class EShaderClass : uint8
			{
				Vertex = 0,
				Pixel = 1,
				Geometry = 2,
				Domain = 3,
				Hull = 4,
				NumGfx = 5,

				Compute = 5,
				Num = 6
			};

  
			enum EShaderStages : uint8
			{
				ssVertex		=	BIT(0),
				ssPixel		=	BIT(1),
				ssGeometry	=	BIT(2),
				ssDomain		=	BIT(3),
				ssHull		=	BIT(4),
				ssCompute		=	BIT(5),

				ssCountGfx =	(uint8)EShaderClass::Compute,
				ssCount =		(uint8)EShaderClass::Num,
				ssNone = 0,
				ssAllWithoutCompute = ssVertex | ssPixel | ssGeometry | ssDomain | ssHull,
				ssAll = ssAllWithoutCompute | ssCompute,
			};

			enum EInputElementFormat
			{
				FORMAT_R32G32B32A32_FLOAT = 2,
				FORMAT_R32G32_FLOAT = 16,
				R8G8B8A8_UNORM = 28,
				R8G8B8A8_UINT = 30
			};

			enum EInputSlotClassification
			{
				PER_VERTEX_DATA = 0,
				PER_INSTANCE_DATA = 1
			};

			struct SInputElementDescription
			{
				const char* semanticName;
				uint32		semanticIndex;
				EInputElementFormat		format; //Dxgi format
				uint32		inputSlot;
				uint32		alignedByteOffset;
				EInputSlotClassification		inputSlotClassification; //D3D11_INPUT_CLASSIFICATION
				uint32		instanceDataStepRate;
			};

			enum class EDrawParamTypes : uint8
			{
				None,
				ExternalBuffers,
				RenderMesh,
				TargetClear
			};

			enum class EDrawTypes : uint16
			{
				Unknown = 9000,
				TriangleList = 4,
			};

			struct SDrawParams
			{
				EDrawParamTypes type = EDrawParamTypes::None;
				EDrawTypes		drawType = EDrawTypes::TriangleList;
			};

			struct SDrawParamsExternalBuffers : public SDrawParams
			{
				SDrawParamsExternalBuffers() { type = EDrawParamTypes::ExternalBuffers; }

				uintptr_t			inputBuffer = ~0u;
				size_t				inputSize = 0;
				uint32				inputStride = 0;
				InputLayoutHandle	inputLayout;

				uintptr_t			idxBuffer = ~0u;;
				bool				isIDX32 = sizeof(vtx_idx) == 2 ? false : true;
				size_t				idxSize = 0;

				size_t				inputOffset = 0;
				size_t				idxOffset = 0;
			};

			struct SDrawParamsRenderMesh : public SDrawParams
			{
				SDrawParamsRenderMesh() { type = EDrawParamTypes::RenderMesh; }
				IRenderMesh* pRenderMesh = nullptr;
				size_t	vtxOffset = 0;
				size_t  idxOffset = 0;
				size_t	idxCount = 0;

			};

			struct SDrawParamsClearPass : public SDrawParams
			{
				SDrawParamsClearPass() { type = EDrawParamTypes::TargetClear; }
			};

			struct STextureParam
			{
				ITexture* pTexture = nullptr;
				uint32	  slot = 0;
			};

			struct SSamplerParam
			{
				SamplerStateHandle handle;
				uint32			   slot = 0;
			};

			//Literally just copied from renderer code. Might as well be in a public header.
			enum class EShaderReflectFlags : uint32
			{
				None = 0,
				ReflectVS = BIT(0),
				ReflectPS = BIT(1),
				ReflectGS = BIT(2),
				ReflectVS_PS = ReflectVS | ReflectPS,
				ReflectAll = ReflectVS_PS | ReflectGS
			};

			struct SShaderParams
			{
				EShaderReflectFlags flags = EShaderReflectFlags::None;
				IShader*	pShader = nullptr;
				uint32		techniqueLCCRC; //lowercase technique crc32
				uint64		rtMask = 0;

				//
				TArray<STextureParam>	textures;
				TArray<SSamplerParam>	samplerStates;
			};

			enum class EConstantParamsType
			{
				NoConstants,
				NamedConstants,
				ConstantBuffer
			};

			struct SConstantParams
			{
				EConstantParamsType type = EConstantParamsType::NoConstants;
			};

			struct SNamedConstant
			{
				//The preregistered constant id
				uint32 constantIDX;
				EShaderClass shaderClass;
				Vec4   value;
			};

			struct SNamedConstantArray
			{
				//The preregistered constant id
				uint32 constantIDX;
				EShaderClass shaderClass;
				TArray<Vec4> values;
			};

			struct SNamedConstantsParams : public SConstantParams
			{
				SNamedConstantsParams()
				{
					type = EConstantParamsType::NamedConstants;
				}

				TArray<SNamedConstant>		 namedConstants;
				TArray<SNamedConstantArray>  namedConstantArrays;
			};

			enum  class EConstantSlot : uint8
			{
				PerDraw			= 0,
				PerMaterial		= 1,
				SkinQuat		= 2,
				SkinQuatPrev	= 3,
				PerGroup		= 4,
				PerPass			= 5,
				PerView			= 6,
				VrProjection	= 7,
			};

			struct SConstantBuffer
			{
				uintptr_t	externalBuffer = INVALID_BUFFER;
				bool		isDirty = true;

				uint8*  newData			= nullptr;
				uint32  dataSize		= 0;
				EConstantSlot slot	= EConstantSlot::PerDraw;
				EShaderStages stages		= EShaderStages(ssVertex | ssPixel);
			};

			struct SInlineConstantParams : public SConstantParams
			{
				SInlineConstantParams() { 
					type = EConstantParamsType::ConstantBuffer;
				}

				TArray<SConstantBuffer> buffers;
			};

			struct SPrimitiveParams
			{
				SPrimitiveParams(SShaderParams &shaderParams, SConstantParams &constantParameters, SDrawParams &drawParams)
					: drawParams(drawParams)
					, shaderParams(shaderParams)
					, constantParameters(constantParameters)
				{}

				EPrimitiveType	type = EPrimitiveType::ePrim_Custom;

				uint32			renderStateFlags = 0;

				uint8			stencilRef = 0;
				uint8			stencilWriteMask = uint8(~0U);
				uint8			stencilReadMask = uint8(~0U);
				int				stencilState = 0;

				ECull			cullMode = eCULL_None;

				Vec4_tpl<ulong>			scissorRect = ZERO;

				SDrawParams	&drawParams;
				SShaderParams			&shaderParams;
				SConstantParams		&constantParameters;
			};

			struct SPassParams
			{
				SRenderViewport viewPort;
				ITexture*		pColorTarget = nullptr;
				ITexture*		pDepthsTarget = nullptr;
				uint32			clearMask = 0;

				//Top, Left, Right, Bottom
				Vec4_tpl<ulong> scissorRect = { 0,0,0,0 };
			};

			
			struct SPassCreationParams : public SPassParams
			{
				string passName;
			};

			//Parameters for render target creation
			struct SRTCreationParams
			{
				const char*		rtName;
				int				width;
				int				height;
				ColorF			clearColor;
				ETEX_Type		textureType;
				uint32			flags;
				ETEX_Format		format;
				int				customID;
			};

			struct ICustomRendererInstance;
			struct ICustomPass
			{
				virtual const char*					GetName() const = 0;
				virtual ICustomRendererInstance*	GetOwner() const = 0;
				virtual uint32						GetID() const = 0;

				virtual void RT_BeginAddingPrimitives(bool bExecutePrevious = false) = 0;
				virtual void RT_UpdatePassData(const SPassParams &params) = 0;

				virtual void RT_AddPrimitives(const TArray<SPrimitiveParams> &primitives) = 0;
				virtual void RT_AddPrimitive(const SPrimitiveParams &primitiveParms) = 0;

				//Executes the current pass and clears primitives/constant buffers
				virtual void RT_ExecutePass() = 0;

			};

			struct ICustomRendererImplementation;
			//A per implementation renderer instance that manages 
			struct ICustomRendererInstance
			{
				friend struct SPassUpdateScope;
				virtual ~ICustomRendererInstance(){}

				virtual void			CreateCustomPass(const SPassCreationParams &params, uint32 customID = ~0u) = 0;
				virtual void			RemoveCustomPass(uint32 id) = 0;
				
				virtual ICustomPass*	RT_GetCustomPass(uint32 id) const = 0;

				//Begin a scoped Render pass. Add primitives during this. Pass will execute after scope is done.
				virtual SPassUpdateScope RT_BeginScopedPass(uint32 passID) = 0;
				virtual SPassUpdateScope RT_BeginScopedPass(uint32 passID,const SPassParams &updateParams) = 0;

				virtual void RT_ScopedAddPrimitives(const TArray<SPrimitiveParams> &primitives) = 0;
				virtual void RT_ScopedAddPrimitive(const SPrimitiveParams &primitive) = 0;

				virtual const char* GetName() const = 0;
				
				//Direct accessor functions
				virtual ICustomRendererImplementation* GetAssignedImplementation() const = 0;
			protected:
				virtual void RT_EndScopedPass() = 0;
			};

			struct ICustomRendererImplementation
			{
				// These calls come directly from the render thread
				virtual bool RT_Initalize(std::unique_ptr<ICustomRendererInstance> pInstance) = 0;
				virtual void RT_Shutdown() = 0;
				virtual void RT_Update(bool bLoadingThread = false) = 0;
				virtual void RT_Render() = 0;
				virtual void RT_OnPassCreated(uint32 id, ICustomPass* pPass) {}
			};

			struct SPassUpdateScope
			{
				friend struct ICustomRendererInstance;

				SPassUpdateScope() = default;
				SPassUpdateScope(ICustomRendererInstance* pInstance, uint32 id)
				{
					if (!pRenderer && pInstance)
						*this = std::move(pInstance->RT_BeginScopedPass(id));
				}

				SPassUpdateScope(SPassUpdateScope &other) = delete;

				SPassUpdateScope(SPassUpdateScope &&other)
				{
					pRenderer = other.pRenderer;
					other.pRenderer = nullptr;
				}

				~SPassUpdateScope()
				{
					if (pRenderer)
						pRenderer->RT_EndScopedPass();
				}

				SPassUpdateScope &operator=(const SPassUpdateScope &other) = delete;
				SPassUpdateScope &operator=(SPassUpdateScope &&other)
				{
					pRenderer = other.pRenderer;
					other.pRenderer = nullptr;
					return *this;
				};

				bool IsValid() { return pRenderer != nullptr; }

				void Assign(ICustomRendererInstance* pInstance) { if (!pRenderer) { pRenderer = pInstance; }}
			private:
				ICustomRendererInstance* pRenderer = nullptr;
			};


			struct ICustomPassRenderer
			{
				friend struct SPassUpdateScope;
				//virtual void RegisterImplementation(IUserInterfaceImplementation *impl);
				virtual void	ExecuteRTCommand(std::function<void()> command) const = 0;

				virtual ITexture*						CreateRenderTarget(const SRTCreationParams &renderTargetParams) const = 0;
				virtual std::unique_ptr<IDynTexture>	CreateDynamicRenderTarget(const SRTCreationParams &renderTargetParams) const = 0;

				virtual void										CreateRendererInstance(const char* name, ICustomRendererImplementation* pImpl, size_t maxConstantSize = 0, uint32 customID = ~0u) = 0;
				virtual ICustomRendererImplementation*				GetRendererImplementation(uint32 id) = 0;

				virtual int					RT_RegisterConstantName(const char* name) = 0;
				//virtual void				RT_UnregisterConstantName(int idx) = 0;

				virtual InputLayoutHandle	RT_RegisterLayout(TArray<SInputElementDescription> &layoutDesc) = 0;
				virtual void				RT_RegisterSamplers() = 0;

				virtual uintptr_t			RT_CreateConstantBuffer(size_t sizeInBytes) = 0;
				virtual void				RT_FreeConstantBuffer(uintptr_t buffer) = 0;

				virtual uintptr_t			RT_CreateOrUpdateBuffer(const SBufferParams &params, uintptr_t bufferHandle = INVALID_BUFFER) = 0;
				virtual void				RT_FreeBuffer(uintptr_t handle) = 0;
				
				virtual void				RT_ClearRenderTarget(ITexture* pTarget, ColorF clearColor) const = 0;
			protected:
			};

			
		}
	}

}


