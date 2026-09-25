#include "egpch.h"
#include "SequenceTrack.h"

namespace Eagle
{
	Ref<SequenceTrack> SequenceTrack::Create(SequenceTrackType type)
	{
		switch (type)
		{
			case SequenceTrackType::Camera:
				return MakeRef<SequenceCameraTrack>();
			case SequenceTrackType::CameraCuts:
				return MakeRef<SequenceCameraCutTrack>();
			case SequenceTrackType::PostProcess:
				return MakeRef<SequencePostProcessTrack>();
			case SequenceTrackType::Event:
				return MakeRef<SequenceEventTrack>();
		}

		EG_CORE_ASSERT(!"Unknown sequence track type");
		return {};
	}

	void SequenceCameraTrack::Evaluate(SequenceEvalContext& context) const
	{
		if (!bEnabled)
			return;

		if (m_Location.IsEmpty() && m_Rotation.IsEmpty())
			return;

		const Transform local = EvaluateLocalTransform(context.Time);

		SequenceCameraState& state = context.Camera;

		// Play the sequence relative to its base transform, so a cutscene authored around the
		// origin can be placed anywhere in the level just by moving the component that owns it.
		const glm::quat baseRotation = context.Base.Rotation.GetQuat();
		state.WorldTransform.Location = context.Base.Location + (baseRotation * (local.Location * context.Base.Scale3D));
		state.WorldTransform.Rotation = baseRotation * local.Rotation.GetQuat();
		state.WorldTransform.Scale3D = glm::vec3(1.f);

		state.VerticalFOVRadians = glm::radians(m_FOV.Evaluate(context.Time, m_DefaultFOVDegrees));
		state.NearClip = m_NearClip;
		state.FarClip = m_FarClip;
		state.bValid = true;
	}

	Ref<SequenceTrack> SequenceCameraTrack::Clone() const
	{
		Ref<SequenceCameraTrack> result = MakeRef<SequenceCameraTrack>(m_Name);
		CopyBaseTo(*result);

		result->m_Location = m_Location;
		result->m_Rotation = m_Rotation;
		result->m_FOV = m_FOV;
		result->m_DefaultFOVDegrees = m_DefaultFOVDegrees;
		result->m_NearClip = m_NearClip;
		result->m_FarClip = m_FarClip;

		return result;
	}

	void SequenceCameraTrack::ShiftKeys(float deltaTime)
	{
		for (auto& key : m_Location.GetKeys())
			key.Time += deltaTime;
		for (auto& key : m_Rotation.GetKeys())
			key.Time += deltaTime;
		for (auto& key : m_FOV.GetKeys())
			key.Time += deltaTime;

		m_Location.SortKeys();
		m_Rotation.SortKeys();
		m_FOV.SortKeys();
	}

	void SequenceCameraTrack::GatherPathPoints(const Transform& base, float fromTime, float toTime, uint32_t segmentsPerSecond, std::vector<glm::vec3>& outPoints) const
	{
		outPoints.clear();

		if (m_Location.GetKeysCount() < 2)
			return;

		const float duration = toTime - fromTime;
		if (duration <= 0.f)
			return;

		segmentsPerSecond = glm::max(1u, segmentsPerSecond);

		// Cap the sample count so a very long sequence can't flood the debug renderer
		constexpr uint32_t maxSamples = 2048;
		const uint32_t samples = glm::min(maxSamples, glm::max(2u, uint32_t(duration * float(segmentsPerSecond))));

		outPoints.reserve(samples);

		const glm::quat baseRotation = base.Rotation.GetQuat();
		for (uint32_t i = 0; i < samples; ++i)
		{
			const float alpha = float(i) / float(samples - 1);
			const float time = fromTime + duration * alpha;

			const glm::vec3 local = m_Location.Evaluate(time, glm::vec3(0.f));
			outPoints.push_back(base.Location + (baseRotation * (local * base.Scale3D)));
		}
	}

