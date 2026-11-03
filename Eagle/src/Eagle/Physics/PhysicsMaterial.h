#pragma once

namespace Eagle
{
	struct PhysicsMaterialSettings
	{
		float StaticFriction = 0.6f;
		float DynamicFriction = 0.6f;
		float Bounciness = 0.f; // (-inf; 1]

		bool operator==(const PhysicsMaterialSettings& other) const = default;
	};

	class PhysicsMaterial
	{
	public:
		const PhysicsMaterialSettings& GetSettings() const { return m_Settings; }
		void SetSettings(const PhysicsMaterialSettings& settings);

		void SetStaticFriction(float value);
		void SetDynamicFriction(float value);
		void SetBounciness(float value);

		float GetStaticFriction() const { return m_Settings.StaticFriction; }
		float GetDynamicFriction() const { return m_Settings.DynamicFriction; }
		float GetBounciness() const { return m_Settings.Bounciness; }
		void* GetNativeHandle() const { return m_NativeHandle; }

		static Ref<PhysicsMaterial> Create(float staticFriction = 0.6f, float dynamicFriction = 0.6f, float bounciness = 0.0f);

	protected:
		PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness);
		virtual ~PhysicsMaterial();

	private:
		PhysicsMaterialSettings m_Settings;
		void* m_NativeHandle = nullptr;
	};
}
