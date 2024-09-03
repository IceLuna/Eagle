#include "egpch.h"
#include "ParticleSystemAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Components/Components.h"

namespace Eagle
{
	ParticleSystemAssetEditor::ParticleSystemAssetEditor(const Ref<AssetParticleSystem>& asset)
		: AssetEditor(true, false), m_Asset(asset)
	{
		Entity entity = m_Scene->CreateEntity("ParticleSystemAssetEditor");
		m_Component = &entity.AddComponent<ParticleSystemComponent>(m_Asset);

		auto& camera = m_Scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		glm::vec3 center = glm::vec3(0.f);
		float length = 1.f;
		if (m_Asset)
		{
			AABB aabb;
			const auto& emitters = m_Asset->GetEmitters();
			for (const auto& emitter : emitters)
				aabb.Grow(emitter.VisibilityAABB);
			center = aabb.Center();
			length = aabb.MaxSide();
		}
		camera.LookAt(center);
		camera.SetLocation(center - cameraDir * length * 2.5f); // Move back
	}

	void ParticleSystemAssetEditor::OnImGuiRender(bool* pOpen)
	{
		bool bChanged = false;

		ImGui::SetNextWindowSize(ImVec2(720.f, 560.f), ImGuiCond_FirstUseEver);
		bool bHidden = !ImGui::Begin(m_Asset->GetPath().u8string().c_str(), pOpen);

		UI::BeginPropertyGrid("ParticleSystemDetails");
		UI::Text("Name", m_Asset->GetPath().stem().u8string());
		UI::Text("Type", "Particle System");
		UI::EndPropertyGrid();

		auto emitters = m_Asset->GetEmitters();

		UI::BeginPropertyGrid("ParticleSystemAssetEditor");

		if (UI::Button("Emitter", "Add"))
		{
			emitters.emplace_back();
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		for (auto& emitter : emitters)
		{
			constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
			const void* hash = (void*)emitter.ID.GetHash();
			const bool opened = ImGui::TreeNodeEx(hash, flags, "Emitter");

			if (!opened)
				continue;

			UI::BeginPropertyGrid("ParticleSystemAssetEditor");
			ImGui::PushID(hash);

			bChanged |= UI::PropertyDrag("Relative Location", emitter.RelativeTransform.Location, 0.1f);
			bChanged |= UI::PropertyDrag("Visibility AABB Min", emitter.VisibilityAABB.Min, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered");
			bChanged |= UI::PropertyDrag("Visibility AABB Max", emitter.VisibilityAABB.Max, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered");

			bChanged |= UI::PropertyDrag("Particles Amount", emitter.NumParticles);
			if (UI::PropertyDrag("Particles Amount Ratio", emitter.NumParticlesRatio, 0.1f, 0, 0, "Can be used to control `Particles Amount`"))
			{
				emitter.NumParticlesRatio = std::max(0.f, emitter.NumParticlesRatio);
				bChanged = true;
			}

			bChanged |= UI::DrawAssetSelection("Texture", emitter.Texture);

			bChanged |= UI::PropertyDrag("Hor. frames number", emitter.AnimationImagesNum.x, 1.f, 1u, UINT_MAX, "The number of columns in the sprite sheet");
			bChanged |= UI::PropertyDrag("Ver. frames number", emitter.AnimationImagesNum.y, 1.f, 1u, UINT_MAX, "The number of rows in the sprite sheet");
			bChanged |= UI::PropertyDrag("Animation Speed", emitter.AnimationSpeed, 0.1f);

			if (UI::PropertyDrag("Fast forward to", emitter.FastForwardTo, 0.1f, 0, 0, "Allows to fast-forward the simulation to make it look like it was running for `Fast forward to` seconds"))
			{
				emitter.FastForwardTo = std::max(0.f, emitter.FastForwardTo);
				bChanged = true;
			}

			bChanged |= UI::PropertyDrag("Radial Acceleration", emitter.RadialAcceleration, 0.1f, 0, 0, "If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center");
			bChanged |= UI::PropertyDrag("Tangential Acceleration", emitter.TangentialAcceleration, 0.1f, 0, 0, "If it's negative, particles will move towards the center of the emitter in a spiral way. If positive, they move away from the center");

			bChanged |= UI::ComboEnum("Collision Mode", emitter.CollisionMode);
			bChanged |= UI::ComboEnum("Emission Shape", emitter.EmissionShape);
			bChanged |= UI::PropertyDrag("Sphere Radius", emitter.SphereRadius, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Box Min", emitter.BoxMin, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Box Max", emitter.BoxMax, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Ring Radius", emitter.RingRadius, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Ring Thickness", emitter.RingThickness, 0.1f, 0, 0);

			bChanged |= UI::Property("Emit", emitter.bEmit);
			bChanged |= UI::Property("OneShot", emitter.bOneShot);
			bChanged |= UI::Property("Explode", emitter.bExplode, "If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime");
			bChanged |= UI::Property("Apply Gravity", emitter.bApplyGravity);
			bChanged |= UI::Property("Alpha Blending", emitter.bAlphaBlending);

			UI::TextWithSeparator("Particle settings");

			bChanged |= UI::PropertyColor("Color Start", emitter.ColorStart, true);
			bChanged |= UI::PropertyColor("Color End", emitter.ColorEnd, true);

			bChanged |= UI::PropertyDrag("Velocity Min", emitter.VelocityMin, 0.25f);
			bChanged |= UI::PropertyDrag("Velocity Max", emitter.VelocityMax, 0.25f);

			bChanged |= UI::PropertyDrag("Velocity Coef Start", emitter.VelocityCoefStart, 0.25f, 0, 0, "Can be used to change the velocity of a particle throughout the lifetime");
			bChanged |= UI::PropertyDrag("Velocity Coef End", emitter.VelocityCoefEnd, 0.25f, 0, 0, "Can be used to change the velocity of a particle throughout the lifetime");

			bChanged |= UI::PropertyDrag("Rotation Z Start", emitter.RotationZStart, 1.f);
			bChanged |= UI::PropertyDrag("Rotation Z End", emitter.RotationZEnd, 1.f);

			bChanged |= UI::PropertyDrag("Size Start", emitter.SizeStart, 0.1f);
			bChanged |= UI::PropertyDrag("Size End", emitter.SizeEnd, 0.1f);

			bChanged |= UI::PropertyDrag("Collider Size Ratio", emitter.ColliderSizeRatio, 0.1f, 0, 0, "Can be used to increase the size of a collider to prevent small and fast-moving particles from clipping through");

			if (UI::PropertyDrag("Lifetime Min", emitter.LifetimeMin, 0.1f, 0.f, FLT_MAX, "In seconds"))
			{
				emitter.LifetimeMin = glm::max(0.f, emitter.LifetimeMin);
				if (emitter.LifetimeMin > emitter.LifetimeMax)
					emitter.LifetimeMax = emitter.LifetimeMin;
				bChanged = true;
			}

			if (UI::PropertyDrag("Lifetime Max", emitter.LifetimeMax, 0.1f, 0.f, FLT_MAX, "In seconds"))
			{
				emitter.LifetimeMax = glm::max(0.f, emitter.LifetimeMax);
				if (emitter.LifetimeMax < emitter.LifetimeMin)
					emitter.LifetimeMin = emitter.LifetimeMax;
				bChanged = true;
			}

			bChanged |= UI::PropertyDrag("Bounciness Min", emitter.BouncinessMin, 0.1f);
			bChanged |= UI::PropertyDrag("Bounciness Max", emitter.BouncinessMax, 0.1f);

			ImGui::PopID();
			UI::EndPropertyGrid();
			ImGui::TreePop();
		}

		if (bChanged)
		{
			m_Asset->SetEmitters(emitters);
			m_Scene->UpdateParticleSystem(m_Component);
		}

		if (bChanged)
			m_Asset->SetDirty(true);

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		DrawViewport();
	}
}
