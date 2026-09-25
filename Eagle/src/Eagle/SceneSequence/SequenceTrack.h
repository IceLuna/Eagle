#pragma once

#include "SequenceCurve.h"

#include "Eagle/Renderer/PostProcessOverride.h"
#include "Eagle/Core/Core.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Math/Transform.h"

#include <string>
#include <vector>

namespace Eagle
{
	class Scene;

	// Adding one means:
	//   1) a new class deriving from `SequenceTrack`
	//   2) a case in `SequenceTrack::Create`
	//   3) serialize/deserialize in `Serializer::(De)SerializeAssetSceneSequence`
	//   4) channel views in `SceneSequenceAssetEditor::BuildChannelViews`

	enum class SequenceTrackType
	{
		Camera,

		// Chooses which camera track is live at any time
		CameraCuts,

		// Overrides rendering settings (exposure, bloom, fog, DoF, ...) while it's active
		PostProcess,

		// Can be used to trigger a callback on scripts side
		Event,
	};

	// The camera state produced by a camera track for one point in time
	struct SequenceCameraState
	{
		Transform WorldTransform;
		float VerticalFOVRadians = glm::radians(45.f);
		float NearClip = 0.01f;
		float FarClip = 150.f;
		bool bValid = false;
	};

	struct SequenceEvent
	{
		std::string Name;
		float Time = 0.f;
	};

	struct SequenceEventData
	{
		uint32_t EntityID = 0;
		std::vector<SequenceEvent> Events;
	};

	// The slice of time playback moved through during a single update. Event keys inside it fire.
	// The window is (From, To] going forward and [To, From) going backwards, so a key that
	// sits exactly on a frame boundary fires once and only once, no matter how the boundaries line up.
	// `bIncludeFrom` opens the near end for the first update after starting playback, so a key at the
	// very start of the sequence isn't skipped.
	struct SequenceEventWindow
	{
		float From = 0.f;
		float To = 0.f;
		bool bValid = false;
		bool bForward = true;
		bool bWrapped = false;
		bool bIncludeFrom = false;

		bool Contains(float time) const
		{
			if (!bValid)
				return false;

			if (bForward)
			{
				const bool bAfterStart = bIncludeFrom ? time >= From : time > From;
				return bWrapped ? (bAfterStart || time <= To) : (bAfterStart && time <= To);
			}

			const bool bBeforeStart = bIncludeFrom ? time <= From : time < From;
			return bWrapped ? (bBeforeStart || time >= To) : (bBeforeStart && time >= To);
		}

		float GetTravelDistance(float time, float duration) const
		{
			const float distance = bForward ? time - From : From - time;
			return (bWrapped && distance < 0.f) ? distance + duration : distance;
		}
	};

	struct SequenceEvalContext
	{
		// The scene being driven. May be null when a sequence is evaluated purely for preview
		Scene* SceneRef = nullptr;

		// Transform the whole sequence is played relative to.
		Transform Base;

		float Time = 0.f;

		// Written by camera tracks. The last enabled camera track wins.
		SequenceCameraState Camera;

		// Written by post process tracks
		PostProcessOverride PostProcess;

		// Future track types add their outputs here
	};

	class SequenceTrack
	{
	public:
		virtual ~SequenceTrack() = default;

		virtual SequenceTrackType GetType() const = 0;

		// Writes this track's contribution at `context.Time` into `context`
		virtual void Evaluate(SequenceEvalContext& context) const = 0;

		// Time of the last key on the track. Used to auto-fit the sequence duration.
		virtual float GetLastKeyTime() const = 0;

		virtual Ref<SequenceTrack> Clone() const = 0;

		// Shifts every key on the track by `deltaTime` seconds
		virtual void ShiftKeys(float deltaTime) = 0;

		const GUID& GetID() const { return m_ID; }
		void SetID(const GUID& id) { m_ID = id; }

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		bool IsEnabled() const { return bEnabled; }
		void SetEnabled(bool bValue) { bEnabled = bValue; }

		// Creates an empty track of the requested type
		static Ref<SequenceTrack> Create(SequenceTrackType type);

	protected:
		SequenceTrack(const std::string& name) : m_Name(name) {}

		void CopyBaseTo(SequenceTrack& other) const
		{
			other.m_ID = m_ID;
			other.m_Name = m_Name;
			other.bEnabled = bEnabled;
		}

