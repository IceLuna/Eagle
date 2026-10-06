#include "egpch.h"
#include "ParticleSystemAssetEditor.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/UI/UI.h"
#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Components/Components.h"

#include "../Widgets/CurvePropertyUI.h"

namespace Eagle
{
	namespace
	{
		std::string FormatNumber(float value)
		{
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "%.2f", value);
			std::string result = buffer;
			// 1.50 -> 1.5, 2.00 -> 2
			while (!result.empty() && result.back() == '0')
				result.pop_back();
			if (!result.empty() && result.back() == '.')
				result.pop_back();
			return result;
		}

		const char* GetShapeName(ParticleEmitter::EmissionShapeType shape)
		{
			switch (shape)
			{
				case ParticleEmitter::EmissionShapeType::Point:         return "Point";
				case ParticleEmitter::EmissionShapeType::Sphere:        return "Sphere";
				case ParticleEmitter::EmissionShapeType::SphereSurface: return "Sphere Surface";
				case ParticleEmitter::EmissionShapeType::Box:           return "Box";
				case ParticleEmitter::EmissionShapeType::BoxSurface:    return "Box Surface";
				case ParticleEmitter::EmissionShapeType::Ring:          return "Ring";
				case ParticleEmitter::EmissionShapeType::Mesh:          return "Mesh";
				default:
					EG_CORE_ASSERT(false);
					return "<unknown>";
			}
			return "";
		}

		template <typename T>
		std::string CurveSummary(const CurveProperty<T>& property, const std::string& constantText)
		{
			return property.IsConstant() ? constantText : "Curve";
		}

