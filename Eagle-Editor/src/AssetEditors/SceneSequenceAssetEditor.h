#pragma once

#include "AssetEditor.h"

#include "Eagle/SceneSequence/SceneSequencePlayer.h"

#include <any>
#include <string>
#include <vector>

namespace Eagle
{
	class AssetSceneSequence;
	class Scene;

	// Type-erased view over a single `SequenceChannel<T>`.
	// The timeline, the details panel and the curve editor only ever talk to this interface.
	// That's what keeps the editor extensible: supporting a new track type means producing views for its channels in
	// `SceneSequenceAssetEditor::BuildChannelViews`. Also, `DrawTrackDetails` needs to be updated if necessary.
	class SequenceChannelView
	{
	public:
		SequenceChannelView(const std::string& name, ImU32 color) : m_Name(name), m_Color(color) {}
		virtual ~SequenceChannelView() = default;

		const std::string& GetName() const { return m_Name; }
		ImU32 GetColor() const { return m_Color; }

		virtual size_t GetKeysCount() const = 0;
		virtual GUID GetKeyID(size_t index) const = 0;
		virtual float GetKeyTime(size_t index) const = 0;
		virtual SequenceInterpolation GetKeyInterpolation(size_t index) const = 0;
		virtual bool FindKeyIndex(const GUID& id, size_t* outIndex) const = 0;

		// Doesn't re-sort, call `Sort()` once all times are updated
		virtual void SetKeyTime(const GUID& id, float time) = 0;
		virtual void Sort() = 0;

		// Switching to Cubic seeds the tangents from the current curve so nothing jumps
		virtual void SetKeyInterpolation(const GUID& id, SequenceInterpolation interpolation) = 0;

		// Sets both tangents of a Cubic key to zero
		virtual void FlattenTangents(const GUID& id) = 0;

		virtual bool RemoveKey(const GUID& id) = 0;

		// Adds (or overwrites) a key at `time` holding the channel's current value there.
		// Returns the key's ID.
		virtual GUID AddKeyAtTime(float time) = 0;

		virtual std::any CopyKey(const GUID& id) const = 0;
		// Returns a null GUID when `data` holds a key of a different value type
		virtual GUID PasteKey(const std::any& data, float time) = 0;

		// Returns true if changed
		virtual bool DrawKeyValue(const GUID& id) = 0;

		// Text drawn next to the key in the timeline
		virtual std::string GetKeyLabel(size_t index) const { return {}; }

		// False for channels whose keys never blend (for example, camera cuts).
		// UI then hides interpolation options
		virtual bool IsInterpolationEditable() const { return true; }

		// Channels that can be switched off individually (for example, post process properties).
		// A disabled channel contributes nothing, as if it weren't on the track at all
		virtual bool CanBeDisabled() const { return false; }
		virtual bool IsEnabled() const { return true; }
		virtual void SetEnabled(bool) {}

		// Number of scalar curves this channel can be shown as. 0 hides it from the curve editor
		virtual uint32_t GetCurveComponentsCount() const = 0;
		virtual const char* GetCurveComponentName(uint32_t component) const = 0;
		virtual float EvaluateComponent(float time, uint32_t component) const = 0;
		virtual float GetKeyComponent(size_t index, uint32_t component) const = 0;
		virtual void SetKeyComponent(const GUID& id, uint32_t component, float value) = 0;

		// Effective tangent (units per second) as used by evaluation, whatever the key's mode
		virtual float GetKeyTangent(size_t index, uint32_t component, bool bOut) const = 0;
		// Converts the key to Cubic if needed
		virtual void SetKeyTangent(const GUID& id, uint32_t component, bool bOut, float value) = 0;

	protected:
		std::string m_Name;
		ImU32 m_Color;
	};

	class SceneSequenceAssetEditor : public AssetEditor
	{
	public:
		SceneSequenceAssetEditor(const Ref<AssetSceneSequence>& asset);
		~SceneSequenceAssetEditor() override;

		void OnImGuiRender(bool* pOpen) override;

		const Ref<Asset> GetAsset() const override { return Cast<Asset>(m_Asset); }

	private:
		// Identifies one key. IDs rather than indices, so it survives keys being re-sorted mid-drag
		struct KeyRef
		{
			GUID TrackID = GUID(0, 0);
			uint32_t ChannelIndex = 0;
			GUID KeyID = GUID(0, 0);

