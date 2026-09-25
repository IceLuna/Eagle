#include "egpch.h"
#include "PostProcessOverride.h"

#include "RendererUtils.h"
#include "Eagle/Asset/Asset.h"
#include "Eagle/Asset/AssetManager.h"

#include <limits>

namespace Eagle
{
	namespace Utils
	{
		template <typename AssetT>
		void SetAsset(const GUID& assetID, Ref<AssetT>& outAsset)
		{
			if (assetID.IsNull())
			{
				outAsset.reset();
				return;
			}

			Ref<Asset> asset;
			if (AssetManager::Get(assetID, &asset))
			{
				if (Ref<AssetT> casted = Cast<AssetT>(asset))
					outAsset = casted;
			}
		}

		// Each entry is one line of glue between a property ID and the field it drives.
		// `Get` is used to seed a new key with whatever the scene currently looks like.
		// NOTE: Don't forget to `undef` it below
#define EG_PP_BOOL(prop, category, name, field, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Bool, category, name, tooltip, 0.f, 0.f, 0.f, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = std::get<bool>(v); } }

#define EG_PP_INT(prop, category, name, field, speed, minValue, maxValue, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Int, category, name, tooltip, speed, minValue, maxValue, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::clamp(std::get<int32_t>(v), int32_t(minValue), maxValue > minValue ? int32_t(maxValue) : std::numeric_limits<int32_t>::max()); } }

#define EG_PP_UINT(prop, category, name, field, speed, minValue, maxValue, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::UInt, category, name, tooltip, speed, minValue, maxValue, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::clamp(std::get<uint32_t>(v), uint32_t(minValue), maxValue > minValue ? uint32_t(maxValue) : std::numeric_limits<uint32_t>::max()); } }

#define EG_PP_FLOAT(prop, category, name, field, speed, minValue, maxValue, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Float, category, name, tooltip, speed, minValue, maxValue, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::clamp(std::get<float>(v), minValue, maxValue > minValue ? maxValue : std::numeric_limits<float>::max()); } }

#define EG_PP_ENUM(prop, category, name, field, enumType, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Enum, category, name, tooltip, 0.f, 0.f, 0.f, enumType, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return int32_t(s.field); }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = decltype(s.field)(std::get<int32_t>(v)); } }

#define EG_PP_VEC2(prop, category, name, field, speed, minValue, maxValue, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Vec2, category, name, tooltip, speed, minValue, maxValue, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::clamp(std::get<glm::vec2>(v), glm::vec2(minValue), glm::vec2(maxValue)); } }

#define EG_PP_VEC3(prop, category, name, field, speed, minValue, maxValue, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Vec3, category, name, tooltip, speed, minValue, maxValue, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::clamp(std::get<glm::vec3>(v), glm::vec3(minValue), glm::vec3(maxValue)); } }

#define EG_PP_COLOR3(prop, category, name, field, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Color3, category, name, tooltip, 0.01f, 0.f, 0.f, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field; }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { s.field = glm::max(std::get<glm::vec3>(v), glm::vec3(0.f)); } }

#define EG_PP_ASSET(prop, category, name, field, tooltip) \
		PostProcessPropertyInfo{ PostProcessProperty::prop, PostProcessValueType::Asset, category, name, tooltip, 0.f, 0.f, 0.f, PostProcessEnumType::None, \
			[](const SceneRendererSettings& s) -> PostProcessValue { return s.field ? s.field->GetGUID() : GUID(0, 0); }, \
			[](SceneRendererSettings& s, const PostProcessValue& v) { SetAsset(std::get<GUID>(v), s.field); } }

