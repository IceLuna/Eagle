#include "egpch.h"
#include "PSAutoDestroyScript.h"

#include "Eagle/Components/Components.h"

namespace Eagle
{
	PSAutoDestroyScript::PSAutoDestroyScript(PSAutoDestroyScript&& other) noexcept
	{
		if (other.m_ParticleAsset)
			other.m_ParticleAsset->RemoveOnAssetModifiedCallback(m_CallbackID);

		m_ParticleAsset = std::move(other.m_ParticleAsset);
		m_CallbackID = std::move(other.m_CallbackID);
		m_Lifetime = std::move(other.m_Lifetime);
		m_Timer = std::move(other.m_Timer);

		if (m_ParticleAsset)
		{
			m_ParticleAsset->AddOnAssetModifiedCallback(m_CallbackID, [this]()
			{
				RecalculateLifetime();
			});
		}
	}

	PSAutoDestroyScript& PSAutoDestroyScript::operator=(PSAutoDestroyScript&& other) noexcept
	{
		if (this == &other)
			return *this;

		if (other.m_ParticleAsset)
			other.m_ParticleAsset->RemoveOnAssetModifiedCallback(m_CallbackID);

		m_ParticleAsset = std::move(other.m_ParticleAsset);
		m_CallbackID = std::move(other.m_CallbackID);
		m_Lifetime = std::move(other.m_Lifetime);
		m_Timer = std::move(other.m_Timer);

		if (m_ParticleAsset)
		{
			m_ParticleAsset->AddOnAssetModifiedCallback(m_CallbackID, [this]()
			{
				RecalculateLifetime();
			});
		}

		return *this;
	}

	void PSAutoDestroyScript::OnCreate()
	{
		OnAssetChanged();
	}

	void PSAutoDestroyScript::OnUpdate(Timestep ts)
	{
		if (m_Timer >= m_Lifetime)
		{
			Parent.GetScene()->DestroyEntity(Parent);
			return;
		}

		m_Timer += ts;
	}

	void PSAutoDestroyScript::OnDestroy()
	{
		if (m_ParticleAsset)
			m_ParticleAsset->RemoveOnAssetModifiedCallback(m_CallbackID);

		m_ParticleAsset.reset();
	}
	
	void PSAutoDestroyScript::OnAssetChanged()
	{
		if (m_ParticleAsset)
			m_ParticleAsset->RemoveOnAssetModifiedCallback(m_CallbackID);
		m_ParticleAsset.reset();

		if (!Parent.HasComponent<ParticleSystemComponent>())
		{
			m_Lifetime = 0.f;
			return;
		}

		const auto& ps = Parent.GetComponent<ParticleSystemComponent>();
		m_ParticleAsset = ps.GetAsset();
		if (!m_ParticleAsset)
		{
			m_Lifetime = 0.f;
			return;
		}

		RecalculateLifetime();
		m_ParticleAsset->AddOnAssetModifiedCallback(m_CallbackID, [this]()
		{
			RecalculateLifetime();
		});
	}

	void PSAutoDestroyScript::RecalculateLifetime()
	{
		// Recalculate timer
		const auto& emitters = Cast<AssetParticleSystem>(m_ParticleAsset)->GetEmitters();
		m_Timer = 0.f;
		m_Lifetime = 0.f;

		for (const auto& emitter : emitters)
		{
			if (emitter.LoopCount == 0)
			{
				EG_CORE_WARN("[ScriptEngine] Particle system won't be auto-destroyed since one of its emitters has infinite loop count! {}", m_ParticleAsset->GetPath().u8string());
				m_Lifetime = FLT_MAX;
				break;
			}

			const float currentLifetime = emitter.LoopCount * emitter.LifetimeMax;
			m_Lifetime = glm::max(currentLifetime, m_Lifetime);
		}
	}
}
