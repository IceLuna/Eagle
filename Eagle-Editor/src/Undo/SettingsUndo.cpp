#include "SettingsUndo.h"

#include "Eagle/Core/Application.h"
#include "Eagle/Core/Scene.h"

#include <string_view>

namespace Eagle
{
	// ------------------------
	// RendererSettingsSnapshot
	// ------------------------
	namespace
	{
		// The options that aren't edited through the "Renderer Settings" panel: editor preferences, view options, and the renderer's internal state.
		// They're never captured or restored
		void CopyNonPanelOptions(SceneRendererSettings& dst, const SceneRendererSettings& src)
		{
			dst.GridCellSize = src.GridCellSize;
			dst.bEnableDebugLinesDepthTest = src.bEnableDebugLinesDepthTest;
			dst.bVisualizeCascades = src.bVisualizeCascades;
			dst.bVisualizeLightTiles = src.bVisualizeLightTiles;
			dst.InternalState = src.InternalState;
		}

		struct RendererOptionsGroup
		{
			std::string_view Name; // "Edit <Name>"
			bool (*IsSame)(const SceneRendererSettings& a, const SceneRendererSettings& b);
		};

		// In the order of the panel
		constexpr RendererOptionsGroup s_RendererOptionsGroups[] =
		{
			{ "Gamma", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.Gamma == b.Gamma; } },
			{ "Exposure", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.Exposure == b.Exposure; } },
			{ "Tonemapping Method", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.Tonemapping == b.Tonemapping; } },
			{ "Object Picking", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.bEnableObjectPicking == b.bEnableObjectPicking; } },
			{ "2D Object Picking", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.bEnable2DObjectPicking == b.bEnable2DObjectPicking; } },
			{ "Sort Opaque Particles", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.bSortOpaqueParticles == b.bSortOpaqueParticles; } },
			{ "Max Particles Budget", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.MaxParticlesBudget == b.MaxParticlesBudget; } },
			{ "Line Width", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.LineWidth == b.LineWidth; } },
			{ "Transparency Layers", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.TransparencyLayers == b.TransparencyLayers; } },
			{ "Ambient Occlusion", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.AO == b.AO; } },
			{ "Anti-Aliasing", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.AA == b.AA; } },
			{ "Geometric Specular Anti-Aliasing", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.bGeometricSpecularAA == b.bGeometricSpecularAA; } },
			{ "Auto Exposure Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.AutoExposure == b.AutoExposure; } },
			{ "Shadow Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b)
				{
					return a.ShadowsSettings == b.ShadowsSettings && a.bTranslucentShadows == b.bTranslucentShadows
						&& a.bEnableSoftShadows == b.bEnableSoftShadows && a.bEnableCSMSmoothTransition == b.bEnableCSMSmoothTransition;
				} },
			{ "Screen Space Shadows Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.ScreenSpaceShadows == b.ScreenSpaceShadows; } },
			{ "Screen Space Reflections Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.ScreenSpaceReflections == b.ScreenSpaceReflections; } },
			{ "Bloom Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.BloomSettings == b.BloomSettings; } },
			{ "SSAO Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.SSAOSettings == b.SSAOSettings; } },
			{ "GTAO Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.GTAOSettings == b.GTAOSettings; } },
			{ "MSAA Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.MSAAParams == b.MSAAParams; } },
			{ "Volumetric Lights Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.VolumetricSettings == b.VolumetricSettings; } },
			{ "Fog Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.FogSettings == b.FogSettings; } },
			{ "Depth of Field Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.DOFSettings == b.DOFSettings; } },
			{ "Motion Blur Settings", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.MotionBlur == b.MotionBlur; } },
			{ "Photo Linear Tonemapping", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.PhotoLinearTonemappingParams == b.PhotoLinearTonemappingParams; } },
			{ "Filmic Tonemapping", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.FilmicTonemappingParams == b.FilmicTonemappingParams; } },
			{ "AgX Tonemapping", [](const SceneRendererSettings& a, const SceneRendererSettings& b) { return a.AgXTonemappingParams == b.AgXTonemappingParams; } },
			{ "Chromatic Aberration", [](const SceneRendererSettings& a, const SceneRendererSettings& b)
				{
					return a.Lens.bEnableChromaticAberration == b.Lens.bEnableChromaticAberration && a.Lens.ChromaticIntensity == b.Lens.ChromaticIntensity;
				} },
			{ "Vignette", [](const SceneRendererSettings& a, const SceneRendererSettings& b)
				{
					return a.Lens.bEnableVignette == b.Lens.bEnableVignette && a.Lens.VignetteIntensity == b.Lens.VignetteIntensity;
				} },
			{ "Film Grain", [](const SceneRendererSettings& a, const SceneRendererSettings& b)
				{
					return a.Lens.bEnableFilmGrain == b.Lens.bEnableFilmGrain && a.Lens.FilmGrainScale == b.Lens.FilmGrainScale
						&& a.Lens.FilmGrainAmount == b.Lens.FilmGrainAmount && a.Lens.FilmGrainSeedUpdateRate == b.Lens.FilmGrainSeedUpdateRate;
				} },
		};
	}

	RendererSettingsSnapshot RendererSettingsSnapshot::Capture(const Scene& scene)
	{
		RendererSettingsSnapshot snapshot;
		snapshot.Options = scene.GetSceneRenderer()->GetOptions();
		CopyNonPanelOptions(snapshot.Options, SceneRendererSettings{}); // So that they never make snapshots differ
		snapshot.bVSync = Application::Get().GetWindow().IsVSync();
		return snapshot;
	}

	void RendererSettingsSnapshot::Apply(Scene& scene) const
	{
		const Ref<SceneRenderer>& sceneRenderer = scene.GetSceneRenderer();
		const SceneRendererSettings& current = sceneRenderer->GetOptions();

		SceneRendererSettings options = Options;
		CopyNonPanelOptions(options, current);
		if (options != current)
			sceneRenderer->SetOptions(options);

		Window& window = Application::Get().GetWindow();
		if (window.IsVSync() != bVSync)
			window.SetVSync(bVSync);
	}

	// -----------------------
	// RendererSettingsCommand
	// -----------------------
	void RendererSettingsCommand::Undo()
	{
		if (const Ref<Scene> scene = m_Scene.lock())
			m_Before.Apply(*scene);
	}

	void RendererSettingsCommand::Redo()
	{
		if (const Ref<Scene> scene = m_Scene.lock())
			m_After.Apply(*scene);
	}

	bool RendererSettingsCommand::CanApply(bool bUndo) const
	{
		const Ref<Scene> scene = m_Scene.lock();
		return scene && !scene->IsPlaying() && scene->GetSceneRenderer();
	}

	std::string RendererSettingsCommand::Describe(const RendererSettingsSnapshot& before, const RendererSettingsSnapshot& after)
	{
		std::string_view changedGroup;
		uint32_t changedCount = 0u;

		if (before.bVSync != after.bVSync)
		{
			changedGroup = "VSync";
			++changedCount;
		}

		for (const RendererOptionsGroup& group : s_RendererOptionsGroups)
		{
			if (!group.IsSame(before.Options, after.Options))
			{
				changedGroup = group.Name;
				++changedCount;
			}
		}

		if (changedCount == 1u)
			return "Edit " + std::string(changedGroup);
		return "Edit Renderer Settings";
	}

	// ---------------------------
	// RendererSettingsEditTracker
	// ---------------------------
	RendererSettingsEditTracker::RendererSettingsEditTracker()
		: m_Tracker(
			[](const Target& target) -> std::optional<RendererSettingsSnapshot>
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene || scene->IsPlaying() || !scene->GetSceneRenderer())
					return {};
				return RendererSettingsSnapshot::Capture(*scene);
			},
			[](UndoHistory& history, const Target& target, RendererSettingsSnapshot&& before, RendererSettingsSnapshot&& after)
			{
				const Ref<Scene> scene = target.TargetScene.lock();
				if (!scene)
					return;

				std::string name = RendererSettingsCommand::Describe(before, after);
				history.Record(std::move(name), MakeScope<RendererSettingsCommand>(scene, std::move(before), std::move(after)));
			})
	{
	}

	void RendererSettingsEditTracker::BeginFrame(UndoHistory* history, const Ref<Scene>& scene)
	{
		const bool bRecord = scene && !scene->IsPlaying();
		m_Tracker.BeginFrame(bRecord ? history : nullptr, Target{ scene });
	}
}
