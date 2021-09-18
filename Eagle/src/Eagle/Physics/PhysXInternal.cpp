#include "egpch.h"
#include "PhysXInternal.h"
#include "PhysXDebugger.h"
#include "PhysXCookingFactory.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	struct PhysXData
	{
		physx::PxFoundation* Foundation;
		physx::PxPhysics* Physics;
		physx::PxDefaultCpuDispatcher* CPUDispatcher;

		physx::PxDefaultAllocator Allocator;
		PhysicsErrorCallback ErrorCallback;
		PhysicsAssertHandler AssertHandler;
	};

	static PhysXData* s_PhysXData = nullptr;

	void PhysXInternal::Init()
	{
		EG_CORE_ASSERT(!s_PhysXData, "Trying to init physics twice!");

		s_PhysXData = new PhysXData();
		s_PhysXData->Foundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_PhysXData->Allocator, s_PhysXData->ErrorCallback);
		EG_CORE_ASSERT(s_PhysXData->Foundation, "Failed to create foundation");

		physx::PxTolerancesScale scale = physx::PxTolerancesScale();
		scale.length = 1.f;
		scale.speed = 9.81f;

		PhysXDebugger::Init();

		#ifdef EG_DEBUG
			bool bTrackMemoryAllocs = true;
		#else
			bool bTrackMemoryAllocs = false;
		#endif

		s_PhysXData->Physics = PxCreatePhysics(PX_PHYSICS_VERSION, *s_PhysXData->Foundation, scale, bTrackMemoryAllocs, PhysXDebugger::GetDebugger());
		EG_CORE_ASSERT(s_PhysXData->Physics, "Failed to create Physics");

		bool bExtensionsLoaded = PxInitExtensions(*s_PhysXData->Physics, PhysXDebugger::GetDebugger());
		EG_CORE_ASSERT(bExtensionsLoaded, "Failed to init Extensions");

		s_PhysXData->CPUDispatcher = physx::PxDefaultCpuDispatcherCreate(1);
		
		PhysXCookingFactory::Init();

		PxSetAssertHandler(s_PhysXData->AssertHandler);
	}

	void PhysXInternal::Shutdown()
	{
		PhysXCookingFactory::Shutdown();

		s_PhysXData->CPUDispatcher->release();
		s_PhysXData->CPUDispatcher = nullptr;

		PxCloseExtensions();

		PhysXDebugger::StopDebugging();
		PhysXDebugger::Shutdown();

		s_PhysXData->Physics->release();
		s_PhysXData->Physics = nullptr;

		s_PhysXData->Foundation->release();
		s_PhysXData->Foundation = nullptr;

		delete s_PhysXData;
		s_PhysXData = nullptr;
	}

	physx::PxFoundation& PhysXInternal::GetFoundation()
	{
		return *s_PhysXData->Foundation;
	}

	physx::PxPhysics& PhysXInternal::GetPhysics()
	{
		return *s_PhysXData->Physics;
	}

	physx::PxDefaultCpuDispatcher* PhysXInternal::GetCPUDispatcher()
	{
		return s_PhysXData->CPUDispatcher;
	}

	physx::PxFilterFlags PhysXInternal::FilterShader(physx::PxFilterObjectAttributes attrs0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attrs1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
	{
		if (physx::PxFilterObjectIsTrigger(attrs0) || physx::PxFilterObjectIsTrigger(attrs1))
		{
			pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
			return physx::PxFilterFlag::eDEFAULT;
		}

		pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

		if (filterData0.word2 == (uint32_t)RigidBodyComponent::CollisionDetectionType::Continuous ||
			filterData1.word2 == (uint32_t)RigidBodyComponent::CollisionDetectionType::Continuous)
		{
			pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
			pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
		}

		if ((filterData0.word0 & filterData1.word1) || (filterData1.word0 & filterData0.word1))
		{
			pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
			pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
			return physx::PxFilterFlag::eDEFAULT;
		}

		return physx::PxFilterFlag::eSUPPRESS;
	}
	
	void PhysicsErrorCallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		const char* errorMessage = nullptr;

		switch (code)
		{
			case physx::PxErrorCode::Enum::eNO_ERROR:			errorMessage = "No Error"; break;
			case physx::PxErrorCode::Enum::eDEBUG_INFO:			errorMessage = "Info"; break;
			case physx::PxErrorCode::Enum::eDEBUG_WARNING:		errorMessage = "Warning"; break;
			case physx::PxErrorCode::Enum::eINVALID_PARAMETER:	errorMessage = "Invalid Parameter"; break;
			case physx::PxErrorCode::Enum::eINVALID_OPERATION:	errorMessage = "Invalid Pperation"; break;
			case physx::PxErrorCode::Enum::eOUT_OF_MEMORY:		errorMessage = "Out of Memory"; break;
			case physx::PxErrorCode::Enum::eINTERNAL_ERROR:		errorMessage = "Internal Error"; break;
			case physx::PxErrorCode::Enum::eABORT:				errorMessage = "Abort"; break;
			case physx::PxErrorCode::Enum::ePERF_WARNING:		errorMessage = "Performance Warning"; break;
			case physx::PxErrorCode::Enum::eMASK_ALL:			errorMessage = "Unknown Error"; break;
		}

		switch (code)
		{
			case physx::PxErrorCode::Enum::eNO_ERROR:
			case physx::PxErrorCode::Enum::eDEBUG_INFO:
				EG_CORE_INFO("[Physics Engine]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
				break;
			case physx::PxErrorCode::Enum::eDEBUG_WARNING:
			case physx::PxErrorCode::Enum::ePERF_WARNING:
				EG_CORE_WARN("[Physics Engine]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
				break;
			case physx::PxErrorCode::Enum::eINVALID_PARAMETER:
			case physx::PxErrorCode::Enum::eINVALID_OPERATION:
			case physx::PxErrorCode::Enum::eOUT_OF_MEMORY:
			case physx::PxErrorCode::Enum::eINTERNAL_ERROR:
				EG_CORE_ERROR("[Physics Engine]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
				break;
			case physx::PxErrorCode::Enum::eABORT:
			case physx::PxErrorCode::Enum::eMASK_ALL:
				EG_CORE_CRITICAL("[Physics Engine]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
				EG_CORE_ASSERT(false, "Critical error");
				break;
		}
	}
	
	void PhysicsAssertHandler::operator()(const char* exception, const char* file, int line, bool& ignore)
	{
		EG_CORE_ERROR("[Physics Engine]: {0}: at {1} ({2})", exception, file, line);
	}
}
