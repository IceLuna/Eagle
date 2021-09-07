#pragma once

namespace Eagle
{
	class PhysicsMaterial
	{
	public:
		float StaticFriction;
		float DynamicFriction;
		float Bounciness;

		PhysicsMaterial() = default;
		PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
		: StaticFriction(staticFriction)
		, DynamicFriction(dynamicFriction)
		, Bounciness(bounciness)
		{}

	};
}