	protected:
		GUID m_ID;
		std::string m_Name = "Track";
		bool bEnabled = true;
	};

	// Drives the camera used to render the scene.
	class SequenceCameraTrack : public SequenceTrack
	{
	public:
		SequenceCameraTrack(const std::string& name = "Camera") : SequenceTrack(name) {}

		SequenceTrackType GetType() const override { return SequenceTrackType::Camera; }

		void Evaluate(SequenceEvalContext& context) const override;

		float GetLastKeyTime() const override
		{
			float result = 0.f;
			result = glm::max(result, m_Location.GetLastKeyTime());
			result = glm::max(result, m_Rotation.GetLastKeyTime());
			result = glm::max(result, m_FOV.GetLastKeyTime());
			return result;
		}

		Ref<SequenceTrack> Clone() const override;

		void ShiftKeys(float deltaTime) override;

		void AddTransformKey(float time, const Transform& transform, SequenceInterpolation interpolation = SequenceInterpolation::Smooth)
		{
			m_Location.AddKey(time, transform.Location, interpolation);
			m_Rotation.AddKey(time, transform.Rotation.GetQuat(), interpolation);
		}

		// Samples the track's pose ignoring the sequence base transform
		Transform EvaluateLocalTransform(float time) const
		{
			Transform result;
			result.Location = m_Location.Evaluate(time, glm::vec3(0.f));
			result.Rotation = m_Rotation.Evaluate(time, glm::quat(1.f, 0.f, 0.f, 0.f));
			result.Scale3D = glm::vec3(1.f);
			return result;
		}

		bool HasTransformKeys() const { return !m_Location.IsEmpty() || !m_Rotation.IsEmpty(); }

		// Time span covered by the location/rotation keys. Returns false if there are none
		bool GetShotRange(float* outStart, float* outEnd) const;

		SequenceVec3Channel& GetLocationChannel() { return m_Location; }
		const SequenceVec3Channel& GetLocationChannel() const { return m_Location; }

		SequenceQuatChannel& GetRotationChannel() { return m_Rotation; }
		const SequenceQuatChannel& GetRotationChannel() const { return m_Rotation; }

		// In degrees
		SequenceFloatChannel& GetFOVChannel() { return m_FOV; }
		const SequenceFloatChannel& GetFOVChannel() const { return m_FOV; }

		float GetDefaultFOVDegrees() const { return m_DefaultFOVDegrees; }
		void SetDefaultFOVDegrees(float value) { m_DefaultFOVDegrees = value; }

		float GetNearClip() const { return m_NearClip; }
		void SetNearClip(float value) { m_NearClip = glm::max(0.001f, value); }

		float GetFarClip() const { return m_FarClip; }
		void SetFarClip(float value) { m_FarClip = glm::max(m_NearClip + 0.001f, value); }

		// Samples the path into a polyline for debug drawing in the viewport.
		// @segmentsPerSecond. Sampling density; higher gives a smoother curve
		void GatherPathPoints(const Transform& base, float fromTime, float toTime, uint32_t segmentsPerSecond, std::vector<glm::vec3>& outPoints) const;

	private:
		SequenceVec3Channel m_Location;
		SequenceQuatChannel m_Rotation;
		SequenceFloatChannel m_FOV;

		// Used when the FOV channel has no keys
		float m_DefaultFOVDegrees = 45.f;

		float m_NearClip = 0.01f;
		float m_FarClip = 150.f;
	};

	// Decides which camera track the scene renders through.
	// Before the first cut, the first cut's camera is live.
	// Without a cuts track (or with an empty one) the sequence picks the camera automatically,
	// see `AssetSceneSequence::ResolveActiveCamera`.
	class SequenceCameraCutTrack : public SequenceTrack
	{
	public:
		SequenceCameraCutTrack(const std::string& name = "Camera Cuts") : SequenceTrack(name) {}

		SequenceTrackType GetType() const override { return SequenceTrackType::CameraCuts; }

		// Writes nothing by itself since it doesn't have access to all the tracks.
		// `AssetSceneSequence` reads the cuts to decide which camera track to evaluate
		void Evaluate(SequenceEvalContext&) const override {}

		float GetLastKeyTime() const override { return m_Cuts.GetLastKeyTime(); }

		Ref<SequenceTrack> Clone() const override;

		void ShiftKeys(float deltaTime) override;

