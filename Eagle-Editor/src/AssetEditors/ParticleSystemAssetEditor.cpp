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
		const auto& scene = GetCurrentScene();

		m_Entity = scene->CreateEntity("ParticleSystemAssetEditor");
		m_Entity.AddComponent<ParticleSystemComponent>().SetAsset(asset);

		m_Emitters = m_Asset->GetEmitters();

		auto& camera = scene->GetEditorCamera();
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
		camera.SetLocation(center - cameraDir * length * 2.5f); // Move back
		camera.LookAt(center);
		RecalculateLifetime();

		m_WindowName = AssetEditor::GetAssetWindowName(m_Asset);
	}

	void ParticleSystemAssetEditor::OnImGuiRender(bool* pOpen)
	{
		constexpr ImGuiTreeNodeFlags defaultTreeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
		bool bChanged = false;

		ImGui::SetNextWindowSize(AssetEditor::GetDefaultWindowSize(), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		UI::BeginPropertyGrid("ParticleSystemAssetEditor");
		UI::Text("Name", m_Asset->GetPath().stem().string());
		UI::Text("Type", "Particle System");

		if (UI::Button("Emitters", "Add"))
		{
			m_Emitters.emplace_back();
			m_SelectedEmitterIndex = m_Emitters.size() - 1u;
			bChanged = true;
		}

		UI::EndPropertyGrid();

		ImGui::Separator();
		size_t emitterIndexToDelete = s_InvalidIndex;
		const size_t emittersCount = m_Emitters.size();
		for (size_t i = 0; i < m_Emitters.size(); ++i)
		{
			auto& emitter = m_Emitters[i];
			const ParticleEmitter* selectedEmitter = m_SelectedEmitterIndex != s_InvalidIndex ? &m_Emitters[m_SelectedEmitterIndex] : nullptr;
			const ImGuiTreeNodeFlags flags = defaultTreeFlags | (&emitter == selectedEmitter ? ImGuiTreeNodeFlags_Selected : 0);
			const void* hash = (void*)emitter.ID.GetHash();
			const bool opened = ImGui::TreeNodeEx(hash, flags, emitter.Name.c_str());

			if (selectedEmitter)
			{
				GetCurrentScene()->DrawAABB(selectedEmitter->VisibilityAABB, selectedEmitter->RelativeTransform);
			}

			if (ImGui::IsItemClicked())
			{
				if (m_SelectedEmitterIndex == i)
					m_SelectedEmitterIndex = s_InvalidIndex; // Remove selection
				else
					m_SelectedEmitterIndex = i;
			}

			if (ImGui::BeginPopupContextItem(nullptr))
			{
				if (ImGui::MenuItem("Delete"))
				{
					if (selectedEmitter == &emitter)
					{
						m_SelectedEmitterIndex = s_InvalidIndex;
					}
					emitterIndexToDelete = i;
					bChanged = true;
				}

				ImGui::EndPopup();
			}

			if (!opened)
				continue;

			bool bEmitterChanged = false;

			ImGui::PushID(hash);

			UI::BeginPropertyGrid("ParticleSystemAssetEditor");
			bEmitterChanged |= UI::PropertyText("Name", emitter.Name);
			UI::EndPropertyGrid();
			ImGui::Separator();

			if (ImGui::TreeNodeEx("Transform", defaultTreeFlags | ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto& transform = emitter.RelativeTransform;
				glm::quat quat = transform.Rotation.GetQuat();
				bool bTransformChanged = false;

				bTransformChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
				if (UI::DrawQuatControl("Rotation (Quat)", quat))
				{
					transform.Rotation = quat;
					bTransformChanged = true;
				}
				bTransformChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });

				bEmitterChanged |= bTransformChanged;

				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Basic", defaultTreeFlags | ImGuiTreeNodeFlags_DefaultOpen))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::PropertyDrag("Visibility AABB Min", emitter.VisibilityAABB.Min, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered. For optimization reasons, keep AABB as small as possible");
				bEmitterChanged |= UI::PropertyDrag("Visibility AABB Max", emitter.VisibilityAABB.Max, 0.1f, 0, 0, "If AABB is not visible by the camera, the particle system is not rendered. For optimization reasons, keep AABB as small as possible");

				bEmitterChanged |= EditorResources::DrawAssetSelection("Texture", emitter.Texture);
				bEmitterChanged |= UI::PropertyDrag("Loop Count", emitter.LoopCount, 1.f, 0, 0, "0 will loop forever");
				bEmitterChanged |= UI::PropertyDrag("Loop Duration", emitter.LoopDuration, 0.1f);
				bEmitterChanged |= UI::PropertyDrag("Spawn Rate", emitter.SpawnRate, 1, 0, 0, "How many particles to spawn in a second. If `Explode` flag is set, this amount of particles will be spawned immediately.");

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Animation", defaultTreeFlags))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::PropertyDrag("Hor. frames number", emitter.AnimationImagesNum.x, 1.f, 1u, UINT_MAX, "The number of columns in the sprite sheet (texture)");
				bEmitterChanged |= UI::PropertyDrag("Ver. frames number", emitter.AnimationImagesNum.y, 1.f, 1u, UINT_MAX, "The number of rows in the sprite sheet (texture)");
				bEmitterChanged |= UI::PropertyDrag("Animation Speed", emitter.AnimationSpeed, 0.05f);
				bEmitterChanged |= UI::Property("Blend Animation", emitter.bBlendAnimation);
				
				UI::TextWithSeparator("Mesh Animation Settings");
				{
					bEmitterChanged |= EditorResources::DrawAssetSelection("Mesh Animation Clip", emitter.MeshAnimationAsset, "Used only with skeletal meshes");
					bEmitterChanged |= UI::PropertyDrag("Playback Speed", emitter.ClipPlaybackSpeed, 0.1f);
					bEmitterChanged |= UI::Property("Is Looping", emitter.bClipLooping);
				}

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			// TODO: it's currently not supported
			//if (UI::PropertyDrag("Fast forward to", emitter.FastForwardTo, 0.1f, 0, 0, "Allows to fast-forward the simulation to make it look like it was running for `Fast forward to` seconds"))
			//{
			//	emitter.FastForwardTo = std::max(0.f, emitter.FastForwardTo);
			//	bEmitterChanged = true;
			//}

			if (ImGui::TreeNodeEx("Acceleration", defaultTreeFlags))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::PropertyDrag("Radial Acceleration", emitter.RadialAcceleration, 0.1f, 0, 0, "If it's negative, particles will move towards the center of the emitter. If positive, they'll move away from the center");
				bEmitterChanged |= UI::PropertyDrag("Tangential Acceleration", emitter.TangentialAcceleration, 0.1f, 0, 0, "Particles will move away from the center of the emitter in a spiral way");
				bEmitterChanged |= UI::PropertyDrag("Normal Velocity Factor", emitter.NormalVelocityFactor, 0.1f, 0, 0, "If not 0, particle's initial velocity will be affected by `EmissionShapeType` normal direction.\nOnly supported for Sphere and Mesh shapes!");

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Modes", defaultTreeFlags))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::ComboEnum("Collision Mode", emitter.CollisionMode, "It's a screen space collision detection");
				bEmitterChanged |= UI::ComboEnum("Emission Shape", emitter.EmissionShape);
				bEmitterChanged |= UI::PropertyDrag("Sphere Radius", emitter.SphereRadius, 0.05f, 0, 0);
				bEmitterChanged |= UI::PropertyDrag("Box Min", emitter.BoxMin, 0.05f, 0, 0);
				bEmitterChanged |= UI::PropertyDrag("Box Max", emitter.BoxMax, 0.05f, 0, 0);
				bEmitterChanged |= UI::PropertyDrag("Ring Radius", emitter.RingRadius, 0.05f, 0, 0);
				bEmitterChanged |= UI::PropertyDrag("Ring Thickness", emitter.RingThickness, 0.05f, 0, 0);
				bEmitterChanged |= EditorResources::DrawAssetSelection("Mesh", emitter.MeshAsset);

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Flags", defaultTreeFlags))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::Property("Destroy Immediately", emitter.bDestroyImmediately, "If set to true, particles will be disabled/destroyed immediately when emitter is destroyed (instead of following their lifetime)");
				bEmitterChanged |= UI::Property("Emit", emitter.bEmit);
				bEmitterChanged |= UI::Property("Explode", emitter.bExplode, "If set to true, all particles will be emitted at once. Otherwise, they're emitted sequentially throughout the lifetime");
				bEmitterChanged |= UI::Property("Apply Gravity", emitter.bApplyGravity);
				bEmitterChanged |= UI::Property("Face Direction", emitter.bFaceDirection, "When set to true, particles will face the velocity direction");
				bEmitterChanged |= UI::Property("Alpha Blending", emitter.bAlphaBlending);
				if (!emitter.bAlphaBlending)
					UI::PushItemDisabled();
				bEmitterChanged |= UI::Property("Additive Blending", emitter.bAdditive);
				if (!emitter.bAlphaBlending)
					UI::PopItemDisabled();

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Particle settings", defaultTreeFlags))
			{
				UI::BeginPropertyGrid("ParticleSystemAssetEditor");
				bEmitterChanged |= UI::PropertyColor("Color Start", emitter.ColorStart, true);
				bEmitterChanged |= UI::PropertyColor("Color End", emitter.ColorEnd, true);

				bEmitterChanged |= UI::PropertyDrag("Velocity Min", emitter.VelocityMin, 0.05f);
				bEmitterChanged |= UI::PropertyDrag("Velocity Max", emitter.VelocityMax, 0.05f);

				bEmitterChanged |= UI::PropertyDrag("Velocity Coef Start", emitter.VelocityCoefStart, 0.05f, 0, 0, "Can be used to change the velocity of a particle throughout the lifetime");
				bEmitterChanged |= UI::PropertyDrag("Velocity Coef End", emitter.VelocityCoefEnd, 0.05f, 0, 0, "Can be used to change the velocity of a particle throughout the lifetime");

				bEmitterChanged |= UI::PropertyDrag("Rotation Z Start", emitter.RotationZStart, 1.f);
				bEmitterChanged |= UI::PropertyDrag("Rotation Z End", emitter.RotationZEnd, 1.f);

				bEmitterChanged |= UI::PropertyDrag("Size Start", emitter.SizeStart, 0.05f);
				bEmitterChanged |= UI::PropertyDrag("Size End", emitter.SizeEnd, 0.05f);

				bEmitterChanged |= UI::PropertyDrag("Collider Size Ratio", emitter.ColliderSizeRatio, 0.05f, 0, 0, "Can be used to increase the size of a collider to prevent small and fast-moving particles from clipping through");

				if (UI::PropertyDrag("Lifetime Min", emitter.LifetimeMin, 0.1f, 0.f, FLT_MAX, "In seconds"))
				{
					emitter.LifetimeMin = glm::max(0.f, emitter.LifetimeMin);
					bEmitterChanged = true;
				}

				if (UI::PropertyDrag("Lifetime Max", emitter.LifetimeMax, 0.1f, 0.f, FLT_MAX, "In seconds"))
				{
					emitter.LifetimeMax = glm::max(0.f, emitter.LifetimeMax);
					bEmitterChanged = true;
				}

				bEmitterChanged |= UI::PropertyDrag("Bounciness Min", emitter.BouncinessMin, 0.1f);
				bEmitterChanged |= UI::PropertyDrag("Bounciness Max", emitter.BouncinessMax, 0.1f);

				UI::EndPropertyGrid();
				ImGui::TreePop();
			}

			if (bEmitterChanged)
			{
				m_SelectedEmitterIndex = i; // Select currently modifying emitter
				bChanged = true;
			}

			ImGui::PopID();
			ImGui::Separator();
			if (opened)
				ImGui::TreePop();
		}

		if (emitterIndexToDelete != s_InvalidIndex)
		{
			auto it = m_Emitters.begin();
			std::advance(it, emitterIndexToDelete);
			m_Emitters.erase(it);

			if (m_SelectedEmitterIndex != s_InvalidIndex)
			{
				if (m_SelectedEmitterIndex == emitterIndexToDelete)
				{
					m_SelectedEmitterIndex = s_InvalidIndex;
				}
				else if (m_SelectedEmitterIndex > emitterIndexToDelete)
				{
					--m_SelectedEmitterIndex;
				}
			}

			emitterIndexToDelete = s_InvalidIndex;
		}

		bChanged |= bGuizmoChanged;
		bGuizmoChanged = false;

		if (bChanged)
		{
			m_Asset->SetEmitters(m_Emitters);
			RecalculateLifetime();
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		if (m_Timer.GetSeconds() >= m_Lifetime)
		{
			m_Timer.Restart();
			auto& ps = m_Entity.GetComponent<ParticleSystemComponent>();
			ps.Destroy();
			ps.Spawn();
		}

		ImGui::Separator();
		ImGui::Separator();
		if (ImGui::Button("Save asset"))
			Asset::Save(m_Asset);

		ImGui::End();

		DrawViewport(false, m_WindowName);
	}

	void ParticleSystemAssetEditor::RecalculateLifetime()
	{
		m_Lifetime = m_Emitters.empty() ? FLT_MAX : 0.f;
		for (const auto& emitter : m_Emitters)
		{
			if (emitter.LoopCount == 0)
			{
				m_Lifetime = FLT_MAX;
				break;
			}

			const float currentLifetime = emitter.LoopCount * emitter.LifetimeMax;
			m_Lifetime = glm::max(currentLifetime, m_Lifetime);
		}

		m_Timer.Restart();
	}
	
	void ParticleSystemAssetEditor::UpdateGuizmo()
	{
		if (m_SelectedEmitterIndex == s_InvalidIndex)
			return;

		auto& emitter = m_Emitters[m_SelectedEmitterIndex];
		bGuizmoChanged = DrawGuizmo(emitter.RelativeTransform, true, false);
	}
}