		static const std::vector<PostProcessPropertyInfo> s_Properties =
		{
			EG_PP_FLOAT(Exposure, "Exposure", "Exposure", Exposure, 0.1f, 0.f, 100.f, "Ignored while Auto Exposure is enabled"),

			EG_PP_BOOL(AutoExposureEnable, "Auto Exposure", "Enable", AutoExposure.bEnable, ""),
			EG_PP_FLOAT(AutoExposureMinLogLuminance, "Auto Exposure", "Min Luminance (log)", AutoExposure.MinLogLum, 0.1f, -100.f, 100.f, "Logarithmic value"),
			EG_PP_FLOAT(AutoExposureMaxLogLuminance, "Auto Exposure", "Max Luminance (log)", AutoExposure.MaxLogLum, 0.1f, -100.f, 100.f, "Logarithmic value"),
			EG_PP_FLOAT(AutoExposureAdaptationSpeed, "Auto Exposure", "Adaptation Speed", AutoExposure.AdaptationSpeed, 0.05f, 0.f, 0.f, "Controls how fast Auto Exposure reacts to changes.\nNote: adaptation is temporal, so scrubbing won't reproduce it frame-exactly"),
			EG_PP_FLOAT(AutoExposureAdaptationKey, "Auto Exposure", "Adaptation Key", AutoExposure.AdaptationKey, 0.01f, 0.f, 0.f, "Controls the final Exposure"),

			EG_PP_BOOL(BloomEnable, "Bloom", "Enable", BloomSettings.bEnable, ""),
			EG_PP_FLOAT(BloomThreshold, "Bloom", "Threshold", BloomSettings.Threshold, 0.05f, 0.f, 0.f, ""),
			EG_PP_FLOAT(BloomIntensity, "Bloom", "Intensity", BloomSettings.Intensity, 0.05f, 0.f, 0.f, ""),
			EG_PP_FLOAT(BloomDirtIntensity, "Bloom", "Dirt Intensity", BloomSettings.DirtIntensity, 0.05f, 0.f, 0.f, "The dirt texture itself is a scene setting and isn't animated"),
			EG_PP_FLOAT(BloomKnee, "Bloom", "Knee", BloomSettings.Knee, 0.01f, 0.f, 0.f, ""),
			EG_PP_ASSET(BloomDirtTexture, "Bloom", "Dirt Texture", BloomSettings.Dirt, "Texture the dirt overlay uses. Switches instantly at each key"),

			EG_PP_BOOL(FogEnable, "Fog", "Enable", FogSettings.bEnable, ""),
			EG_PP_ENUM(FogEquationType, "Fog", "Equation", FogSettings.Equation, PostProcessEnumType::FogEquation, ""),
			EG_PP_COLOR3(FogColor, "Fog", "Color", FogSettings.Color, ""),
			EG_PP_FLOAT(FogMinDistance, "Fog", "Min Distance", FogSettings.MinDistance, 0.5f, 0.f, 0.f, "Everything closer won't be affected by the fog. Used by the Linear equation"),
			EG_PP_FLOAT(FogMaxDistance, "Fog", "Max Distance", FogSettings.MaxDistance, 0.5f, 0.f, 0.f, "Everything after this distance is fog. Used by the Linear equation"),
			EG_PP_FLOAT(FogDensity, "Fog", "Density", FogSettings.Density, 0.001f, 0.f, 0.f, "Used by the Exponential equations"),

			EG_PP_VEC2(DOFApertureShape, "Depth of Field", "Aperture Shape", DOFSettings.ApertureShape, 0.1f, 0.f, 2.f, ""),
			EG_PP_FLOAT(DOFApertureSize, "Depth of Field", "Aperture Size", DOFSettings.ApertureSize, 0.01f, 0.f, 0.f, "0 disables Depth of Field, so this is what a focus pull animates"),
			EG_PP_FLOAT(DOFFocalLength, "Depth of Field", "Focal Length", DOFSettings.FocalLength, 0.01f, 0.f, 0.f, "Distance to the point that stays in focus"),
			EG_PP_FLOAT(DOFCOCScale, "Depth of Field", "COC Scale", DOFSettings.COCScale, 0.1f, 0.f, 0.f, "Circle of Confusion scale"),
			EG_PP_FLOAT(DOFMaxCOC, "Depth of Field", "Max COC", DOFSettings.MaxCOC, 0.1f, 0.f, 0.f, "Max Circle of Confusion"),

			EG_PP_BOOL(MotionBlurEnable, "Motion Blur", "Enable", MotionBlur.bEnable, "Toggling this mid-shot changes what the renderer generates (motion vectors), so prefer keying Strength"),
			EG_PP_FLOAT(MotionBlurStrength, "Motion Blur", "Strength", MotionBlur.Strength, 0.01f, 0.f, 1.f, ""),
			EG_PP_UINT(MotionBlurNumSamples, "Motion Blur", "Num Samples", MotionBlur.NumSamples, 1.0f, 1.0f, 64.0f, ""),
			EG_PP_BOOL(MotionBlurUseCheapOnLowMotion, "Motion Blur", "Use Cheap On Low Motion", MotionBlur.bUseCheapOnLowMotion, ""),
			EG_PP_FLOAT(MotionBlurNoMotionThreshold, "Motion Blur", "No Motion Blur Threshold", MotionBlur.NoMotionBlurThreshold, 0.001f, 0.f, 1.f, "Below this motion vector magnitude, no motion blur is applied"),
			EG_PP_FLOAT(MotionBlurLowMotionThreshold, "Motion Blur", "Low Motion Threshold", MotionBlur.LowMotionThreshold, 0.001f, 0.f, 1.f, "Below this motion vector magnitude, cheaper motion blur is applied"),

			EG_PP_BOOL(ChromaticAberrationEnable, "Chromatic Aberration", "Enable", Lens.bEnableChromaticAberration, ""),
			EG_PP_FLOAT(ChromaticAberrationIntensity, "Chromatic Aberration", "Intensity", Lens.ChromaticIntensity, 0.01f, 0.f, 0.f, ""),

			EG_PP_BOOL(VignetteEnable, "Vignette", "Enable", Lens.bEnableVignette, ""),
			EG_PP_FLOAT(VignetteIntensity, "Vignette", "Intensity", Lens.VignetteIntensity, 0.01f, 0.f, 2.5f, ""),

			EG_PP_BOOL(FilmGrainEnable, "Film Grain", "Enable", Lens.bEnableFilmGrain, ""),
			EG_PP_FLOAT(FilmGrainScale, "Film Grain", "Scale", Lens.FilmGrainScale, 0.01f, 0.f, 0.f, ""),
			EG_PP_FLOAT(FilmGrainAmount, "Film Grain", "Amount", Lens.FilmGrainAmount, 0.01f, 0.f, 0.f, ""),
			EG_PP_FLOAT(FilmGrainSeedUpdateRate, "Film Grain", "Seed Update Rate", Lens.FilmGrainSeedUpdateRate, 0.005f, 0.f, 0.f, "How often (in seconds) the grain pattern changes"),
		};

#undef EG_PP_BOOL
#undef EG_PP_INT
#undef EG_PP_UINT
#undef EG_PP_FLOAT
#undef EG_PP_ENUM
#undef EG_PP_VEC2
#undef EG_PP_VEC3
#undef EG_PP_COLOR3
#undef EG_PP_ASSET
	}

	const std::vector<PostProcessPropertyInfo>& GetPostProcessProperties()
	{
		return Utils::s_Properties;
	}

	const PostProcessPropertyInfo* FindPostProcessProperty(PostProcessProperty property)
	{
		for (const auto& info : Utils::s_Properties)
			if (info.Property == property)
				return &info;
		return nullptr;
	}

	void PostProcessOverride::Set(PostProcessProperty property, const PostProcessValue& value)
	{
		for (auto& entry : m_Entries)
		{
			if (entry.Property == property)
			{
				entry.Value = value;
				return;
			}
		}
		m_Entries.push_back({ property, value });
	}

	const PostProcessValue* PostProcessOverride::Find(PostProcessProperty property) const
	{
		for (const auto& entry : m_Entries)
			if (entry.Property == property)
				return &entry.Value;
		return nullptr;
	}

	void PostProcessOverride::ApplyTo(SceneRendererSettings& settings) const
	{
		for (const auto& entry : m_Entries)
		{
			const PostProcessPropertyInfo* info = FindPostProcessProperty(entry.Property);
			if (info && info->Set && info->Get(settings).index() == entry.Value.index())
				info->Set(settings, entry.Value);
		}
	}
}