	bool SequenceCameraTrack::GetShotRange(float* outStart, float* outEnd) const
	{
		if (!HasTransformKeys())
			return false;

		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::lowest();
		if (!m_Location.IsEmpty())
		{
			start = glm::min(start, m_Location.GetFirstKeyTime());
			end = glm::max(end, m_Location.GetLastKeyTime());
		}
		if (!m_Rotation.IsEmpty())
		{
			start = glm::min(start, m_Rotation.GetFirstKeyTime());
			end = glm::max(end, m_Rotation.GetLastKeyTime());
		}

		*outStart = start;
		*outEnd = end;
		return true;
	}

	Ref<SequenceTrack> SequenceCameraCutTrack::Clone() const
	{
		Ref<SequenceCameraCutTrack> result = MakeRef<SequenceCameraCutTrack>(m_Name);
		CopyBaseTo(*result);
		result->m_Cuts = m_Cuts;
		return result;
	}

	void SequenceCameraCutTrack::ShiftKeys(float deltaTime)
	{
		for (auto& key : m_Cuts.GetKeys())
			key.Time += deltaTime;
		m_Cuts.SortKeys();
	}

	float SequencePostProcessTrack::Channel::GetFirstKeyTime() const
	{
		return std::visit([](const auto& c) { return c.GetFirstKeyTime(); }, Data);
	}

	float SequencePostProcessTrack::Channel::GetLastKeyTime() const
	{
		return std::visit([](const auto& c) { return c.GetLastKeyTime(); }, Data);
	}

	size_t SequencePostProcessTrack::Channel::GetKeysCount() const
	{
		return std::visit([](const auto& c) { return c.GetKeysCount(); }, Data);
	}

	void SequencePostProcessTrack::Channel::Shift(float deltaTime)
	{
		std::visit([deltaTime](auto& c)
		{
			for (auto& key : c.GetKeys())
				key.Time += deltaTime;
			c.SortKeys();
		}, Data);
	}

	bool SequencePostProcessTrack::Channel::Evaluate(float time, PostProcessValue* outValue) const
	{
		return std::visit([time, outValue](const auto& c) -> bool
		{
			if (c.IsEmpty())
				return false;

			using ValueType = std::decay_t<decltype(c.GetKey(0).Value)>;
			if constexpr (Utils::IsVariantType<ValueType, PostProcessValue>::value)
			{
				*outValue = c.Evaluate(time, ValueType{});
				return true;
			}
			else
			{
				EG_CORE_ERROR("Unsupported variant type: {}", typeid(ValueType).name());
				EG_CORE_ASSERT(false);
				return false;
			}
		}, Data);
	}

	void SequencePostProcessTrack::Evaluate(SequenceEvalContext& context) const
	{
		for (const auto& channel : m_Channels)
		{
			if (!channel.bEnabled)
				continue;

			PostProcessValue value;
			if (channel.Evaluate(context.Time, &value))
				context.PostProcess.Set(channel.Property, value);
		}
	}

	float SequencePostProcessTrack::GetLastKeyTime() const
	{
		float result = 0.f;
		for (const auto& channel : m_Channels)
			result = glm::max(result, channel.GetLastKeyTime());
		return result;
	}

	Ref<SequenceTrack> SequencePostProcessTrack::Clone() const
	{
		Ref<SequencePostProcessTrack> result = MakeRef<SequencePostProcessTrack>(m_Name);
		CopyBaseTo(*result);
		result->m_Channels = m_Channels;
		result->m_CameraTrackID = m_CameraTrackID;
		result->bApplyWholeSequence = bApplyWholeSequence;
		return result;
	}

	void SequencePostProcessTrack::ShiftKeys(float deltaTime)
	{
		for (auto& channel : m_Channels)
			channel.Shift(deltaTime);
	}

