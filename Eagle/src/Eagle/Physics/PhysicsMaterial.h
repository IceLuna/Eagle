#pragma once

namespace Eagle
{
	class PhysicsMaterial
	{
	public:
		float StaticFriction = 0.6f;
		float DynamicFriction = 0.6f;
		float Bounciness = 0.5f;

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

		static const Ref<PhysicsMaterial> Default;
	};
}
