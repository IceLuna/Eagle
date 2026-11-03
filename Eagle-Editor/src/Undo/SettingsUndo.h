#pragma once

#include "UndoHistory.h"
#include "EditTracker.h"

#include "Eagle/Renderer/RendererUtils.h"

#include <functional>

namespace Eagle
{
	class Scene;

	// Sets a value to what it was before or after a change. For simple values that are changed in one place (for example, a flag of an asset)
	template<typename T>
	class ValueCommand : public UndoCommand
	{
	public:
		using SetFn = std::function<void(const T& value)>;
		using CanApplyFn = std::function<bool()>;

		// @set. Sets the value
		// @canApply. Optional. Whether the value can still be set (for example, whether its owner still exists)
		ValueCommand(T before, T after, SetFn set, CanApplyFn canApply = {})
			: m_Before(std::move(before)), m_After(std::move(after)), m_Set(std::move(set)), m_CanApply(std::move(canApply)) {}

		void Undo() override { m_Set(m_Before); }
		void Redo() override { m_Set(m_After); }
		bool CanApply(bool bUndo) const override { return !m_CanApply || m_CanApply(); }
		size_t GetMemoryUsage() const override { return sizeof(*this); }

	private:
		T m_Before;
		T m_After;
		SetFn m_Set;
		CanApplyFn m_CanApply;
	};

	// -----------------
	// Renderer settings
	// -----------------
	struct RendererSettingsSnapshot
	{
		SceneRendererSettings Options;
		bool bVSync = false;

		static RendererSettingsSnapshot Capture(const Scene& scene);
		void Apply(Scene& scene) const;

		bool operator==(const RendererSettingsSnapshot& other) const = default;
	};

	class RendererSettingsCommand : public UndoCommand
	{
	public:
		RendererSettingsCommand(const Ref<Scene>& scene, RendererSettingsSnapshot before, RendererSettingsSnapshot after)
			: m_Scene(scene), m_Before(std::move(before)), m_After(std::move(after)) {}

		void Undo() override;
		void Redo() override;
		bool CanApply(bool bUndo) const override;
		size_t GetMemoryUsage() const override { return sizeof(*this); }

		// For example, "Edit Bloom Settings", "Edit Gamma", or "Edit Renderer Settings" if several groups were changed
		static std::string Describe(const RendererSettingsSnapshot& before, const RendererSettingsSnapshot& after);

	private:
		Weak<Scene> m_Scene;
		RendererSettingsSnapshot m_Before;
		RendererSettingsSnapshot m_After;
	};

	// Turns the edits of the renderer settings, made through the widgets of a panel, into undo transactions
	class RendererSettingsEditTracker
	{
	public:
		RendererSettingsEditTracker();

		// Call within the panel's window, right before its widgets are drawn
		void BeginFrame(UndoHistory* history, const Ref<Scene>& scene);

		// Call every frame, after the panel (whether it was drawn or not)
		void EndFrame() { m_Tracker.EndFrame(); }

		void Reset() { m_Tracker.Reset(); }

	private:
		struct Target
		{
			Weak<Scene> TargetScene;

			bool operator==(const Target& other) const { return !TargetScene.owner_before(other.TargetScene) && !other.TargetScene.owner_before(TargetScene); }
		};

		EditTracker<Target, RendererSettingsSnapshot> m_Tracker;
	};
}