			bool operator==(const KeyRef& other) const
			{
				return TrackID == other.TrackID && ChannelIndex == other.ChannelIndex && KeyID == other.KeyID;
			}
		};

		struct TrackViews
		{
			Ref<SequenceTrack> Track;
			std::vector<Scope<SequenceChannelView>> Channels;
		};

		// One visual row in the timeline
		struct RowLayout
		{
			float MinY = 0.f;
			float MaxY = 0.f;
			uint32_t TrackIndex = 0;
			int32_t ChannelIndex = -1; // -1: track header (summary) row
		};

		struct PendingPropertyOp
		{
			GUID TrackID = GUID(0, 0);
			PostProcessProperty Property = PostProcessProperty::Exposure;
			float Time = 0.f;
			bool bAdd = true;
		};

		struct ClipboardEntry
		{
			GUID TrackID = GUID(0, 0);
			uint32_t ChannelIndex = 0;
			float RelativeTime = 0.f; // Relative to the earliest copied key
			std::any Key;
		};

		enum class DragMode
		{
			None,
			MoveKeys,
			BoxSelect,
			Pan,
			Playhead,
			Duration
		};

	private:
		// ---- Layout ----
		void DrawToolbar();
		void DrawTimeline();
		void DrawRuler(float width);
		void DrawRows(float width, float height);
		void DrawDetailsTab();
		void DrawCurvesTab();
		void DrawKeyDetails();
		void DrawTrackDetails();
		void DrawAddTrackPopup();
		// Left region: the timeline and the curve editor, as two tabs
		void DrawTracksAndCurves();

		// ---- Interaction ----
		void HandleShortcuts();
		void HandleRowsMouse(const ImVec2& areaMin, const ImVec2& areaMax);
		void HandleZoomPan(bool bHovered);
		void UpdatePreview(bool bWindowVisible);

		// ---- Editing ----
		void BuildChannelViews();
		void PruneSelection();
		void AddTrack(SequenceTrackType type);
		void DuplicateTrack(const GUID& trackID);
		void DeleteTrack(const GUID& trackID);
		void BeginRenameTrack(const GUID& trackID);

		// Cuts to `cameraTrackID` at `time`, creating the Camera Cuts track if there isn't one yet
		void AddCameraCut(float time, const GUID& cameraTrackID);

		// Adds a rendering property to a post process track, keyed at `time` with whatever the scene
		// currently renders with, so the first key never changes the look on its own
		// Adding or removing a property resizes the track's channel vector, which invalidates every
		// channel pointer the views hold. Both are therefore queued and applied once the whole frame's
		// UI has been submitted, after which the views are rebuilt
		void RequestAddPostProcessProperty(const GUID& trackID, PostProcessProperty property, float time);
		void RequestRemovePostProcessProperty(const GUID& trackID, PostProcessProperty property);
		void ApplyPendingPropertyOps();
		void AddPostProcessProperty(const GUID& trackID, PostProcessProperty property, float time);
		void DrawAddPostProcessPropertyMenu(const GUID& trackID, float time);

		Ref<SequenceCameraCutTrack> FindCameraCutTrack() const;
		// Keys location + rotation at `time` with what the user is currently looking at:
		// the sequence camera if we're previewing through it, the editor camera otherwise.
		// Creates a camera track if `track` is null.
		void KeyCameraFromViewport(const Ref<SequenceCameraTrack>& track, float time);
		void SnapPilotCameraToPlayhead();
		void DeleteSelectedKeys();
		void CopySelectedKeys();
		void PasteKeys(float atTime);
		void SetSelectedInterpolation(SequenceInterpolation interpolation);
		void AddKeysAtTime(uint32_t trackIndex, int32_t channelIndex, float time);
		void FitViewToContent();
		void ComputeTickSteps(float* outMajor, float* outMinor) const;
		void ResolveOverlapsAfterMove();
		bool IsTrackExpanded(const GUID& trackID) const;
		void SetTrackExpanded(const GUID& trackID, bool bExpanded);
		// Keys of `row` within `radius` pixels of `mouseX`. For a summary row, every key of the track at that time
		bool HitTestKeys(const RowLayout& row, float mouseX, float radius, std::vector<KeyRef>& outRefs) const;
		void MarkDirty();