		void AddCut(float time, const GUID& cameraTrackID) { m_Cuts.AddKey(time, cameraTrackID, SequenceInterpolation::Constant); }

		// ID of the camera track that's live at `time`. Null if there are no cuts
		GUID GetCameraAt(float time) const { return m_Cuts.Evaluate(time, GUID(0, 0)); }

		SequenceGUIDChannel& GetCutsChannel() { return m_Cuts; }
		const SequenceGUIDChannel& GetCutsChannel() const { return m_Cuts; }

	private:
		SequenceGUIDChannel m_Cuts; // ID of a camera track
	};

	// Overrides rendering settings for as long as it's active.
	// Active means "between its first and last key" (or the whole sequence, if `bApplyWholeSequence`),
	// and, when it's linked to a camera track, only while that camera is live. Leaving the track restores
	// whatever the scene's own settings say, because a property that nothing keys is simply not overridden.
	// Several tracks can coexist. When two active tracks key the same property, the one further down the track list wins.
	class SequencePostProcessTrack : public SequenceTrack
	{
	public:
		// One animated rendering property. The channel type is decided by the property's registry entry
		struct Channel
		{
			PostProcessProperty Property = PostProcessProperty::Exposure;
			SequenceChannelVariant Data;
			bool bEnabled = true;

			float GetFirstKeyTime() const;
			float GetLastKeyTime() const;
			size_t GetKeysCount() const;
			void Shift(float deltaTime);

			bool Evaluate(float time, PostProcessValue* outValue) const;
		};

		SequencePostProcessTrack(const std::string& name = "Post Process") : SequenceTrack(name) {}

		SequenceTrackType GetType() const override { return SequenceTrackType::PostProcess; }

		// Writes every keyed property into `context.PostProcess`. Whether this track is active at all
		// is decided by `AssetSceneSequence`, which knows the live camera
		void Evaluate(SequenceEvalContext& context) const override;

		float GetLastKeyTime() const override;

		Ref<SequenceTrack> Clone() const override;
		void ShiftKeys(float deltaTime) override;

		// Time span this track affects. False when it has no keys at all
		bool GetActiveRange(float* outStart, float* outEnd) const;
		bool IsActiveAt(float time) const;

		// Null: the track applies whenever it's in range, whatever camera is live.
		// Otherwise it only applies while that camera track is the live one
		const GUID& GetCameraTrackID() const { return m_CameraTrackID; }
		void SetCameraTrackID(const GUID& id) { m_CameraTrackID = id; }

		bool DoesApplyWholeSequence() const { return bApplyWholeSequence; }
		void SetApplyWholeSequence(bool bValue) { bApplyWholeSequence = bValue; }

		// Adds a channel for `property` if there isn't one yet
		Channel& AddProperty(PostProcessProperty property);
		bool RemoveProperty(PostProcessProperty property);
		Channel* FindChannel(PostProcessProperty property);
		const Channel* FindChannel(PostProcessProperty property) const;
		bool HasProperty(PostProcessProperty property) const { return FindChannel(property) != nullptr; }

		std::vector<Channel>& GetChannels() { return m_Channels; }
		const std::vector<Channel>& GetChannels() const { return m_Channels; }

	private:
		std::vector<Channel> m_Channels;
		GUID m_CameraTrackID = GUID(0, 0);
		bool bApplyWholeSequence = false;
	};

	class SequenceEventTrack : public SequenceTrack
	{
	public:
		SequenceEventTrack(const std::string& name = "Events") : SequenceTrack(name) {}

		SequenceTrackType GetType() const override { return SequenceTrackType::Event; }

		// Nothing to evaluate: an event track has no value at a given time, it only fires as
		// playback passes its keys. See `GatherEvents`
		void Evaluate(SequenceEvalContext&) const override {}

		float GetLastKeyTime() const override { return m_Events.GetLastKeyTime(); }

		Ref<SequenceTrack> Clone() const override;

		void ShiftKeys(float deltaTime) override;

		// Appends every event key inside `window`. The track holds no playback state of its own:
		// the window comes from the player, so several players (and the editor's preview) can play
		// the same asset without interfering with each other
		void GatherEvents(const SequenceEventWindow& window, std::vector<SequenceEvent>& outEvents) const;

		SequenceStringChannel& GetEventsChannel() { return m_Events; }
		const SequenceStringChannel& GetEventsChannel() const { return m_Events; }

	private:
		SequenceStringChannel m_Events;
	};
}
