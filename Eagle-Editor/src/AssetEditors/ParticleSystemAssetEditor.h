#pragma once

#include "Eagle/Renderer/ParticleEmitter.h"
#include "Eagle/Utils/Timer.h"

#include "AssetEditor.h"
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

	private:
		// Over-lifetime properties that can be opened in the curve panel
		enum class CurveTarget
		{
			None, Color, ColorIntensity, Size, RotationZ, RotationSpeed, VelocityCoef, Drag
		};

		void OnViewportEnd() override { UpdateGuizmo(); }
		void UpdateGuizmo();
		void RecalculateLifetime();
		void RestartPreview();

		void DrawToolbar();
		bool DrawEmittersList(); // Returns true if emitters were added, removed, reordered or toggled
		bool DrawEmitterDetails(ParticleEmitter& emitter);
		bool DrawCurvePanel(ParticleEmitter& emitter);
		void OpenCurve(CurveTarget target);
		void SelectEmitter(size_t index);

	private:
		static constexpr size_t s_InvalidIndex = size_t(-1);

		Ref<AssetParticleSystem> m_Asset;
		std::vector<ParticleEmitter> m_Emitters;
		size_t m_SelectedEmitterIndex = s_InvalidIndex;
		Entity m_Entity;
		std::string m_WindowName;
		Timer m_Timer;
		float m_Lifetime = FLT_MAX;
		bool bGuizmoChanged = false;

		// UI state
		CurveTarget m_ActiveCurve = CurveTarget::None;
		CurveEditor m_CurveEditor;
		GradientEditor m_GradientEditor;
		std::vector<bool> m_CurveComponentVisible;
		float m_EmittersPanelWidth = 200.f;
		float m_CurvePanelHeight = 280.f;
	};
}