		// Collapsible section with a short, dimmed summary on its header, so closed sections still tell what they do.
		bool BeginSection(const char* name, std::string_view summary, bool bDefaultOpen)
		{
			const bool bOpen = ImGui::CollapsingHeader(name, bDefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
			if (!summary.empty())
			{
				const ImVec2 min = ImGui::GetItemRectMin();
				const ImVec2 max = ImGui::GetItemRectMax();
				const ImVec2 textSize = ImGui::CalcTextSize(summary.data());
				const float x = max.x - textSize.x - ImGui::GetStyle().FramePadding.x * 2.f;
				const float labelEnd = min.x + ImGui::GetTreeNodeToLabelSpacing() + ImGui::CalcTextSize(name).x + 16.f;
				if (x > labelEnd)
					ImGui::GetWindowDrawList()->AddText(ImVec2(x, min.y + (max.y - min.y - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), summary.data());
			}

			if (bOpen)
				UI::BeginPropertyGrid(name);
			return bOpen;
		}

		void EndSection()
		{
			UI::EndPropertyGrid();
			ImGui::Spacing();
		}

		bool PropertyRandomRange(const char* label, glm::vec2& range, float speed, const char* minFormat, const char* maxFormat, std::string_view helpMessage)
		{
			ImGui::PushID(label);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
			ImGui::TextUnformatted(label);
			if (!helpMessage.empty())
			{
				ImGui::SameLine();
				UI::HelpMarker(helpMessage);
			}
			ImGui::NextColumn();

			ImGui::SetNextItemWidth(-1.f);
			const bool bChanged = ImGui::DragFloatRange2("##Range", &range.x, &range.y, speed, 0.f, 0.f, minFormat, maxFormat);
			if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
				ImGui::SetTooltip("Each particle picks a random value between Min and Max");

			ImGui::NextColumn();
			ImGui::PopID();

			return bChanged;
		}

		// A draggable bar between two panels. Returns the drag delta along its axis.
		float Splitter(const char* id, bool bVertical, float thickness, float length)
		{
			const ImVec2 size = bVertical ? ImVec2(thickness, length) : ImVec2(length, thickness);
			ImGui::InvisibleButton(id, size);
			const bool bActive = ImGui::IsItemActive();
			const bool bHovered = ImGui::IsItemHovered();
			if (bActive || bHovered)
				ImGui::SetMouseCursor(bVertical ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const ImVec2 min = ImGui::GetItemRectMin();
			const ImVec2 max = ImGui::GetItemRectMax();
			drawList->AddRectFilled(min, max, ImGui::GetColorU32(bActive ? ImGuiCol_SeparatorActive : (bHovered ? ImGuiCol_SeparatorHovered : ImGuiCol_WindowBg)));

			const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
			const ImU32 gripColor = ImGui::GetColorU32((bActive || bHovered) ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			for (int i = -1; i <= 1; ++i)
			{
				const ImVec2 dot = bVertical ? ImVec2(center.x, center.y + float(i) * 5.f) : ImVec2(center.x + float(i) * 5.f, center.y);
				drawList->AddCircleFilled(dot, 1.25f, gripColor);
			}

			if (!bActive)
				return 0.f;
			return bVertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;
		}
	}

	ParticleSystemAssetEditor::ParticleSystemAssetEditor(const Ref<AssetParticleSystem>& asset)
		: AssetEditor(true, false), m_Asset(asset)
	{
		m_FirstUseDockSize = ImVec2(1500.f, 900.f);
		m_FirstUseViewportRatio = 0.45f;

		EG_CORE_ASSERT(m_Asset);
		const auto& scene = GetCurrentScene();

		m_Entity = scene->CreateEntity("ParticleSystemAssetEditor");
		m_Entity.AddComponent<ParticleSystemComponent>().SetAsset(asset);

		m_Emitters = m_Asset->GetEmitters();
		if (!m_Emitters.empty())
			m_SelectedEmitterIndex = 0;

		auto& camera = scene->EditorCamera;
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
		ImGui::SetNextWindowSize(ImVec2(m_FirstUseDockSize.x * (1.f - m_FirstUseViewportRatio), m_FirstUseDockSize.y), ImGuiCond_FirstUseEver);
		ImGui::Begin(m_WindowName.c_str(), pOpen);

		DrawToolbar();
		ImGui::Separator();

		bool bChanged = false;
		const float contentHeight = ImGui::GetContentRegionAvail().y;

		// Left: emitters
		m_EmittersPanelWidth = glm::clamp(m_EmittersPanelWidth, 140.f, glm::max(140.f, ImGui::GetContentRegionAvail().x - 250.f));
		ImGui::BeginChild("##Emitters", ImVec2(m_EmittersPanelWidth, contentHeight), true);
		bChanged |= DrawEmittersList();
		ImGui::EndChild();

		ImGui::SameLine(0.f, 0.f);
		m_EmittersPanelWidth += Splitter("##EmittersSplitter", true, 6.f, contentHeight);
		ImGui::SameLine(0.f, 0.f);

		// Right: the selected emitter, and the curve panel under it
		ImGui::BeginChild("##Details", ImVec2(0.f, contentHeight), false);
		if (m_SelectedEmitterIndex < m_Emitters.size())
		{
			ParticleEmitter& emitter = m_Emitters[m_SelectedEmitterIndex];
			GetCurrentScene()->DrawAABB(emitter.VisibilityAABB, emitter.RelativeTransform);

			const bool bShowCurvePanel = m_ActiveCurve != CurveTarget::None;
			const float detailsHeight = ImGui::GetContentRegionAvail().y;
			const float splitterThickness = 6.f;
			m_CurvePanelHeight = glm::clamp(m_CurvePanelHeight, 140.f, glm::max(140.f, detailsHeight - 160.f));

			const float propertiesTop = ImGui::GetCursorPosY();
			const float propertiesHeight = bShowCurvePanel ? detailsHeight - m_CurvePanelHeight - splitterThickness : 0.f;
			ImGui::BeginChild("##Properties", ImVec2(0.f, propertiesHeight), false);
			bChanged |= DrawEmitterDetails(emitter);
			ImGui::EndChild();

			if (bShowCurvePanel)
			{
				ImGui::SetCursorPosY(propertiesTop + propertiesHeight);
				m_CurvePanelHeight -= Splitter("##CurveSplitter", false, splitterThickness, ImGui::GetContentRegionAvail().x);
				ImGui::SetCursorPosY(propertiesTop + propertiesHeight + splitterThickness);
				ImGui::BeginChild("##CurvePanel", ImVec2(0.f, 0.f), true);
				bChanged |= DrawCurvePanel(emitter);
				ImGui::EndChild();
			}
		}
		else
		{
			ImGui::Spacing();
			ImGui::TextDisabled(m_Emitters.empty() ? "This particle system has no emitters. Click '+' to add one" : "Select an emitter to edit it");
		}
		ImGui::EndChild();

		bChanged |= bGuizmoChanged;
		bGuizmoChanged = false;

		if (bChanged)
		{
			m_Asset->SetEmitters(m_Emitters);
			RecalculateLifetime();
			m_Asset->SetDirty(true);
			m_Asset->OnModified();
		}

		// Finite systems are restarted automatically once everything has finished
		if (m_Timer.GetSeconds() >= m_Lifetime)
			RestartPreview();

		ImGui::End();

		DrawViewport(false, m_WindowName);
	}

	void ParticleSystemAssetEditor::DrawToolbar()
	{
		if (ImGui::Button("Save"))
			Asset::Save(m_Asset);

		ImGui::SameLine();
		if (ImGui::Button("Restart"))
			RestartPreview();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Restarts the preview from the beginning (`Fast Forward To` is applied again)");

		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		if (m_Lifetime < FLT_MAX)
			ImGui::TextDisabled("Preview: %.1f / %.1f s (restarts automatically)", float(m_Timer.GetSeconds()), m_Lifetime);
		else
			ImGui::TextDisabled("Preview: %.1f s", float(m_Timer.GetSeconds()));

		ImGui::SameLine();
		UI::HelpMarker("Values marked 'Curve' change over each particle's lifetime. Click a curve's preview to edit it in the panel below the settings.\n"
			"Use the gizmo in the viewport to move the selected emitter");
	}

	bool ParticleSystemAssetEditor::DrawEmittersList()
	{
		bool bChanged = false;

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Emitters");
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() + ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
		if (ImGui::Button("+", ImVec2(ImGui::GetFrameHeight(), 0.f)))
		{
			ParticleEmitter& emitter = m_Emitters.emplace_back();
			emitter.Name = "Emitter " + std::to_string(m_Emitters.size());
			SelectEmitter(m_Emitters.size() - 1u);
			bChanged = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Add an emitter");
		ImGui::Separator();

		enum class Operation { None, Duplicate, MoveUp, MoveDown, Delete };
		Operation operation = Operation::None;
		size_t operationIndex = s_InvalidIndex;

		for (size_t i = 0; i < m_Emitters.size(); ++i)
		{
			auto& emitter = m_Emitters[i];
			ImGui::PushID((void*)emitter.ID.GetHash());

			if (ImGui::Checkbox("##Emit", &emitter.bEmit))
				bChanged = true;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(emitter.bEmit ? "Emitting. Uncheck to mute this emitter" : "Muted. Check to emit particles");

			ImGui::SameLine();
			if (!emitter.bEmit)
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			if (ImGui::Selectable(emitter.Name.c_str(), m_SelectedEmitterIndex == i))
				SelectEmitter(i);
			if (!emitter.bEmit)
				ImGui::PopStyleColor();

			if (ImGui::BeginPopupContextItem("##EmitterMenu"))
			{
				if (ImGui::MenuItem("Duplicate"))
					operation = Operation::Duplicate, operationIndex = i;
				if (ImGui::MenuItem("Move Up", nullptr, false, i > 0))
					operation = Operation::MoveUp, operationIndex = i;
				if (ImGui::MenuItem("Move Down", nullptr, false, i + 1 < m_Emitters.size()))
					operation = Operation::MoveDown, operationIndex = i;
				ImGui::Separator();
				if (ImGui::MenuItem("Delete"))
					operation = Operation::Delete, operationIndex = i;
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}

		if (m_Emitters.empty())
			ImGui::TextDisabled("Click '+' to add\nan emitter");
		else
			ImGui::TextDisabled("Right-click an emitter\nfor more options");

		switch (operation)
		{
			case Operation::Duplicate:
			{
				ParticleEmitter copy = m_Emitters[operationIndex];
				copy.ID = GUID();
				copy.Name += " (copy)";
				m_Emitters.insert(m_Emitters.begin() + operationIndex + 1, std::move(copy));
				SelectEmitter(operationIndex + 1);
				bChanged = true;
				break;
			}
			case Operation::MoveUp:
			case Operation::MoveDown:
			{
				const size_t other = operation == Operation::MoveUp ? operationIndex - 1 : operationIndex + 1;
				std::swap(m_Emitters[operationIndex], m_Emitters[other]);
				if (m_SelectedEmitterIndex == operationIndex)
					m_SelectedEmitterIndex = other;
				else if (m_SelectedEmitterIndex == other)
					m_SelectedEmitterIndex = operationIndex;
				bChanged = true;
				break;
			}
			case Operation::Delete:
			{
				m_Emitters.erase(m_Emitters.begin() + operationIndex);
				if (m_SelectedEmitterIndex == operationIndex)
					SelectEmitter(m_Emitters.empty() ? s_InvalidIndex : std::min(operationIndex, m_Emitters.size() - 1u));
				else if (m_SelectedEmitterIndex != s_InvalidIndex && m_SelectedEmitterIndex > operationIndex)
					--m_SelectedEmitterIndex;
				bChanged = true;
				break;
			}
			default:
				break;
		}

		return bChanged;
	}

	bool ParticleSystemAssetEditor::DrawEmitterDetails(ParticleEmitter& emitter)
	{
		using EmissionShapeType = ParticleEmitter::EmissionShapeType;
		using CollisionModeType = ParticleEmitter::CollisionModeType;
		bool bChanged = false;

		auto curveProperty = [this, &bChanged](const char* label, auto& property, CurveTarget target, const CurvePropertyParams& params)
		{
			bool bEdit = false;
			bChanged |= CurveUI::PropertyCurve(label, property, params, m_ActiveCurve == target, bEdit);
			if (bEdit)
				OpenCurve(target);
		};

		// ---------------- Emitter ----------------
		{
			std::string summary = emitter.bExplode ? ("Bursts of " + std::to_string(emitter.SpawnRate)) : (std::to_string(emitter.SpawnRate) + "/s");
			summary += emitter.LoopCount == 0 ? ", looping" : (", " + std::to_string(emitter.LoopCount) + (emitter.LoopCount == 1 ? " loop" : " loops"));
			if (!emitter.bExplode && emitter.SpawnPerMeter > 0.f)
				summary += " + " + FormatNumber(emitter.SpawnPerMeter) + "/m";
			if (emitter.StartDelay > 0.f)
				summary += ", delay " + FormatNumber(emitter.StartDelay) + " s";
			if (emitter.SimulationSpace == ParticleEmitter::SimulationSpaceType::Local)
				summary += ", local space";

			if (BeginSection("Emitter", summary, true))
			{
				bChanged |= UI::PropertyText("Name", emitter.Name);
				bChanged |= UI::Property("Emit", emitter.bEmit, "Unchecked emitters don't spawn particles");
				bChanged |= UI::PropertyDrag("Spawn Rate", emitter.SpawnRate, 1.f, 0, int(ParticleEmitter::MaxSpawnRate), emitter.bExplode ? "Particles spawned by each burst" : "Particles spawned per second");

				if (emitter.bExplode)
					UI::PushItemDisabled();

				if (UI::PropertyDrag("Spawn per Meter", emitter.SpawnPerMeter, 0.1f, 0.f, 0.f, "Extra particles per unit of distance the emitter moves, on top of `Spawn Rate`.\n"
					"Use it for trails. Moving emitters leave a continuous line of particles, however fast they move. `Spawn Rate` can be 0.\nNot used in `Explode` mode"))
				{
					emitter.SpawnPerMeter = glm::max(0.f, emitter.SpawnPerMeter);
					bChanged = true;
				}

				if (emitter.bExplode)
					UI::PopItemDisabled();

				bChanged |= UI::Property("Explode", emitter.bExplode, "Spawn particles in bursts (one burst per loop) instead of continuously");
				if (UI::PropertyDrag("Loop Duration", emitter.LoopDuration, 0.05f, 0.f, 0.f, emitter.bExplode ? "Time between bursts, in seconds" : "Length of one loop, in seconds"))
				{
					emitter.LoopDuration = glm::max(0.f, emitter.LoopDuration);
					bChanged = true;
				}
				bChanged |= UI::PropertyDrag("Loop Count", emitter.LoopCount, 1.f, 0, 0, "How many loops to play. 0 - loop forever");
				if (UI::PropertyDrag("Start Delay", emitter.StartDelay, 0.05f, 0.f, 0.f, "Seconds to wait before the emitter starts spawning, counted from when it's spawned or restarted.\n"
					"`Fast Forward To` uses up the delay first"))
				{
					emitter.StartDelay = glm::max(0.f, emitter.StartDelay);
					bChanged = true;
				}
				if (UI::PropertyDrag("Fast Forward To", emitter.FastForwardTo, 0.1f, 0.f, 0.f, "When the emitter is spawned, it starts as if it had already been running for this many seconds.\n"
					"For looping emitters, `Lifetime Max` is enough to reach the fully filled state. Collisions are ignored during the fast-forward"))
				{
					emitter.FastForwardTo = glm::max(0.f, emitter.FastForwardTo);
					bChanged = true;
				}
				bChanged |= UI::ComboEnum("Simulation Space", emitter.SimulationSpace, "World: particles stay where they were spawned, so a moving emitter leaves them behind.\n"
					"Local: particles move, turn and scale with the emitter");
				bChanged |= UI::Property("Destroy Immediately", emitter.bDestroyImmediately, "When the emitter is disabled or destroyed, remove its particles right away instead of letting them finish their lifetime");
				UI::EndPropertyGrid();

				if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_SpanAvailWidth))
				{
					auto& transform = emitter.RelativeTransform;
					glm::quat quat = transform.Rotation.GetQuat();
					bChanged |= UI::DrawVec3Control("Location", transform.Location, glm::vec3{ 0.f });
					if (UI::DrawQuatControl("Rotation (Quat)", quat))
					{
						transform.Rotation = quat;
						bChanged = true;
					}
					bChanged |= UI::DrawVec3Control("Scale", transform.Scale3D, glm::vec3{ 1.f });
					ImGui::TreePop();
				}

				UI::BeginPropertyGrid("EmitterBounds");
				const char* boundsHelp = "When this box isn't visible to the camera, the emitter's particles aren't rendered. Keep it as small as possible, but big enough to contain the particles";
				bChanged |= UI::PropertyDrag("Culling Bounds Min", emitter.VisibilityAABB.Min, 0.1f, 0.f, 0.f, boundsHelp);
				bChanged |= UI::PropertyDrag("Culling Bounds Max", emitter.VisibilityAABB.Max, 0.1f, 0.f, 0.f, boundsHelp);
				EndSection();
			}
		}

		// ---------------- Lifetime ----------------
		{
			const std::string summary = emitter.LifetimeMin == emitter.LifetimeMax ? (FormatNumber(emitter.LifetimeMax) + " s")
				: (FormatNumber(emitter.LifetimeMin) + " - " + FormatNumber(emitter.LifetimeMax) + " s");
			if (BeginSection("Lifetime", summary, true))
			{
				const char* help = "Each particle lives a random time between Min and Max, in seconds";
				if (UI::PropertyDrag("Lifetime Min", emitter.LifetimeMin, 0.05f, 0.f, FLT_MAX, help))
				{
					emitter.LifetimeMin = glm::max(0.f, emitter.LifetimeMin);
					emitter.LifetimeMax = glm::max(emitter.LifetimeMax, emitter.LifetimeMin);
					bChanged = true;
				}
				if (UI::PropertyDrag("Lifetime Max", emitter.LifetimeMax, 0.05f, 0.f, FLT_MAX, help))
				{
					emitter.LifetimeMax = glm::max(0.f, emitter.LifetimeMax);
					emitter.LifetimeMin = glm::min(emitter.LifetimeMin, emitter.LifetimeMax);
					bChanged = true;
				}
				EndSection();
			}
		}

		// ---------------- Shape ----------------
		if (BeginSection("Shape", GetShapeName(emitter.EmissionShape), true))
		{
			bChanged |= UI::ComboEnum("Emission Shape", emitter.EmissionShape, "Where particles are spawned, relative to the emitter");
			switch (emitter.EmissionShape)
			{
				case EmissionShapeType::Sphere:
				case EmissionShapeType::SphereSurface:
					bChanged |= UI::PropertyDrag("Sphere Radius", emitter.SphereRadius, 0.05f);
					break;
				case EmissionShapeType::Box:
				case EmissionShapeType::BoxSurface:
					bChanged |= UI::PropertyDrag("Box Min", emitter.BoxMin, 0.05f);
					bChanged |= UI::PropertyDrag("Box Max", emitter.BoxMax, 0.05f);
					break;
				case EmissionShapeType::Ring:
					bChanged |= UI::PropertyDrag("Ring Radius", emitter.RingRadius, 0.05f);
					bChanged |= UI::PropertyDrag("Ring Thickness", emitter.RingThickness, 0.05f);
					break;
				case EmissionShapeType::Mesh:
					bChanged |= EditorResources::DrawAssetSelection("Mesh", emitter.MeshAsset, "Particles are spawned on the surface of this mesh");
					if (emitter.IsSkeletalMeshUsed())
					{
						bChanged |= EditorResources::DrawAssetSelection("Animation Clip", emitter.MeshAnimationAsset, "Animates the skeletal mesh, so particles are spawned on its animated surface");
						bChanged |= UI::PropertyDrag("Playback Speed", emitter.ClipPlaybackSpeed, 0.05f);
						bChanged |= UI::Property("Loop Animation", emitter.bClipLooping);
					}
					break;
				default:
					break;
			}

			if (emitter.EmissionShape != EmissionShapeType::Point)
			{
				bChanged |= UI::PropertyDrag("Normal Velocity Factor", emitter.NormalVelocityFactor, 0.1f, 0.f, 0.f, "Adds the emission shape's normal direction, multiplied by this value, to the particle's initial velocity.\n"
					"For example, positive values push particles away from the center of a sphere, negative values pull them in. "
					"Always follows the emitter's rotation and scale, regardless of `Velocity Space`");
			}
			EndSection();
		}

		// ---------------- Velocity ----------------
		{
			std::string summary = emitter.VelocityMin == emitter.VelocityMax ? "Fixed" : "Random";
			if (emitter.InheritVelocity != 0.f)
				summary += ", inherits";
			if (emitter.VelocitySpace == ParticleEmitter::VelocitySpaceType::World)
				summary += ", world space";
			if (!emitter.VelocityCoef.IsConstant())
				summary += ", over lifetime";

			if (BeginSection("Velocity", summary, false))
			{
				bChanged |= UI::ComboEnum("Velocity Space", emitter.VelocitySpace, "Space of `Velocity Min/Max` and `Velocity Multiplier`.\n"
					"Local: relative to the emitter. Velocity rotates and scales with the emitter, and the multiplier's axes rotate with it.\n"
					"World: world-space values. The emitter's rotation and scale are ignored.\n"
					"The part of the velocity that comes from `Normal Velocity Factor` always follows the emitter");
				bChanged |= UI::PropertyDrag("Inherit Emitter Velocity", emitter.InheritVelocity, 0.01f, 0.f, 0.f, "Fraction of the emitter's own velocity that particles start with.\n"
					"0 - none, 1 - particles start moving with the emitter (for example, sparks from a moving object)");

				const char* help = "Each particle starts with a random velocity between Min and Max";
				bChanged |= UI::PropertyDrag("Velocity Min", emitter.VelocityMin, 0.05f, 0.f, 0.f, help);
				bChanged |= UI::PropertyDrag("Velocity Max", emitter.VelocityMax, 0.05f, 0.f, 0.f, help);

				CurvePropertyParams params;
				params.HelpMessage = "Multiplies the velocity over the particle's lifetime, per axis. 1 - no change, 0 - stopped";
				curveProperty("Velocity Multiplier", emitter.VelocityCoef, CurveTarget::VelocityCoef, params);
				EndSection();
			}
		}

		// ---------------- Forces ----------------
		{
			std::string summary;
			auto append = [&summary](const char* text) { summary += summary.empty() ? text : (std::string(", ") + text); };
			if (emitter.bApplyGravity)
				append("Gravity");
			if (emitter.RadialAcceleration != 0.f)
				append("Radial");
			if (emitter.TangentialAcceleration != 0.f)
				append("Swirl");
			if (emitter.TurbulenceStrength != 0.f)
				append("Turbulence");
			if (!emitter.Drag.IsConstant() || emitter.Drag.Constant > 0.f)
				append("Drag");
			if (summary.empty())
				summary = "None";

			if (BeginSection("Forces", summary, false))
			{
				bChanged |= UI::Property("Apply Gravity", emitter.bApplyGravity);
				bChanged |= UI::PropertyDrag("Radial Acceleration", emitter.RadialAcceleration, 0.1f, 0.f, 0.f, "Positive values push particles away from the emitter's center, negative values pull them in");
				bChanged |= UI::PropertyDrag("Tangential Acceleration", emitter.TangentialAcceleration, 0.1f, 0.f, 0.f, "Makes particles swirl around the emitter's local Z axis. The sign sets the direction");
				{
					CurvePropertyParams dragParams;
					dragParams.Min = 0.f;
					dragParams.Max = 1000.f;
					dragParams.HelpMessage = "Air resistance, per second. Particles lose speed over time. Higher values slow them down faster.\n"
						"Unlike `Velocity Multiplier`, it changes the particle's real velocity, so with gravity particles reach a steady falling speed.\n"
						"As a curve, it changes over the particle's lifetime (for example, sparks that fly freely, then get caught by the air)";
					curveProperty("Drag", emitter.Drag, CurveTarget::Drag, dragParams);
				}
				bChanged |= UI::PropertyDrag("Turbulence", emitter.TurbulenceStrength, 0.05f, 0.f, 0.f, "Strength of a swirling force field (curl noise), as an acceleration.\n"
					"For example, makes smoke/dust move naturally instead of in straight lines. 0 - off");
				if (UI::PropertyDrag("Turbulence Scale", emitter.TurbulenceScale, 0.05f, 0.01f, 0.f, "Size of the swirls, in world units. Smaller values give tighter, busier swirls"))
				{
					emitter.TurbulenceScale = glm::max(0.01f, emitter.TurbulenceScale);
					bChanged = true;
				}
				bChanged |= UI::PropertyDrag("Turbulence Speed", emitter.TurbulenceSpeed, 0.05f, 0.f, 0.f, "How fast the swirl pattern drifts, in world units per second. 0 - a static pattern");
				EndSection();
			}
		}

		// ---------------- Color ----------------
		{
			std::string summary = emitter.Color.IsConstant() ? "Constant" : "Gradient";
			const bool bIntensityChanges = !emitter.ColorIntensity.IsConstant() || emitter.ColorIntensity.Constant != 1.f;
			if (bIntensityChanges)
				summary += emitter.ColorIntensity.IsConstant() ? (", intensity " + FormatNumber(emitter.ColorIntensity.Constant)) : ", intensity curve";
			if (emitter.bRandomTint)
				summary += ", random tint";

			if (BeginSection("Color", summary, true))
			{
				bool bEdit = false;
				bChanged |= CurveUI::PropertyGradient("Color", emitter.Color, "The particle's color and opacity. As a gradient, it changes over the particle's lifetime",
					m_ActiveCurve == CurveTarget::Color, bEdit);
				if (bEdit)
					OpenCurve(CurveTarget::Color);

				CurvePropertyParams params;
				params.Min = 0.f;
				params.Max = 1000.f;
				params.HelpMessage = "HDR brightness multiplier of the color. 1 - unchanged. Values above 1 make particles bright enough to bloom";
				curveProperty("Intensity", emitter.ColorIntensity, CurveTarget::ColorIntensity, params);

				bChanged |= UI::Property("Random Tint", emitter.bRandomTint, "Each particle's color is multiplied by a random color between the two colors, so particles of the same emitter differ slightly");
				
				if (!emitter.bRandomTint)
					UI::PushItemDisabled();
				bChanged |= UI::PropertyColor("Random Tint A", emitter.RandomTintA);
				bChanged |= UI::PropertyColor("Random Tint B", emitter.RandomTintB);
				if (!emitter.bRandomTint)
					UI::PopItemDisabled();

				EndSection();
			}
		}

		// ---------------- Size ----------------
		{
			const glm::vec2 size = emitter.Size.Constant;
			std::string summary = CurveSummary(emitter.Size, FormatNumber(size.x) + " x " + FormatNumber(size.y));
			if (emitter.StartSizeMultiplierRandomRange.x != 1.f || emitter.StartSizeMultiplierRandomRange.y != 1.f)
				summary += ", random size";
			if (BeginSection("Size", summary, true))
			{
				CurvePropertyParams params;
				params.HelpMessage = "Width and height of a particle. Also scaled by the emitter's scale";
				curveProperty("Size", emitter.Size, CurveTarget::Size, params);
				if (PropertyRandomRange("Random Start Size Multiplier", emitter.StartSizeMultiplierRandomRange, 0.01f, "Min: %.2f", "Max: %.2f",
					"Each particle's size is multiplied by a random value in this range. [1; 1] - no randomness"))
				{
					emitter.StartSizeMultiplierRandomRange = glm::max(emitter.StartSizeMultiplierRandomRange, glm::vec2(0.f));
					bChanged = true;
				}
				EndSection();
			}
		}

		// ---------------- Rotation ----------------
		{
			std::string summary = CurveSummary(emitter.RotationZ, FormatNumber(emitter.RotationZ.Constant) + " deg");
			if (emitter.StartRotationRandomRange.x != 0.f || emitter.StartRotationRandomRange.y != 0.f)
				summary += ", random start";
			const bool bSpinCurve = !emitter.RotationSpeed.IsConstant() || emitter.RotationSpeed.Constant != 0.f;
			if (emitter.StartRotationSpeedRandomRange.x != 0.f || emitter.StartRotationSpeedRandomRange.y != 0.f || bSpinCurve)
				summary += ", spinning";
			if (emitter.bFaceDirection)
				summary += ", faces velocity";
			if (BeginSection("Rotation", summary, false))
			{
				CurvePropertyParams params;
				params.Speed = 1.f;
				params.HelpMessage = "Rotation around the view direction, in degrees";
				curveProperty("Rotation", emitter.RotationZ, CurveTarget::RotationZ, params);
				bChanged |= PropertyRandomRange("Random Start Rotation", emitter.StartRotationRandomRange, 1.f, "Min: %.0f deg", "Max: %.0f deg",
					"Added to the rotation above. For example, [-180; 180] gives every particle a different orientation");
				bChanged |= PropertyRandomRange("Random Start Rotation Speed", emitter.StartRotationSpeedRandomRange, 1.f, "Min: %.0f deg/s", "Max: %.0f deg/s",
					"How fast each particle spins, on top of the rotation above");
				{
					CurvePropertyParams speedParams;
					speedParams.Speed = 1.f;
					speedParams.HelpMessage = "Spin speed in degrees per second that changes over the particle's lifetime, for example spinning fast and slowing down.\n"
						"Adds to `Rotation Speed`";
					curveProperty("Rotation Speed over Lifetime", emitter.RotationSpeed, CurveTarget::RotationSpeed, speedParams);
				}
				bChanged |= UI::Property("Face Velocity", emitter.bFaceDirection, "Rotate particles so they point in the direction they move");
				EndSection();
			}
		}

		// ---------------- Rendering ----------------
		{
			std::string summary = !emitter.bAlphaBlending ? "Opaque" : (emitter.bAdditive ? "Additive" : "Translucent");
			if (emitter.Texture)
				summary += ", textured";
			if (BeginSection("Rendering", summary, false))
			{
				bChanged |= EditorResources::DrawAssetSelection("Texture", emitter.Texture);
				bChanged |= UI::Property("Alpha Blending", emitter.bAlphaBlending, "Translucent particles. When off, particles are opaque (texture alpha below 0.5 is cut out)");
				if (!emitter.bAlphaBlending)
					UI::PushItemDisabled();
				bChanged |= UI::Property("Additive", emitter.bAdditive, "Particles add light instead of covering what's behind them. Good for fire, sparks and magic");
				if (!emitter.bAlphaBlending)
					UI::PopItemDisabled();

				if (emitter.Texture)
				{
					UI::EndPropertyGrid();
					UI::TextWithSeparator("Sprite Sheet");
					UI::BeginPropertyGrid("SpriteSheet");
					bChanged |= UI::PropertyDrag("Columns & Rows", emitter.AnimationImagesNum, 1.f, 1, INT_MAX, "Number of frames horizontally/vertically in the texture");
					if (emitter.AnimationImagesNum.x * emitter.AnimationImagesNum.y > 1u)
					{
						bChanged |= UI::PropertyDrag("Animation Speed", emitter.AnimationSpeed, 0.05f, 0.f, 0.f, "How many times the frames are played over a particle's lifetime");
						bChanged |= UI::Property("Blend Frames", emitter.bBlendAnimation, "Smoothly blend between frames instead of switching");
					}
				}
				EndSection();
			}
		}

		// ---------------- Collision ----------------
		{
			const char* summary = emitter.CollisionMode == CollisionModeType::None ? "None" : (emitter.CollisionMode == CollisionModeType::Bounce ? "Bounce" : "Destroy on hit");
			if (BeginSection("Collision", summary, false))
			{
				bChanged |= UI::ComboEnum("Collision Mode", emitter.CollisionMode, "Particles collide with what's visible on the screen (screen-space collision)");
				if (emitter.CollisionMode != CollisionModeType::None)
				{
					bChanged |= UI::PropertyDrag("Collider Size Ratio", emitter.ColliderSizeRatio, 0.05f, 0.f, 0.f, "Scales the particle's collider. Increase it to stop small, fast particles from passing through surfaces");
					if (emitter.CollisionMode == CollisionModeType::Bounce)
					{
						const char* help = "How much speed a particle keeps after a bounce, random between Min and Max. 1 - all of it";
						bChanged |= UI::PropertyDrag("Bounciness Min", emitter.BouncinessMin, 0.05f, 0.f, 0.f, help);
						bChanged |= UI::PropertyDrag("Bounciness Max", emitter.BouncinessMax, 0.05f, 0.f, 0.f, help);
					}
				}
				EndSection();
			}
		}

		return bChanged;
	}

	bool ParticleSystemAssetEditor::DrawCurvePanel(ParticleEmitter& emitter)
	{
		struct CurveInfo
		{
			const char* Title = "";
			std::vector<const char*> ComponentNames;
		};

		CurveInfo info;
		switch (m_ActiveCurve)
		{
			case CurveTarget::Color:          info = { "Color over lifetime", {} }; break;
			case CurveTarget::ColorIntensity: info = { "Intensity over lifetime", { "Intensity" } }; break;
			case CurveTarget::Size:           info = { "Size over lifetime", { "Width", "Height" } }; break;
			case CurveTarget::RotationZ:      info = { "Rotation over lifetime (degrees)", { "Rotation" } }; break;
			case CurveTarget::RotationSpeed:  info = { "Rotation speed over lifetime (degrees per second)", { "Speed" } }; break;
			case CurveTarget::Drag:           info = { "Drag over lifetime (per second)", { "Drag" } }; break;
			case CurveTarget::VelocityCoef:   info = { "Velocity multiplier over lifetime", { "X", "Y", "Z" } }; break;
			default: return false;
		}

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(info.Title);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::GetFrameHeight());
		if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.f)))
		{
			m_ActiveCurve = CurveTarget::None;
			return false;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Close the curve panel");

		bool bChanged = false;

		auto drawCurve = [&](auto& property)
		{
			if (property.IsConstant())
			{
				ImGui::TextDisabled("This value is constant.");
				ImGui::SameLine();
				if (ImGui::SmallButton("Make it a curve"))
				{
					property.SetMode(CurvePropertyMode::Curve);
					m_CurveEditor.RequestFit();
					bChanged = true;
				}
				return;
			}

			using ValueType = std::decay_t<decltype(property.Constant)>;
			TypedCurveView<ValueType> view(info.Title, IM_COL32(230, 230, 230, 255), &property.Keys, property.Constant);

			static const ImVec4 s_ComponentColors[] = { ImVec4(0.95f, 0.35f, 0.35f, 1.f), ImVec4(0.4f, 0.9f, 0.4f, 1.f), ImVec4(0.4f, 0.55f, 1.f, 1.f) };
			std::vector<CurveEditorEntry> entries;
			const uint32_t count = view.GetCurveComponentsCount();
			for (uint32_t c = 0; c < count; ++c)
			{
				CurveEditorEntry& entry = entries.emplace_back();
				entry.View = &view;
				entry.Component = c;
				entry.Color = count == 1 ? ImVec4(0.9f, 0.9f, 0.9f, 1.f) : s_ComponentColors[c % 3];
				entry.Label = c < info.ComponentNames.size() ? info.ComponentNames[c] : view.GetCurveComponentName(c);
			}

			if (m_CurveComponentVisible.size() != entries.size())
				m_CurveComponentVisible.assign(entries.size(), true);
			for (size_t i = 0; i < entries.size(); ++i)
				entries[i].bVisible = m_CurveComponentVisible[i];

			CurveEditorSettings settings;
			settings.TimeAxisLabel = "Lifetime";
			settings.FitTimeMin = 0.f;
			settings.FitTimeMax = 1.f;
			settings.TimeMin = 0.f;
			settings.TimeMax = 1.f;
			settings.bAllowKeyEditing = true;
			settings.TimeDisplayScale = 100.f;
			settings.TimeDisplayFormat = "%.1f%%";
			settings.FormatTime = [](float time)
			{
				char buffer[16];
				snprintf(buffer, sizeof(buffer), "%.0f%%", time * 100.f);
				return std::string(buffer);
			};

			if (entries.size() > 1)
			{
				m_CurveEditor.DrawVisibilityToggles(entries);
				for (size_t i = 0; i < entries.size(); ++i)
					m_CurveComponentVisible[i] = entries[i].bVisible;
			}
			else if (ImGui::Button("Fit"))
			{
				m_CurveEditor.RequestFit();
			}

			ImGui::SameLine();
			UI::HelpMarker("X axis: the particle's lifetime, from spawn (0%) to death (100%).\n"
				"Double-click empty space to add a key. Drag keys to change them, or select one and edit it in the fields above the plot.\n"
				"Right-click a key to change its interpolation or delete it. Press Delete to remove the selected keys.\n"
				"'Cubic' keys get tangent handles. Drag empty space to pan, scroll to zoom, 'Fit' (or a middle-button double-click) to frame the curve");

			// The selected key's properties, between the toolbar and the plot
			bChanged |= m_CurveEditor.DrawSelectedKeyProperties(entries, settings);

			bChanged |= m_CurveEditor.Draw("##ParticleCurve", entries, settings);
		};

		switch (m_ActiveCurve)
		{
			case CurveTarget::Color:
				if (emitter.Color.IsConstant())
				{
					ImGui::TextDisabled("The color is constant.");
					ImGui::SameLine();
					if (ImGui::SmallButton("Make it a gradient"))
					{
						emitter.Color.SetMode(CurvePropertyMode::Curve);
						bChanged = true;
					}
				}
				else
				{
					ImGui::Spacing();
					bChanged |= m_GradientEditor.Draw("##ColorGradient", emitter.Color.Keys);
				}
				break;
			case CurveTarget::ColorIntensity: drawCurve(emitter.ColorIntensity); break;
			case CurveTarget::Size:           drawCurve(emitter.Size); break;
			case CurveTarget::RotationZ:      drawCurve(emitter.RotationZ); break;
			case CurveTarget::RotationSpeed:  drawCurve(emitter.RotationSpeed); break;
			case CurveTarget::Drag:           drawCurve(emitter.Drag); break;
			case CurveTarget::VelocityCoef:   drawCurve(emitter.VelocityCoef); break;
			default: break;
		}

		return bChanged;
	}

