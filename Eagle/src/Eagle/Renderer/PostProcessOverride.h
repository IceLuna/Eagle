#pragma once

#include "Eagle/Core/Core.h"
#include "Eagle/Core/GUID.h"

#include <glm/glm.hpp>

#include <variant>
#include <vector>

namespace Eagle
{
	struct SceneRendererSettings;

	// Can be used to temporarily overridde settings.
	// Limited to values that describe how a frame looks.
	enum class PostProcessProperty : uint16_t
	{
		Exposure,

		AutoExposureEnable,
		AutoExposureMinLogLuminance,
		AutoExposureMaxLogLuminance,
		AutoExposureAdaptationSpeed,
		AutoExposureAdaptationKey,

		BloomEnable,
		BloomThreshold,
		BloomIntensity,
		BloomDirtIntensity,
		BloomDirtTexture,
		BloomKnee,

		FogEnable,
		FogEquationType,
		FogColor,
		FogMinDistance,
		FogMaxDistance,
		FogDensity,

		DOFApertureShape,
		DOFApertureSize,
		DOFFocalLength,
		DOFCOCScale,
		DOFMaxCOC,

		MotionBlurEnable,
		MotionBlurStrength,
		MotionBlurNumSamples,
		MotionBlurUseCheapOnLowMotion,
		MotionBlurNoMotionThreshold,
		MotionBlurLowMotionThreshold,

		ChromaticAberrationEnable,
		ChromaticAberrationIntensity,

		VignetteEnable,
		VignetteIntensity,

		FilmGrainEnable,
		FilmGrainScale,
		FilmGrainAmount,
		FilmGrainSeedUpdateRate,
	};

	// When expanded, new `Sequence*Channel` needs to be created for the type (such as `SequenceBoolChannel`)
	enum class PostProcessValueType : uint8_t
	{
		Bool,
		Int,
		UInt,
		Float,
		Enum,
		Vec2,
		Vec3,
		Color3,
		Asset,
	};

	// Which enum an `Enum` property holds, so the UI can show proper names
	// When a new one is added here, you'd probably want to update UI code to render it.
	enum class PostProcessEnumType : uint8_t
	{
		None,
		FogEquation
	};

	// Stores `PostProcessValueType`. Enum as int32_t, Asset as GUID
	using PostProcessValue = std::variant<bool, int32_t, uint32_t, float, glm::vec2, glm::vec3, GUID>;

	struct PostProcessPropertyInfo
	{
		PostProcessProperty Property = PostProcessProperty::Exposure;
		PostProcessValueType Type = PostProcessValueType::Float;

		std::string Category;
		std::string Name;
		std::string Tooltip;

		float DragSpeed = 0.05f;
		float Min = 0.f;
		float Max = 0.f;

		PostProcessEnumType EnumType = PostProcessEnumType::None;

		PostProcessValue (*Get)(const SceneRendererSettings& settings) = nullptr;
		void (*Set)(SceneRendererSettings& settings, const PostProcessValue& value) = nullptr;
	};

	// Every overridable property, in the order they should be listed in the UI
	const std::vector<PostProcessPropertyInfo>& GetPostProcessProperties();
	const PostProcessPropertyInfo* FindPostProcessProperty(PostProcessProperty property);

	// A set of rendering settings. Only the properties it holds are affected;
	// everything else keeps whatever the scene's own settings say
	struct PostProcessOverride
	{
		struct Entry
		{
			PostProcessProperty Property = PostProcessProperty::Exposure;
			PostProcessValue Value;

			bool operator==(const Entry& other) const { return Property == other.Property && Value == other.Value; }
		};

		bool IsEmpty() const { return m_Entries.empty(); }
		void Clear() { m_Entries.clear(); }

		// Adds the property, or replaces it if it's already overridden
		void Set(PostProcessProperty property, const PostProcessValue& value);
		const PostProcessValue* Find(PostProcessProperty property) const;

		void ApplyTo(SceneRendererSettings& settings) const;

		bool operator==(const PostProcessOverride& other) const { return m_Entries == other.m_Entries; }
		bool operator!=(const PostProcessOverride& other) const { return !(*this == other); }

	private:
		std::vector<Entry> m_Entries;
	};
}
