#pragma once

#include "PhysicsSettings.h"
#include "Eagle/Core/EnumUtils.h"

namespace Eagle
{
	enum class ForceMode
	{
		Force,
		Impulse,
		VelocityChange,
		Acceleration
	};

	enum class ActorLockFlag
	{
		None = 0,
		PositionX = BIT(0), PositionY = BIT(1), PositionZ = BIT(2), Position = PositionX | PositionY | PositionZ,
		RotationX = BIT(3), RotationY = BIT(4), RotationZ = BIT(5), Rotation = RotationX | RotationY | RotationZ
	};
	DECLARE_FLAGS(ActorLockFlag);

	class PhysicsEngine
	{
	public:
		static void Init();
		static void Shutdown();
	};
}