	void ParticleSystemAssetEditor::OpenCurve(CurveTarget target)
	{
		if (m_ActiveCurve == target)
			return;

		m_ActiveCurve = target;
		m_CurveComponentVisible.clear();
		m_CurveEditor.ClearSelection();
		m_CurveEditor.RequestFit();
	}

	void ParticleSystemAssetEditor::SelectEmitter(size_t index)
	{
		if (m_SelectedEmitterIndex == index)
			return;

		m_SelectedEmitterIndex = index;
		m_CurveEditor.ClearSelection();
		m_CurveEditor.RequestFit();
	}

	void ParticleSystemAssetEditor::RestartPreview()
	{
		m_Timer.Restart();
		m_Entity.GetComponent<ParticleSystemComponent>().Restart();
	}

	void ParticleSystemAssetEditor::RecalculateLifetime()
	{
		// Time until every particle of a finite system has died
		m_Lifetime = m_Emitters.empty() ? FLT_MAX : 0.f;
		for (const auto& emitter : m_Emitters)
		{
			if (emitter.LoopCount == 0)
			{
				m_Lifetime = FLT_MAX;
				break;
			}

			const float currentLifetime = emitter.StartDelay + float(emitter.LoopCount) * emitter.LoopDuration + emitter.LifetimeMax;
			m_Lifetime = glm::max(currentLifetime, m_Lifetime);
		}

		m_Timer.Restart();
	}

	void ParticleSystemAssetEditor::UpdateGuizmo()
	{
		if (m_SelectedEmitterIndex >= m_Emitters.size())
			return;

		auto& emitter = m_Emitters[m_SelectedEmitterIndex];
		bGuizmoChanged = DrawGuizmo(emitter.RelativeTransform, true, false);
	}
}
