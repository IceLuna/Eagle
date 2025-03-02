#pragma once

namespace Eagle
{
	class PhysicsMaterial
	{
	public:
		void SetStaticFriction(float value);
		void SetDynamicFriction(float value);
		void SetBounciness(float value);

		float GetStaticFriction() const { return m_StaticFriction; }
		float GetDynamicFriction() const { return m_DynamicFriction; }
		float GetBounciness() const { return m_Bounciness; }
		void* GetNativeHandle() const { return m_NativeHandle; }

		static Ref<PhysicsMaterial> Create(float staticFriction = 0.6f, float dynamicFriction = 0.6f, float bounciness = 0.0f);

	protected:
		PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness);
		virtual ~PhysicsMaterial();

	private:
		float m_StaticFriction = 0.6f;
		float m_DynamicFriction = 0.6f;
		float m_Bounciness = 0.5f;
		void* m_NativeHandle = nullptr;
	};
}