		void StartPiloting();
		void StopPiloting();

		// ---- Selection ----
		bool IsSelected(const KeyRef& ref) const;
		void Select(const KeyRef& ref, bool bAdditive);
		void Deselect(const KeyRef& ref);
		void ClearSelection() { m_Selection.clear(); }
		// All keys of a track sitting at `time` (what a summary-row key stands for)
		void GatherTrackKeysAtTime(uint32_t trackIndex, float time, std::vector<KeyRef>& outRefs) const;

		// ---- Lookups ----
		SequenceChannelView* FindView(const GUID& trackID, uint32_t channelIndex) const;
		int32_t FindTrackIndex(const GUID& trackID) const;
		Ref<SequenceCameraTrack> GetTargetCameraTrack() const;

		// Time of the last key across every track. Used by the editor's "fit duration" action.
		float GetLastKeyTime() const;

		// ---- Helpers ----
		float TimeToX(float time) const { return m_TimelineOriginX + (time - m_ViewStart) * m_PixelsPerSecond; }
		float XToTime(float x) const { return m_ViewStart + (x - m_TimelineOriginX) / m_PixelsPerSecond; }
		float SnapTime(float time) const;
		float GetFrameDuration() const;
		void SetPlayheadTime(float time);
		std::string FormatTime(float time) const;

		// The transform the sequence is previewed relative to (see "Preview Context")
		Transform GetPreviewBase() const;
		Ref<Scene> GetEditorScene() const;

	private:
		Ref<AssetSceneSequence> m_Asset;
		SceneSequencePlayer m_Player;
		std::string m_WindowName;

		std::vector<TrackViews> m_TrackViews;
		std::vector<RowLayout> m_Rows;
		std::vector<KeyRef> m_Selection;
		std::vector<ClipboardEntry> m_Clipboard;
		std::vector<PendingPropertyOp> m_PendingPropertyOps;
		std::vector<GUID> m_ExpandedTracks;

		GUID m_SelectedTrackID = GUID(0, 0);

		// Inline track renaming in the track list
		GUID m_RenamingTrackID = GUID(0, 0);
		std::string m_RenameBuffer;
		bool bRenameFocusPending = false;

		// Owner ID used for the scene camera override while previewing
		GUID m_PreviewOwnerID;
		std::weak_ptr<Scene> m_PreviewScene;

		// Entity whose `SceneSequenceComponent` provides the base transform for previewing.
		// Null means "world origin"
		GUID m_PreviewContextEntity = GUID(0, 0);
		bool bPreviewContextInitialized = false;

		// ---- Timeline view ----
		float m_ViewStart = -0.25f;       // Time at the left edge, in seconds
		float m_PixelsPerSecond = 120.f;
		float m_TimelineOriginX = 0.f;    // Screen X of time == m_ViewStart
		float m_TimelineWidth = 1.f;
		float m_TrackListWidth = 230.f;
		float m_SidePanelWidth = 380.f;   // Width of the Details/Curves panel on the right (resizable)

		// ---- Drag state ----
		DragMode m_DragMode = DragMode::None;
		ImVec2 m_DragStartMouse = ImVec2(0.f, 0.f);
		std::vector<std::pair<KeyRef, float>> m_DragOriginalTimes;
		float m_DragAnchorTime = 0.f;
		std::vector<KeyRef> m_ClickedKeys;   // What was under the mouse at press
		bool bDragMoved = false;
		bool bReduceSelectionOnRelease = false;

		// Context menu target
		RowLayout m_ContextRow;
		float m_ContextTime = 0.f;
		std::vector<KeyRef> m_ContextKeys;

		// Curve editor
		std::vector<bool> m_CurveComponentVisible;
		bool bFitCurvesRequested = true;
		GUID m_CurvesTrackID = GUID(0, 0); // Track the curve editor was last fitted to

		// ---- Options ----
		bool bPreviewInViewport = true;
		bool bPiloting = false;
		bool bAutoKey = false;
		bool bSnapToFrames = true;
		bool bShowFrames = false;
		bool bLoopPlayback = true;
		bool bWasPlaying = false;

		Transform m_LastPilotTransform;

	};
}
