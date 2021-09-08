#pragma once
#include <PhysX/PxPhysicsAPI.h>

namespace Eagle
{
	class PhysicsErrorCallback : public physx::PxDefaultErrorCallback
	{
	public:
		virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override;
	};

	class PhysicsAssertHandler : public physx::PxAssertHandler
	{
	public:
		virtual void operator()(const char* exception, const char* file, int line, bool& ignore);
	};

	class PhysXInternal
	{
	public:
		static void Init();
		static void Shutdown();

		static physx::PxFoundation& GetFoundation();
		static physx::PxPhysics& GetPhysics();
		static physx::PxDefaultCpuDispatcher* GetCPUDispatcher();
		static physx::PxFilterFlags FilterShader(physx::PxFilterObjectAttributes attrs0, physx::PxFilterData filterData0,
												 physx::PxFilterObjectAttributes attrs1, physx::PxFilterData filterData1,
												 physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize);
	};
}
