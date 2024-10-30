#include "egpch.h"
#include "PhysicsMaterial.h"

#include "PhysXInternal.h"

namespace Eagle
{
	PhysicsMaterial::PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
		: m_StaticFriction(staticFriction), m_DynamicFriction(dynamicFriction), m_Bounciness(bounciness)
	{
		m_NativeHandle = PhysXInternal::GetPhysics().createMaterial(m_StaticFriction, m_DynamicFriction, m_Bounciness);
	}

	PhysicsMaterial::~PhysicsMaterial()
	{
		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->release();
		m_NativeHandle = nullptr;
	}

	void PhysicsMaterial::SetStaticFriction(float value)
	{
		m_StaticFriction = value;

		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->setStaticFriction(m_StaticFriction);
	}

	void PhysicsMaterial::SetDynamicFriction(float value)
	{
		m_DynamicFriction = value;

		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->setDynamicFriction(m_DynamicFriction);
	}

	void PhysicsMaterial::SetBounciness(float value)
	{
		m_Bounciness = glm::clamp(value, 0.f, 1.f);

		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->setRestitution(m_Bounciness);
	}
	
	Ref<PhysicsMaterial> PhysicsMaterial::Create(float staticFriction, float dynamicFriction, float bounciness)
	{
		class LocalPhysicsMaterial : public PhysicsMaterial
		{
		public:
			LocalPhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
				: PhysicsMaterial(staticFriction, dynamicFriction, bounciness) {}
		};

		return MakeRef<LocalPhysicsMaterial>(staticFriction, dynamicFriction, bounciness);
	}
}
