#include "egpch.h"
#include "PhysicsMaterial.h"

#include "PhysXInternal.h"

namespace Eagle
{
	PhysicsMaterial::PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
		: m_Settings{ staticFriction, dynamicFriction, bounciness }
	{
		m_NativeHandle = PhysXInternal::GetPhysics().createMaterial(m_Settings.StaticFriction, m_Settings.DynamicFriction, m_Settings.Bounciness);
	}

	PhysicsMaterial::~PhysicsMaterial()
	{
		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->release();
		m_NativeHandle = nullptr;
	}

	void PhysicsMaterial::SetSettings(const PhysicsMaterialSettings& settings)
	{
		m_Settings = settings;
		m_Settings.Bounciness = glm::min(settings.Bounciness, 1.f);

		physx::PxMaterial* material = (physx::PxMaterial*)m_NativeHandle;
		material->setStaticFriction(m_Settings.StaticFriction);
		material->setDynamicFriction(m_Settings.DynamicFriction);
		material->setRestitution(m_Settings.Bounciness);
	}

	void PhysicsMaterial::SetStaticFriction(float value)
	{
		PhysicsMaterialSettings settings = m_Settings;
		settings.StaticFriction = value;
		SetSettings(settings);
	}

	void PhysicsMaterial::SetDynamicFriction(float value)
	{
		PhysicsMaterialSettings settings = m_Settings;
		settings.DynamicFriction = value;
		SetSettings(settings);
	}

	void PhysicsMaterial::SetBounciness(float value)
	{
		PhysicsMaterialSettings settings = m_Settings;
		settings.Bounciness = value;
		SetSettings(settings);
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
