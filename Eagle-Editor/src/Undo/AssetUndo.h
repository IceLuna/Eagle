#pragma once

#include "UndoHistory.h"
#include "EditTracker.h"

#include "Eagle/Asset/Asset.h"

namespace Eagle
{
	namespace UndoUtils
	{
		struct NamedChange
		{
			std::string_view Name;
			bool bChanged = false;
		};

		// "Edit <Name>" if exactly one of them changed, otherwise `fallback`.
		// For example, `Describe({ { "Volume", a.Volume != b.Volume }, { "Pitch", a.Pitch != b.Pitch } }, "Edit Sound Group")`
		std::string Describe(std::initializer_list<NamedChange> changes, std::string_view fallback);

		template<typename T>
		size_t GetMemoryUsage(const T& value)
		{
			if constexpr (requires(const T& v) { { v.GetMemoryUsage() } -> std::convertible_to<size_t>; })
				return value.GetMemoryUsage();
			else
				return sizeof(T);
		}
	}

	// Sets an asset to its state before or after a change
	// @StateType. What the change modified
	template<typename AssetType, typename StateType>
	class AssetStateCommand : public UndoCommand
	{
	public:
		using ApplyFn = std::function<void(const Ref<AssetType>& asset, const StateType& state)>;

		AssetStateCommand(const Ref<AssetType>& asset, StateType before, StateType after, ApplyFn apply)
			: m_Asset(asset), m_Before(std::move(before)), m_After(std::move(after)), m_Apply(std::move(apply)) {}

		void Undo() override { Apply(m_Before); }
		void Redo() override { Apply(m_After); }
		bool CanApply(bool bUndo) const override { return !m_Asset.expired(); }
		size_t GetMemoryUsage() const override { return sizeof(*this) + UndoUtils::GetMemoryUsage(m_Before) + UndoUtils::GetMemoryUsage(m_After); }

	private:
		void Apply(const StateType& state)
		{
			if (const Ref<AssetType> asset = m_Asset.lock())
			{
				m_Apply(asset, state);
				asset->SetDirty(true);
				asset->OnModified();
			}
		}

	private:
		Weak<AssetType> m_Asset;
		StateType m_Before;
		StateType m_After;
		ApplyFn m_Apply;
	};

	// For editors whose only settings that are recorded are the common ones (such as "Always build", and the path to the raw file)
	struct NoAssetState
	{
		bool operator==(const NoAssetState&) const { return true; }
	};

	// Turns the edits of an asset, made through the widgets of an asset editor, into undo transactions.
	// Besides the editor-specific state (`StateType`), it records the settings every asset editor has (such as "Always build", and the path to the raw file).
	// The editor-specific state is usually the asset's settings: `AssetType::Settings`, captured with `GetSettings()` and applied with `SetSettings()`.
	// A setting that's added to them is recorded too, so editors don't list what they edit.
	// Usage:
	// - Call `BeginFrame()` in each window of the editor that has widgets that edit the asset, right before they're drawn.
	// - Call `EndFrame()` once per frame, after the editor's windows.
	// - Call `UndoHistory::FlushPendingEdits()` before the editor makes a change that it records on its own (for example, an operation that's too heavy to be captured on every edit).
	// @StateType. Needs `operator==`
	template<typename AssetType, typename StateType>
	class AssetEditTracker
	{
	public:
		// Captures the editor-specific state of the asset
		using CaptureFn = std::function<StateType(const Ref<AssetType>& asset)>;

		// Applies the editor-specific state to the asset
		using ApplyFn = std::function<void(const Ref<AssetType>& asset, const StateType& state)>;

		// Names the edit, for example "Edit Volume". Only called if the editor-specific state was changed
		using DescribeFn = std::function<std::string(const StateType& before, const StateType& after)>;

		struct State
		{
			StateType Data;
			Path PathToRaw;
			bool bAlwaysIncludeInGameBuilds = false;

			size_t GetMemoryUsage() const { return UndoUtils::GetMemoryUsage(Data) + PathToRaw.native().capacity() * sizeof(Path::value_type); }
			bool operator==(const State& other) const { return bAlwaysIncludeInGameBuilds == other.bAlwaysIncludeInGameBuilds && PathToRaw == other.PathToRaw && Data == other.Data; }
		};

		AssetEditTracker(CaptureFn capture, ApplyFn apply, DescribeFn describe)
			: m_Tracker(
				[capture](const Target& target) -> std::optional<State>
				{
					const Ref<AssetType> asset = target.TargetAsset.lock();
					if (!asset)
						return {};

					return State{ capture(asset), asset->GetPathToRaw(), asset->IsAlwaysIncludedInGameBuilds() };
				},
				[apply, describe](UndoHistory& history, const Target& target, State&& before, State&& after)
				{
					const Ref<AssetType> asset = target.TargetAsset.lock();
					if (!asset)
						return;

					std::string name;
					if (!(before.Data == after.Data))
						name = describe(before.Data, after.Data);
					else if (before.PathToRaw != after.PathToRaw && before.bAlwaysIncludeInGameBuilds == after.bAlwaysIncludeInGameBuilds)
						name = "Change Source File";
					else if (before.PathToRaw == after.PathToRaw)
						name = "Edit Always Build";
					else
						name = "Edit Asset Settings";

					history.Record(std::move(name), MakeScope<AssetStateCommand<AssetType, State>>(asset, std::move(before), std::move(after),
						[apply](const Ref<AssetType>& asset, const State& state)
						{
							asset->SetAlwaysIncludedInGameBuilds(state.bAlwaysIncludeInGameBuilds);
							if (asset->GetPathToRaw() != state.PathToRaw)
								asset->SetPathToRaw(state.PathToRaw);
							apply(asset, state.Data);
						}));
				})
		{
		}

		// The state is the asset's settings (see `GetSettings()` and `SetSettings()` of the asset), or `NoAssetState`
		explicit AssetEditTracker(DescribeFn describe)
			: AssetEditTracker(&CaptureSettings, &ApplySettings, std::move(describe))
		{}

		void BeginFrame(UndoHistory* history, const Ref<AssetType>& asset) { m_Tracker.BeginFrame(asset ? history : nullptr, Target{ asset }); }
		void EndFrame() { m_Tracker.EndFrame(); }
		void Reset() { m_Tracker.Reset(); }

	private:
		static StateType CaptureSettings(const Ref<AssetType>& asset)
		{
			if constexpr (std::is_same_v<StateType, NoAssetState>)
				return {};
			else
				return asset->GetSettings();
		}

		static void ApplySettings(const Ref<AssetType>& asset, const StateType& state)
		{
			if constexpr (!std::is_same_v<StateType, NoAssetState>)
				asset->SetSettings(state);
		}

	private:
		struct Target
		{
			Weak<AssetType> TargetAsset;

			bool operator==(const Target& other) const { return !TargetAsset.owner_before(other.TargetAsset) && !other.TargetAsset.owner_before(TargetAsset); }
		};

		EditTracker<Target, State> m_Tracker;
	};
}