	bool SequencePostProcessTrack::GetActiveRange(float* outStart, float* outEnd) const
	{
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::lowest();
		bool bAny = false;

		for (const auto& channel : m_Channels)
		{
			if (!channel.bEnabled || channel.GetKeysCount() == 0)
				continue;

			start = glm::min(start, channel.GetFirstKeyTime());
			end = glm::max(end, channel.GetLastKeyTime());
			bAny = true;
		}

		if (!bAny)
			return false;

		*outStart = start;
		*outEnd = end;
		return true;
	}

	bool SequencePostProcessTrack::IsActiveAt(float time) const
	{
		float start = 0.f, end = 0.f;
		if (!GetActiveRange(&start, &end))
			return false;

		// A track that covers the whole sequence holds its first/last value outside the keyed range
		return bApplyWholeSequence || (time >= start && time <= end);
	}

	SequencePostProcessTrack::Channel& SequencePostProcessTrack::AddProperty(PostProcessProperty property)
	{
		if (Channel* existing = FindChannel(property))
			return *existing;

		Channel& channel = m_Channels.emplace_back();
		channel.Property = property;

		const PostProcessPropertyInfo* info = FindPostProcessProperty(property);
		PostProcessValueType type = PostProcessValueType::Float;
		if (info)
		{
			type = info->Type;
		}
		else
		{
			type = PostProcessValueType::Float;
			EG_CORE_ERROR("Failed to add Post Process property: {}. Falling back to `Float`", Utils::GetEnumName(property));
		}

		switch (type)
		{
			case PostProcessValueType::Bool:   channel.Data = SequenceBoolChannel{};  break;
			case PostProcessValueType::Int:    channel.Data = SequenceIntChannel{};   break;
			case PostProcessValueType::UInt:   channel.Data = SequenceUIntChannel{};  break;
			case PostProcessValueType::Float:  channel.Data = SequenceFloatChannel{}; break;
			case PostProcessValueType::Enum:   channel.Data = SequenceIntChannel{};   break;
			case PostProcessValueType::Vec2:   channel.Data = SequenceVec2Channel{};  break;
			case PostProcessValueType::Vec3:   channel.Data = SequenceVec3Channel{};  break;
			case PostProcessValueType::Color3: channel.Data = SequenceVec3Channel{};  break;
			case PostProcessValueType::Asset:  channel.Data = SequenceGUIDChannel{};  break;
			default:
				EG_CORE_ASSERT(!"Unknown type");
		}
		return channel;
	}

	bool SequencePostProcessTrack::RemoveProperty(PostProcessProperty property)
	{
		for (auto it = m_Channels.begin(); it != m_Channels.end(); ++it)
		{
			if (it->Property == property)
			{
				m_Channels.erase(it);
				return true;
			}
		}
		return false;
	}

	SequencePostProcessTrack::Channel* SequencePostProcessTrack::FindChannel(PostProcessProperty property)
	{
		for (auto& channel : m_Channels)
			if (channel.Property == property)
				return &channel;
		return nullptr;
	}

	const SequencePostProcessTrack::Channel* SequencePostProcessTrack::FindChannel(PostProcessProperty property) const
	{
		for (const auto& channel : m_Channels)
			if (channel.Property == property)
				return &channel;
		return nullptr;
	}

	Ref<SequenceTrack> SequenceEventTrack::Clone() const
	{
		Ref<SequenceEventTrack> result = MakeRef<SequenceEventTrack>(m_Name);
		CopyBaseTo(*result);
		result->m_Events = m_Events;
		return result;
	}

	void SequenceEventTrack::GatherEvents(const SequenceEventWindow& window, std::vector<SequenceEvent>& outEvents) const
	{
		if (!window.bValid)
			return;

		for (const auto& key : m_Events.GetKeys())
		{
			if (window.Contains(key.Time))
				outEvents.push_back({ key.Value, key.Time });
		}
	}

	void SequenceEventTrack::ShiftKeys(float deltaTime)
	{
		for (auto& key : m_Events.GetKeys())
			key.Time += deltaTime;
		m_Events.SortKeys();
	}
}
