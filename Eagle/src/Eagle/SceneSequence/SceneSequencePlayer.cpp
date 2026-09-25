#include "egpch.h"
#include "SceneSequencePlayer.h"

#include "Eagle/Asset/Asset.h"
#include "Eagle/Core/Scene.h"
#include "Eagle/Math/Math.h"

namespace Eagle
{
	void SceneSequencePlayer::SetAsset(const Ref<AssetSceneSequence>& asset)
	{
		m_Asset = asset;
		m_Time = 0.f;
		m_State = SequencePlayState::Stopped;

		m_FiredEvents.clear();
		m_EventWindow = {};
		bResetEventWindow = true;
	}

	void SceneSequencePlayer::Play()
	{
		if (!m_Asset)
			return;

		if (m_State == SequencePlayState::Stopped)
		{
			const float duration = GetDuration();
			if (m_PlayRate >= 0.f && m_Time >= duration)
				m_Time = 0.f;
			else if (m_PlayRate < 0.f && m_Time <= 0.f)
				m_Time = duration;
		}

		bResetEventWindow = true;
		m_State = SequencePlayState::Playing;
	}

	void SceneSequencePlayer::Pause()
	{
		if (m_State == SequencePlayState::Playing)
			m_State = SequencePlayState::Paused;
	}

	void SceneSequencePlayer::Stop()
	{
		m_State = SequencePlayState::Stopped;
		m_Time = 0.f;

		m_FiredEvents.clear();
		m_EventWindow = {};
		bResetEventWindow = true;
	}

	void SceneSequencePlayer::SetTime(float time)
	{
		m_Time = glm::clamp(time, 0.f, GetDuration());

		// Seeking skips over time rather than playing through it, so nothing in between fires
		bResetEventWindow = true;
	}

	bool SceneSequencePlayer::IsLooping() const
	{
		if (bUseLoopOverride)
			return bLoopOverride;

		return m_Asset ? m_Asset->IsLooping() : false;
	}

	float SceneSequencePlayer::GetDuration() const
	{
		return m_Asset ? m_Asset->GetDuration() : 0.f;
	}

	void SceneSequencePlayer::Update(Timestep ts)
	{
		m_FiredEvents.clear();
		m_EventWindow = {};

		if (m_State != SequencePlayState::Playing || !m_Asset)
			return;

		const float duration = GetDuration();
		if (duration <= 0.f)
			return;

		const float previousTime = m_Time;
		m_Time += float(ts) * m_PlayRate;

		bool bWrapped = false;
		if (m_PlayRate >= 0.f)
		{
			if (m_Time >= duration)
			{
				if (IsLooping())
				{
					m_Time = glm::mod(m_Time, duration);
					bWrapped = true;
				}
				else
				{
					m_Time = duration;
					m_State = SequencePlayState::Stopped;

					if (m_OnFinished)
						m_OnFinished();
				}
			}
		}
		else
		{
			if (m_Time <= 0.f)
			{
				if (IsLooping())
				{
					// Landing exactly on 0 isn't a wrap: the playhead is still inside the sequence,
					// and calling it a wrap would make this frame's window cover every key
					if (m_Time < 0.f)
					{
						m_Time = glm::mod(m_Time, duration);
						bWrapped = true;
					}
				}
				else
				{
					m_Time = 0.f;
					m_State = SequencePlayState::Stopped;

					if (m_OnFinished)
						m_OnFinished();
				}
			}
		}

		if (m_PlayRate == 0.f)
			return;

		m_EventWindow.bValid = true;
		m_EventWindow.From = previousTime;
		m_EventWindow.To = m_Time;
		m_EventWindow.bForward = m_PlayRate > 0.f;
		m_EventWindow.bWrapped = bWrapped;
		m_EventWindow.bIncludeFrom = bResetEventWindow;
		bResetEventWindow = false;

		m_Asset->GatherEvents(m_EventWindow, m_FiredEvents);
	}

	void SceneSequencePlayer::Evaluate(Scene* scene, const Transform& base, SequenceEvalContext* outContext) const
	{
		if (!outContext)
			return;

		*outContext = SequenceEvalContext{};
		outContext->SceneRef = scene;
		outContext->Base = base;
		outContext->Time = m_Time;

		if (m_Asset)
			m_Asset->Evaluate(*outContext);
	}

	bool SceneSequencePlayer::ApplyToScene(Scene* scene, const GUID& owner, const Transform& base, bool bApplyCamera) const
	{
		if (!scene || !m_Asset)
			return false;

		SequenceEvalContext context;
		Evaluate(scene, base, &context);

		if (context.PostProcess.IsEmpty())
			scene->ClearPostProcessOverride(owner);
		else
			scene->SetPostProcessOverride(owner, context.PostProcess);

		if (!bApplyCamera)
		{
			scene->ClearCameraOverride(owner);
			return false;
		}

		if (!context.Camera.bValid)
			return false;

		Scene::CameraOverrideData data;
		data.WorldTransform = context.Camera.WorldTransform;
		data.VerticalFOVRadians = context.Camera.VerticalFOVRadians;
		data.NearClip = context.Camera.NearClip;
		data.FarClip = context.Camera.FarClip;

		scene->SetCameraOverride(owner, data);
		return true;
	}

	void SceneSequencePlayer::ReleaseScene(Scene* scene, const GUID& owner)
	{
		if (scene)
		{
			scene->ClearCameraOverride(owner);
			scene->ClearPostProcessOverride(owner);
		}
	}
}
