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
		EG_CORE_ASSERT(m_Asset);
		Entity entity = m_Scene->CreateEntity("ParticleSystemAssetEditor");
		m_Component = &entity.AddComponent<ParticleSystemComponent>(m_Asset);
		m_Emitters = m_Asset->GetEmitters();

		auto& camera = m_Scene->GetEditorCamera();
		camera.SetLocation(glm::vec3(0.f, 5.f, 15.f));
		camera.LookAt(glm::vec3(0, 0, 0));
		const glm::vec3 cameraDir = camera.GetForwardVector();

		glm::vec3 center = glm::vec3(0.f);
		float length = 1.f;
		if (m_Emitters.size())
		{
			AABB aabb;
			for (const auto& emitter : m_Emitters)
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

		UI::BeginPropertyGrid("ParticleSystemAssetEditor");

		if (UI::Button("Emitter", "Add"))
		{
			m_Emitters.emplace_back();
			m_SelectedEmitter = &m_Emitters.back();
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		const ParticleEmitter* emitterToDelete = nullptr;
		for (auto& emitter : m_Emitters)
		{
			const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | (&emitter == m_SelectedEmitter ? ImGuiTreeNodeFlags_Selected : 0);
			const void* hash = (void*)emitter.ID.GetHash();
			const bool opened = ImGui::TreeNodeEx(hash, flags, "Emitter");

			if (ImGui::IsItemClicked())
			{
				m_SelectedEmitter = &emitter;
			}

			if (ImGui::BeginPopupContextItem(nullptr))
			{
				if (ImGui::MenuItem("Delete"))
				{
					if (m_SelectedEmitter == &emitter)
					{
						m_SelectedEmitter = nullptr;
					}
					emitterToDelete = &emitter;
					bChanged = true;
				}

				ImGui::EndPopup();
			}

			if (!opened && !bChanged)
				continue;

			ImGui::Separator();
			{
				auto& transform = emitter.RelativeTransform;
				const glm::quat q = transform.Rotation.GetQuat();
				glm::vec4 quat(q.x, q.y, q.z, q.w);
				bool bTransformChanged = false;

				bTransformChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				if (UI::DrawVec4Control("Rotation (Quat)", quat, glm::vec4{ 0, 0, 0, 1 }))
				{
					if (glm::all(glm::epsilonEqual(quat, glm::vec4(0), 0.001f)))
						quat.w = 1.f;
					quat = glm::normalize(quat);
					transform.Rotation = glm::quat(quat.w, quat.x, quat.y, quat.z);
					bTransformChanged = true;
				}
				bTransformChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });

				bChanged |= bTransformChanged;
			}
			ImGui::Separator();

			UI::BeginPropertyGrid("ParticleSystemAssetEditor");
			ImGui::PushID(hash);

			bChanged |= UI::PropertyDrag("Visibility AABB Min", emitter.VisibilityAABB.Min, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered");
			bChanged |= UI::PropertyDrag("Visibility AABB Max", emitter.VisibilityAABB.Max, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered");

			bChanged |= UI::PropertyDrag("Loop Count", emitter.LoopCount, 1.f, 0, 0, "0 will loop forever");
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

			// TODO: it's currently not supported
			//if (UI::PropertyDrag("Fast forward to", emitter.FastForwardTo, 0.1f, 0, 0, "Allows to fast-forward the simulation to make it look like it was running for `Fast forward to` seconds"))
			//{
			//	emitter.FastForwardTo = std::max(0.f, emitter.FastForwardTo);
			//	bChanged = true;
			//}

			bChanged |= UI::PropertyDrag("Radial Acceleration", emitter.RadialAcceleration, 0.1f, 0, 0, "If it's negative, particles will move towards the center of the emitter. If positive, they move away from the center");
			bChanged |= UI::PropertyDrag("Tangential Acceleration", emitter.TangentialAcceleration, 0.1f, 0, 0, "If it's negative, particles will move towards the center of the emitter in a spiral way. If positive, they move away from the center");
			bChanged |= UI::PropertyDrag("Normal Velocity Factor", emitter.NormalVelocityFactor, 0.1f, 0, 0, "If not 0, particle's initial velocity will be affected by `EmissionShapeType` normal direction.\nOnly supported for Sphere and Mesh shapes!");

			bChanged |= UI::ComboEnum("Collision Mode", emitter.CollisionMode, "It's a screen space collision detection");
			bChanged |= UI::ComboEnum("Emission Shape", emitter.EmissionShape);
			bChanged |= UI::PropertyDrag("Sphere Radius", emitter.SphereRadius, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Box Min", emitter.BoxMin, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Box Max", emitter.BoxMax, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Ring Radius", emitter.RingRadius, 0.1f, 0, 0);
			bChanged |= UI::PropertyDrag("Ring Thickness", emitter.RingThickness, 0.1f, 0, 0);
			bChanged |= UI::DrawAssetSelection("Mesh", emitter.MeshAsset);

			bChanged |= UI::Property("Emit", emitter.bEmit);
			bChanged |= UI::Property("Explode", emitter.bExplode, "If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime");
			bChanged |= UI::Property("Apply Gravity", emitter.bApplyGravity);
			bChanged |= UI::Property("Alpha Blending", emitter.bAlphaBlending);
			if (!emitter.bAlphaBlending)
				UI::PushItemDisabled();
			bChanged |= UI::Property("Additive Blending", emitter.bAdditive);
			if (!emitter.bAlphaBlending)
				UI::PopItemDisabled();

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
			if (opened)
				ImGui::TreePop();
		}

		if (emitterToDelete)
		{
			// Not the most efficient way, but hey, it's executed once in a lifetime
			for (size_t i = 0; i < m_Emitters.size(); ++i)
			{
				if (emitterToDelete == &m_Emitters[i])
				{
					auto it = m_Emitters.begin();
					std::advance(it, i);
					m_Emitters.erase(it);
					break;
				}
			}
			emitterToDelete = nullptr;
		}

		bChanged |= bGuizmoChanged;
		bGuizmoChanged = false;

		if (bChanged)
		{
			m_Asset->SetEmitters(m_Emitters);
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
	
	void ParticleSystemAssetEditor::UpdateGuizmo()
	{
		if (!m_SelectedEmitter)
			return;

		const int id = int(m_SelectedEmitter->ID.GetHash());
		bGuizmoChanged = DrawGuizmo(m_SelectedEmitter->RelativeTransform, id, true);
	}
}
