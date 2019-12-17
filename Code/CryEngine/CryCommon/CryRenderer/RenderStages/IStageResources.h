#pragma once

namespace Cry
{
	namespace Renderer
	{
		namespace StageResources
		{
			#define INVALID_BUFFER ~0u
		}

		struct IStageResourceProvider
		{
			virtual _smart_ptr<ITexture>	CreateRenderTarget(const SRTCreationParams& renderTargetParams) const = 0;
			virtual std::unique_ptr<IDynTexture>	CreateDynamicRenderTarget(const SRTCreationParams& renderTargetParams) const = 0;

			virtual int						RegisterConstantName(const char* name) = 0;

			virtual InputLayoutHandle		RegisterLayout(const SInputElementDescription* pDescriptions, size_t count) = 0;
			virtual InputLayoutHandle		RegisterLayout(TArray<SInputElementDescription>& layoutDesc) = 0;
			virtual void					RegisterSamplers() = 0;

			virtual uintptr_t				CreateConstantBuffer(size_t sizeInBytes) = 0;
			virtual void					FreeConstantBuffer(uintptr_t buffer) = 0;

			virtual uintptr_t				CreateOrUpdateBuffer(const SBufferParams& params, uintptr_t bufferHandle = INVALID_BUFFER) = 0;
			virtual void					FreeBuffer(uintptr_t bufferHandle) = 0;;

			virtual void* BufferBeginWrite(uintptr_t handle) = 0;
			virtual void					BufferEndWrite(uintptr_t handle) = 0;
		};
	}
}