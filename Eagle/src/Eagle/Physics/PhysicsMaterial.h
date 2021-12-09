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
		PhysicsMaterial(const PhysicsMaterial&) = default;

		PhysicsMaterial(const Ref<PhysicsMaterial>& other)
		: StaticFriction(other->StaticFriction)
		, DynamicFriction(other->DynamicFriction)
		, Bounciness(other->Bounciness) {}

	};
}
