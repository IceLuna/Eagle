#pragma once

#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Utils/Timer.h"

#include "AssetEditor.h"
#include "../Undo/AssetUndo.h"
#include "../Widgets/CurveEditor.h"
#include "../Widgets/GradientEditor.h"

namespace Eagle
{
	class AssetParticleSystem;
	class ParticleSystemComponent;

	class ParticleSystemAssetEditor : public AssetEditor
	{
	public:
		ParticleSystemAssetEditor(const Ref<AssetParticleSystem>& asset);

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }
		bool UsesGuizmo() const override { return true; }

	private:
		// Over-lifetime properties that can be opened in the curve panel
		enum class CurveTarget
		{
			None, Color, ColorIntensity, Size, RotationZ, RotationSpeed, VelocityCoef, VelocityOverLifetime, Drag, TurbulenceStrength, TurbulenceScale, ApplyGravity
		};

		void OnViewportEnd() override;
		void UpdateGuizmo();

		void ApplyEmitters();

		struct EditState
		{
			std::vector<ParticleEmitter> Emitters;
			std::vector<std::string> Serialized; // Per emitter, as they're saved. Emitters only compare their IDs, so this is what's compared

			size_t GetMemoryUsage() const;
			bool operator==(const EditState& other) const { return Serialized == other.Serialized; }
		};

		void RecalculateLifetime();
		void RestartPreview();

		void DrawToolbar();
		bool DrawEmittersList(); // Returns true if emitters were added, removed, reordered or toggled
		bool DrawEmitterDetails(ParticleEmitter& emitter);
		bool DrawCurvePanel(ParticleEmitter& emitter);
		void OpenCurve(CurveTarget target);
		void SelectEmitter(size_t index);

		static EditState CaptureState(const Ref<AssetParticleSystem>& asset);
		static void ApplyState(const Ref<AssetParticleSystem>& asset, const EditState& state);
		static std::string DescribeState(const EditState& before, const EditState& after);

	private:
		static constexpr size_t s_InvalidIndex = size_t(-1);

		Ref<AssetParticleSystem> m_Asset;
		std::vector<ParticleEmitter> m_Emitters;
		size_t m_SelectedEmitterIndex = s_InvalidIndex;
		Entity m_Entity;
		Timer m_Timer;
		float m_Lifetime = FLT_MAX;
		AssetEditTracker<AssetParticleSystem, EditState> m_EditTracker;

		// UI state
		CurveTarget m_ActiveCurve = CurveTarget::None;
		CurveEditor m_CurveEditor;
		GradientEditor m_GradientEditor;
		std::vector<bool> m_CurveComponentVisible;
		float m_EmittersPanelWidth = 200.f;
		float m_CurvePanelHeight = 280.f;
	};
}
