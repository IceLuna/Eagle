#pragma once

#include "SequenceTrack.h"

#include "Eagle/Core/Core.h"
#include "Eagle/Core/GUID.h"
#include "Eagle/Core/Timestep.h"
#include "Eagle/Math/Transform.h"

namespace Eagle
{
	class AssetSceneSequence;
	class Scene;

	enum class SequencePlayState
	{
		Stopped,
		Playing,
		Paused
	};

	class SceneSequencePlayer
	{
	public:
		SceneSequencePlayer() = default;

		void SetAsset(const Ref<AssetSceneSequence>& asset);
		const Ref<AssetSceneSequence>& GetAsset() const { return m_Asset; }

		void Play();
		void Pause();
		void Stop();

		SequencePlayState GetState() const { return m_State; }
		bool IsPlaying() const { return m_State == SequencePlayState::Playing; }
		bool IsStopped() const { return m_State == SequencePlayState::Stopped; }

		// Current playhead position in seconds
		float GetTime() const { return m_Time; }
		void SetTime(float time);

		float GetPlayRate() const { return m_PlayRate; }
		void SetPlayRate(float rate) { m_PlayRate = rate; }

		// When unset, the sequence asset's own loop flag is used
		void SetLoopOverride(bool bLoop, bool bUseOverride = true)
		{
			bLoopOverride = bLoop;
			bUseLoopOverride = bUseOverride;
		}
		void ClearLoopOverride() { bUseLoopOverride = false; }
		bool IsLooping() const;

		float GetDuration() const;

		// Advances the playhead. Does nothing unless the player is in the Playing state.
		void Update(Timestep ts);

		// Events passed by the last `Update`
		// Events are produced here rather than while evaluating, because a sequence is evaluated
		// several times per frame (preview, scrubbing, camera keying) and by every player that shares
		// the asset.
		const std::vector<SequenceEvent>& GetFiredEvents() const { return m_FiredEvents; }

		// Time window the last `Update` moved through. Invalid unless playback actually advanced
		const SequenceEventWindow& GetEventWindow() const { return m_EventWindow; }

		// Samples every track at the current time.
		// @base. Transform the sequence plays relative to
		void Evaluate(Scene* scene, const Transform& base, SequenceEvalContext* outContext) const;

		// Samples the sequence and, if a camera track produced a pose, asks the scene to render
		// through it. Safe to call every frame; it's a no-op when there's no camera data.
		// @owner. Identifies the requester so that releasing the override can't steal it from someone else.
		// Returns true if the scene camera is now being driven by this player.
		// @bApplyCamera. When false, doesn't control the camera, only post processing is applied
		bool ApplyToScene(Scene* scene, const GUID& owner, const Transform& base, bool bApplyCamera = true) const;

		// Fired when a non-looping sequence reaches its end
		void SetOnFinishedCallback(const std::function<void()>& callback) { m_OnFinished = callback; }

		// Hands the camera back. Safe to call even when this player never took it over.
		static void ReleaseScene(Scene* scene, const GUID& owner);

	private:
		Ref<AssetSceneSequence> m_Asset;
		std::function<void()> m_OnFinished;

		float m_Time = 0.f;
		float m_PlayRate = 1.f;

		SequencePlayState m_State = SequencePlayState::Stopped;

		std::vector<SequenceEvent> m_FiredEvents;
		SequenceEventWindow m_EventWindow;

		// Set by anything that moves the playhead without playing (Play, Stop, SetTime, SetAsset, etc...).
		// The next update then starts a fresh window instead of treating the jump as travelled time,
		// which would re-fire every event in between
		bool bResetEventWindow = true;

		bool bLoopOverride = false;
		bool bUseLoopOverride = false;
	};
}
