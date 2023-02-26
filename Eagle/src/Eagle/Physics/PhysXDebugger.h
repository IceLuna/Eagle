#pragma once

#include <PhysX/PxPhysicsAPI.h>
#include <filesystem>

namespace Eagle
{
	class PhysXDebugger
	{
	public:
		static void Init();
		static void Shutdown();

		static void StartDebugging(const Path& filepath, bool networkDebugging = false);
		static bool IsDebugging();
		static void StopDebugging();

		static physx::PxPvd* GetDebugger();
	};
}
