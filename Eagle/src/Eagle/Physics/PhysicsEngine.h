#pragma once

#include "PhysicsSettings.h"

namespace Eagle
{
	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static const PhysicsSettings& GetSettings() { return s_Settings; }

	private:
		static PhysicsSettings s_Settings;
	};
}
