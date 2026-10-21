#include "egpch.h"
#include "VolumetricLightTask.h"

#include "Eagle/Renderer/SceneRenderer.h"
#include "Eagle/Renderer/RenderManager.h"

#include "Eagle/Renderer/VidWrappers/RenderCommandManager.h"

#include "Eagle/Debug/CPUTimings.h"
#include "Eagle/Debug/GPUTimings.h"

#include "../../Eagle-Editor/assets/shaders/defines.h"

#include <glm/gtc/packing.hpp>

namespace Eagle
{
	namespace Noise
	{
		// Fog noise generation (`GenerateFogNoise()`). References:
		// - Gradient noise: K. Perlin, "An Image Synthesizer", SIGGRAPH 1985
		// - Quintic interpolation: K. Perlin, "Improving Noise", SIGGRAPH 2002. Reference implementation: https://cs.nyu.edu/~perlin/noise/
		// - Tileable (periodic) gradient noise, like `pnoise()` in S. Gustavson's webgl-noise: https://github.com/stegu/webgl-noise
		// - Fractal sum of noise octaves (fBm): F. K. Musgrave, in D. Ebert et al., "Texturing & Modeling: A Procedural Approach"
		// - Hash: M. Jarzynski, M. Olano, "Hash Functions for GPU Rendering", JCGT 2020: https://www.jcgt.org/published/0009/03/02/
		//   Also see N. Reed, "Hash Functions for GPU Rendering": https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
		// - Uniform directions: M. Pharr, W. Jakob, G. Humphreys, "Physically Based Rendering", 3rd ed., 13.6 "2D Sampling with Multidimensional Transformations":
		//   https://pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations

		// `pcg_hash()` from Jarzynski & Olano (see above). It's derived from M. E. O'Neill's PCG random number generator: https://www.pcg-random.org/
		uint32_t PCGHash(uint32_t value)
		{
			const uint32_t state = value * 747796405u + 2891336453u;
			const uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
			return (word >> 22u) ^ word;
		}

		// Maps a hash to [0; 1). The top 24 bits are used, since a float represents all multiples of 2^-24 in [0; 1) exactly
		float HashToUnitFloat(uint32_t hash)
		{
			return float(hash >> 8u) * (1.f / 16777216.f);
		}

		// Adds an octave of tileable 3D gradient (Perlin) noise, multiplied by `amplitude`, to `values` (a cube of `size`^3 voxels covering one tile).
		// Random unit gradients are placed on a lattice of `cells`^3 cells that wraps around, which makes the noise tileable.
		// It's evaluated per lattice cell: the cell's 8 gradients are fetched once and the per-axis interpolation weights are shared by all cells.
		// Perlin's 1985 noise with the quintic interpolation from his 2002 paper (see the references above). Unlike the 2002 version, which uses 12 fixed gradient
		// directions to make runtime evaluation cheaper, the gradients here are random unit vectors (as in the original), since the noise is generated only once.
		void AddTileableGradientNoise(std::vector<float>& values, uint32_t size, uint32_t cells, float amplitude, uint32_t seed)
		{
			EG_CORE_ASSERT(size % cells == 0);
			EG_CORE_ASSERT(values.size() == size_t(size) * size * size);

			std::vector<glm::vec3> gradients(size_t(cells) * cells * cells);
			const uint32_t seedHash = PCGHash(seed);
			for (uint32_t i = 0; i < uint32_t(gradients.size()); ++i)
			{
				// Uniformly distributed directions: z and the azimuth are uniform (Archimedes' hat-box theorem). See PBRT's `UniformSampleSphere()` (referenced above)
				const float z = 1.f - 2.f * HashToUnitFloat(PCGHash(seedHash + 2u * i));
				const float phi = glm::two_pi<float>() * HashToUnitFloat(PCGHash(seedHash + 2u * i + 1u));
				const float r = std::sqrt(std::max(0.f, 1.f - z * z));
				gradients[i] = glm::vec3(r * std::cos(phi), r * std::sin(phi), z);
			}
			auto getGradient = [&gradients, cells](uint32_t x, uint32_t y, uint32_t z) -> const glm::vec3&
			{
				return gradients[size_t(x % cells) + size_t(cells) * (size_t(y % cells) + size_t(cells) * size_t(z % cells))];
			};

			// Voxel centers relative to their cell, and the quintic (C2-continuous) interpolation weights: 6t^5 - 15t^4 + 10t^3 (Perlin 2002). The same for all axes & cells
			const uint32_t voxelsPerCell = size / cells;
			std::vector<float> local(voxelsPerCell);
			std::vector<float> fade(voxelsPerCell);
			for (uint32_t i = 0; i < voxelsPerCell; ++i)
			{
				const float t = (float(i) + 0.5f) / float(voxelsPerCell);
				local[i] = t;
				fade[i] = t * t * t * (t * (t * 6.f - 15.f) + 10.f);
			}

			for (uint32_t cz = 0; cz < cells; ++cz)
				for (uint32_t cy = 0; cy < cells; ++cy)
					for (uint32_t cx = 0; cx < cells; ++cx)
					{
						const glm::vec3& g000 = getGradient(cx,     cy,     cz);
						const glm::vec3& g100 = getGradient(cx + 1, cy,     cz);
						const glm::vec3& g010 = getGradient(cx,     cy + 1, cz);
						const glm::vec3& g110 = getGradient(cx + 1, cy + 1, cz);
						const glm::vec3& g001 = getGradient(cx,     cy,     cz + 1);
						const glm::vec3& g101 = getGradient(cx + 1, cy,     cz + 1);
						const glm::vec3& g011 = getGradient(cx,     cy + 1, cz + 1);
						const glm::vec3& g111 = getGradient(cx + 1, cy + 1, cz + 1);

						for (uint32_t z = 0; z < voxelsPerCell; ++z)
							for (uint32_t y = 0; y < voxelsPerCell; ++y)
								for (uint32_t x = 0; x < voxelsPerCell; ++x)
								{
									const float fx = local[x], fy = local[y], fz = local[z];
									const float fx1 = fx - 1.f, fy1 = fy - 1.f, fz1 = fz - 1.f;

									// Dot products of the corners' gradients with the offsets from the corners
									const float n000 = g000.x * fx  + g000.y * fy  + g000.z * fz;
									const float n100 = g100.x * fx1 + g100.y * fy  + g100.z * fz;
									const float n010 = g010.x * fx  + g010.y * fy1 + g010.z * fz;
									const float n110 = g110.x * fx1 + g110.y * fy1 + g110.z * fz;
									const float n001 = g001.x * fx  + g001.y * fy  + g001.z * fz1;
									const float n101 = g101.x * fx1 + g101.y * fy  + g101.z * fz1;
									const float n011 = g011.x * fx  + g011.y * fy1 + g011.z * fz1;
									const float n111 = g111.x * fx1 + g111.y * fy1 + g111.z * fz1;

									const float u = fade[x], v = fade[y], w = fade[z];
									const float nx00 = n000 + u * (n100 - n000);
									const float nx10 = n010 + u * (n110 - n010);
									const float nx01 = n001 + u * (n101 - n001);
									const float nx11 = n011 + u * (n111 - n011);
									const float nxy0 = nx00 + v * (nx10 - nx00);
									const float nxy1 = nx01 + v * (nx11 - nx01);
									const float noise = nxy0 + w * (nxy1 - nxy0);

									const size_t index = size_t(cx * voxelsPerCell + x) + size_t(size) * (size_t(cy * voxelsPerCell + y) + size_t(size) * size_t(cz * voxelsPerCell + z));
									values[index] += amplitude * noise;
								}
					}
		}

		// Generates the tileable fog density texture (fractal sum of gradient noise, i.e. fBm with the lacunarity of 2 and the gain of 0.5), as half floats.
		// The density is normalized to the mean of exactly 1, see `SampleFog()` in `volumetrics/utils.h`
		std::vector<uint16_t> GenerateFogNoise()
		{
			constexpr uint32_t size = EG_FOG_NOISE_TEXTURE_SIZE;
			constexpr uint32_t baseLatticeCells = 4u;
			// Lattices of 4, 8 & 16 cells per tile. Finer octaves would be under-sampled by the texture (the detail comes from the second layer instead)
			constexpr uint32_t octaves = 3u;
			// Maps the noise to the density. Values below 0 are clamped, which creates clear gaps in the fog
			constexpr float contrast = 2.f;

			std::vector<float> fbm(size_t(size) * size * size, 0.f);
			float amplitudesSum = 0.f;
			for (uint32_t octave = 0; octave < octaves; ++octave)
			{
				const float amplitude = 1.f / float(1u << octave);
				AddTileableGradientNoise(fbm, size, baseLatticeCells << octave, amplitude, octave);
				amplitudesSum += amplitude;
			}

			double densitySum = 0.0;
			for (float& value : fbm)
			{
				value = std::max(0.5f + contrast * (value / amplitudesSum), 0.f); // Density now
				densitySum += value;
			}

			const float invMean = float(double(fbm.size()) / densitySum);
			std::vector<uint16_t> result(fbm.size());
			for (size_t i = 0; i < fbm.size(); ++i)
				result[i] = glm::packHalf1x16(fbm[i] * invMean);
			return result;
		}

		const std::vector<uint16_t>& GetFogNoiseData()
		{
			// Generated once and shared by all renderers
			static const std::vector<uint16_t> s_Data = GenerateFogNoise();
			return s_Data;
		}
	}

    VolumetricLightTask::VolumetricLightTask(SceneRenderer& renderer)
        : RendererTask(renderer)
    {
		const auto& options = m_Renderer.GetOptions();
		m_VolumetricSettings = options.VolumetricSettings;
		bTranslucentShadows = options.bTranslucentShadows;

		ImageSpecifications specs;
		specs.Format = ImageFormat::R16G16B16A16_Float; // rgb - in-scattered light, a - transmittance
		specs.Size = glm::uvec3(GetResourceSize(m_Renderer.GetHDROutput()->GetSize()), 1u);
		specs.Usage = ImageUsage::Sampled | ImageUsage::Storage;
		m_VolumetricsImage = Image::Create(specs, "PBR_Volumetric");
		m_VolumetricsImageBlurred = Image::Create(specs, "PBR_Volumetric_Blurred");

		specs.Format = ImageFormat::R32_Float;
		m_VolumetricsDepthImage = Image::Create(specs, "PBR_Volumetric_Depth"); // Linear depth that each low-res pixel was traced to

		{
			BufferSpecifications specs{};
			specs.Size = sizeof(DispatchIndirectArgs) * 2; // For blur & composite passes
			specs.Usage = BufferUsage::TransferDst | BufferUsage::StorageBuffer | BufferUsage::IndirectBuffer;
			specs.Layout = BufferLayoutType::StorageBuffer;
			m_IndirectArgs = Buffer::Create(specs, "VolumetricLights_IndirectArgs");
		}

		{
			PipelineComputeState state{};
			state.ComputeShader = Shader::Create("volumetrics/prepare_args.comp", ShaderType::Compute);
			m_PrepareArgsPipeline = PipelineCompute::Create(state);
		}

		InitPipeline();
    }

	void VolumetricLightTask::RecordCommandBuffer(const Ref<CommandBuffer>& cmd)
	{
		if (!m_Renderer.HasVolumetricLights())
			return;

		EG_GPU_TIMING_SCOPED(cmd, "Volumetric Light Pass");
		EG_CPU_TIMING_SCOPED("Volumetric Light Pass");

		auto& stats = m_Renderer.GetStats();
		const auto& input = m_Renderer.GetHDROutput();

		const glm::uvec2 size = input->GetSize();
		const glm::uvec2 volumetricsImageSize = m_VolumetricsImage->GetSize();
		const Ref<LightCullingTask>& lightCulling = m_Renderer.GetLightCullingTask();

		const bool bFog = m_VolumetricSettings.bFogEnable;
		if (bFog)
		{
			if (!m_FogNoise)
				CreateFogNoise(cmd);
			UpdateFogNoiseOffsets(Application::Get().GetTimestep());
		}

		struct VolumetricPushData
		{
			glm::vec3 CameraPos;
			float VolumetricMaxScatteringDist;
			glm::ivec2 Size;
			uint32_t DownscaleFactor;
			uint32_t TilesBufferWidth;
			glm::vec3 FogAlbedo;
			float FogAnisotropy;
			glm::vec3 FogNoiseOffset0;
			uint32_t HasDirLight;
			glm::vec3 FogNoiseOffset1;
			float Density;
			float FogScaleRCP;
		} volumetricPushData;
		static_assert(sizeof(VolumetricPushData) <= 128);

		volumetricPushData.CameraPos = m_Renderer.GetViewPosition();
		volumetricPushData.VolumetricMaxScatteringDist = m_VolumetricSettings.MaxScatteringDistance;
		volumetricPushData.Size = volumetricsImageSize;
		volumetricPushData.DownscaleFactor = GetDownscaleFactor();
		volumetricPushData.TilesBufferWidth = lightCulling->GetTilesBufferWidth();
		volumetricPushData.FogAlbedo = m_VolumetricSettings.Albedo;
		volumetricPushData.FogAnisotropy = m_VolumetricSettings.Anisotropy;
		volumetricPushData.FogNoiseOffset0 = m_FogNoiseOffsets[0];
		volumetricPushData.HasDirLight = uint32_t(m_Renderer.HasDirectionalLight());
		volumetricPushData.FogNoiseOffset1 = m_FogNoiseOffsets[1];
		volumetricPushData.Density = glm::max(m_VolumetricSettings.Density, 0.f);
		volumetricPushData.FogScaleRCP = 1.f / GetFogScale();

		const auto& gbuffer = m_Renderer.GetGBuffer();
		m_Pipeline->SetImage(m_VolumetricsImage, 0, 0);
		m_Pipeline->SetImage(m_VolumetricsDepthImage, 0, 1);
		m_Pipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 2);
		m_Pipeline->SetBuffer(m_IndirectArgs, 0, 3);
		if (bFog)
			m_Pipeline->SetImageSampler(m_FogNoise, Sampler::BilinearSampler, 0, 4);
		m_Pipeline->SetBuffer(lightCulling->GetCulledPointLightsBuffer(), EG_SCENE_SET, 0);
		m_Pipeline->SetBuffer(lightCulling->GetCulledSpotLightsBuffer(), EG_SCENE_SET, 1);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_PL(), EG_SCENE_SET, 2);
		m_Pipeline->SetBuffer(lightCulling->GetTiles_Translucent_SL(), EG_SCENE_SET, 3);
		m_Pipeline->SetBuffer(lightCulling->GetLightsCountersBuffer(), EG_SCENE_SET, 4);
		m_Pipeline->SetBuffer(m_Renderer.GetDirectionalLightBuffer(), EG_SCENE_SET, 5);
		m_Pipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), EG_SCENE_SET, 6);
		m_Pipeline->SetBuffer(m_Renderer.GetLightMatricesBuffer(), EG_SCENE_SET, 7);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 2, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 3, 0);
		m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMaps(), m_Renderer.GetShadowMapPCFSampler(), 4, 0);

		if (bTranslucentShadows)
		{
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 5, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 6, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColored(), m_Renderer.GetColoredShadowMapSampler(), 7, 0);

			m_Pipeline->SetImageSamplerArray(m_Renderer.GetDirectionalLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 8, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetPointLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 9, 0);
			m_Pipeline->SetImageSamplerArray(m_Renderer.GetSpotLightShadowMapsColoredDepth(), m_Renderer.GetShadowMapPointSampler(), 10, 0);
		}

		const ImageLayout resultLayout = input->GetLayout();
		const ImageLayout depthLayout = gbuffer.Depth->GetLayout();

		cmd->TransitionLayout(input, resultLayout, ImageLayoutType::StorageImage);
		cmd->TransitionLayout(gbuffer.Depth, depthLayout, ImageReadAccess::PixelShaderRead);

		cmd->FillBuffer(m_IndirectArgs, 0);

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lighting");
			EG_CPU_TIMING_SCOPED("Volumetric Lighting");

			const glm::uvec2 numGroups = CalcNumGroups(volumetricsImageSize, m_Pipeline->GetWorkGroupSize());

			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->TransitionLayout(m_VolumetricsDepthImage, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->Dispatch(m_Pipeline, numGroups, &volumetricPushData);
			cmd->TransitionLayout(m_VolumetricsImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			cmd->TransitionLayout(m_VolumetricsDepthImage, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}
		cmd->Barrier(m_IndirectArgs);

		const glm::uvec2 blurGroupSize = m_BlurPipeline->GetWorkGroupSize();
		const glm::uvec2 compositeGroupSize = m_CompositePipeline->GetWorkGroupSize();
		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Lights. Prepare args");
			EG_CPU_TIMING_SCOPED("Volumetric Lights. Prepare args");

			struct PrepareArgsPushData
			{
				glm::uvec2 BlurSize;
				glm::uvec2 CompositeSize;
				glm::uvec2 BlurGroupSize;
				glm::uvec2 CompositeGroupSize;
			} prepareArgsPushData;
			prepareArgsPushData.BlurSize = volumetricsImageSize;
			prepareArgsPushData.CompositeSize = size;
			prepareArgsPushData.BlurGroupSize = blurGroupSize;
			prepareArgsPushData.CompositeGroupSize = compositeGroupSize;

			m_PrepareArgsPipeline->SetBuffer(m_IndirectArgs, 0, 0);
			cmd->Dispatch(m_PrepareArgsPipeline, 1, 1, 1, &prepareArgsPushData);
			cmd->TransitionLayout(m_IndirectArgs, BufferLayoutType::StorageBuffer, BufferReadAccess::IndirectArgument);
		}

		struct BlurPushData
		{
			glm::ivec2 Size;
			glm::vec2 TexelSize;
		};

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Blur");
			EG_CPU_TIMING_SCOPED("Volumetric Blur");

			m_BlurPipeline->SetImageSampler(m_VolumetricsImage, Sampler::PointSamplerClamp, 0, 0);
			m_BlurPipeline->SetImageSampler(m_VolumetricsDepthImage, Sampler::PointSamplerClamp, 0, 1);
			m_BlurPipeline->SetImage(m_VolumetricsImageBlurred, 0, 2);

			BlurPushData blurPushData;
			blurPushData.Size = volumetricsImageSize;
			blurPushData.TexelSize = 1.f / glm::vec2(volumetricsImageSize);

			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::Unknown, ImageLayoutType::StorageImage);
			cmd->DispatchIndirect(m_BlurPipeline, m_IndirectArgs, 0, &blurPushData);
			cmd->TransitionLayout(m_VolumetricsImageBlurred, ImageLayoutType::StorageImage, ImageReadAccess::PixelShaderRead);
			++stats.Dispatches;
		}

		{
			EG_GPU_TIMING_SCOPED(cmd, "Volumetric Composite");
			EG_CPU_TIMING_SCOPED("Volumetric Composite");

			struct CompositePushData
			{
				glm::ivec2 Size;
				glm::vec2 TexelSize;
				uint32_t DownscaleFactor;
			} compositePushData;
			compositePushData.Size = size;
			compositePushData.TexelSize = 1.f / glm::vec2(size);
			compositePushData.DownscaleFactor = GetDownscaleFactor();

			m_CompositePipeline->SetImageSampler(m_VolumetricsImageBlurred, Sampler::PointSamplerClamp, 0, 0);
			m_CompositePipeline->SetImageSampler(m_VolumetricsDepthImage, Sampler::PointSamplerClamp, 0, 1);
			m_CompositePipeline->SetImageSampler(gbuffer.Depth, Sampler::PointSamplerClamp, 0, 2);
			m_CompositePipeline->SetImage(input, 0, 3);
			m_CompositePipeline->SetBuffer(m_Renderer.GetCameraMatricesBuffer(), 0, 4);

			cmd->DispatchIndirect(m_CompositePipeline, m_IndirectArgs, sizeof(DispatchIndirectArgs), &compositePushData);
			++stats.Dispatches;
		}

		cmd->TransitionLayout(gbuffer.Depth, ImageReadAccess::PixelShaderRead, depthLayout);
		cmd->TransitionLayout(input, ImageLayoutType::StorageImage, resultLayout);
		cmd->TransitionLayout(m_IndirectArgs, BufferReadAccess::IndirectArgument, BufferLayoutType::StorageBuffer);
	}

	void VolumetricLightTask::InitWithOptions(const SceneRendererSettings& settings)
	{
		const bool bSamplesChanged = m_VolumetricSettings.Samples != settings.VolumetricSettings.Samples;
		const bool bFogChanged = m_VolumetricSettings.bFogEnable != settings.VolumetricSettings.bFogEnable;
		const bool bTranslucentShadowsChanged = bTranslucentShadows != settings.bTranslucentShadows;
		const bool bResolutionChanged = m_VolumetricSettings.EvalResolution != settings.VolumetricSettings.EvalResolution;

		m_VolumetricSettings = settings.VolumetricSettings;
		bTranslucentShadows = settings.bTranslucentShadows;

		if (bSamplesChanged || bFogChanged || bTranslucentShadowsChanged)
			InitPipeline();

		if (!m_VolumetricSettings.bFogEnable)
			m_FogNoise.reset();

		if (bResolutionChanged)
			OnResize(m_Renderer.GetHDROutput()->GetSize());
	}

	void VolumetricLightTask::OnResize(glm::uvec2 size)
	{
		const glm::uvec3 lowResSize = glm::uvec3(GetResourceSize(size), 1u);
		m_VolumetricsImage->Resize(lowResSize);
		m_VolumetricsImageBlurred->Resize(lowResSize);
		m_VolumetricsDepthImage->Resize(lowResSize);
	}

	uint32_t VolumetricLightTask::GetDownscaleFactor() const
	{
		switch (m_VolumetricSettings.EvalResolution)
		{
			case VolumetricLightsSettings::Resolution::Full:
				return 1u;
			case VolumetricLightsSettings::Resolution::Half:
				return 2u;
			case VolumetricLightsSettings::Resolution::Quarter:
				return 4u;
			default:
				EG_CORE_ASSERT(false);
				return 2u;
		}
	}

	glm::uvec2 VolumetricLightTask::GetResourceSize(glm::uvec2 fullResSize) const
	{
		return glm::max(fullResSize / GetDownscaleFactor(), glm::uvec2(1u));
	}

	void VolumetricLightTask::CreateFogNoise(const Ref<CommandBuffer>& cmd)
	{
		ImageSpecifications specs;
		specs.Size = glm::uvec3(EG_FOG_NOISE_TEXTURE_SIZE);
		specs.Format = ImageFormat::R16_Float;
		specs.Usage = ImageUsage::Sampled | ImageUsage::TransferDst;
		specs.Type = ImageType::Type3D;
		m_FogNoise = Image::Create(specs, "Volumetric_FogNoise");

		const std::vector<uint16_t>& data = Noise::GetFogNoiseData();
		cmd->Write(m_FogNoise, data.data(), data.size() * sizeof(uint16_t), ImageLayoutType::Unknown, ImageReadAccess::PixelShaderRead);
	}

	void VolumetricLightTask::UpdateFogNoiseOffsets(float deltaTime)
	{
		// The offsets are in tiles of their layers, wrapped to [0; 1)
		const glm::vec3& windDirection = m_VolumetricSettings.WindDirection;
		const float windDirectionLength = glm::length(windDirection);
		if (windDirectionLength <= 0.f)
			return;

		// In tiles of the base layer. The detail layer's tiles are `EG_FOG_NOISE_DETAIL_SCALE` times smaller, and it moves `EG_FOG_NOISE_DETAIL_SPEED` times faster
		const glm::vec3 displacement = windDirection * (m_VolumetricSettings.FogSpeed * deltaTime / (windDirectionLength * GetFogScale()));
		m_FogNoiseOffsets[0] = glm::fract(m_FogNoiseOffsets[0] + displacement);
		m_FogNoiseOffsets[1] = glm::fract(m_FogNoiseOffsets[1] + displacement * (EG_FOG_NOISE_DETAIL_SCALE * EG_FOG_NOISE_DETAIL_SPEED));
	}

	float VolumetricLightTask::GetFogScale() const
	{
		return glm::max(m_VolumetricSettings.FogScale, VolumetricLightsSettings::MinFogScale);
	}

	ShaderDefines VolumetricLightTask::GetShaderDefines() const
	{
		ShaderDefines defines;
		if (bTranslucentShadows)
			defines["EG_TRANSLUCENT_SHADOWS"] = "";
		if (m_VolumetricSettings.bFogEnable)
			defines["EG_VOLUMETRIC_FOG"] = "";
		return defines;
	}

	void VolumetricLightTask::InitPipeline()
	{
		m_Constants.VolumetricSamples = glm::max(m_VolumetricSettings.Samples, 1u);
		const ShaderDefines defines = GetShaderDefines();

		if (m_Pipeline)
		{
			// The pipeline references `m_Constants`, so it's enough to just recreate it
			const Ref<Shader>& shader = m_Pipeline->GetState().ComputeShader;
			if (shader->GetDefines() != defines)
				shader->SetDefines(defines);
			else
				m_Pipeline->Recreate();
		}
		else
		{
			ShaderSpecializationInfo constants;
			constants.MapEntries.push_back({ 0, uint32_t(offsetof(ConstantData, VolumetricSamples)), sizeof(uint32_t) });
			constants.Data = &m_Constants;
			constants.Size = sizeof(m_Constants);

			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/shade.comp", ShaderType::Compute, defines);
			state.ComputeSpecializationInfo = constants;

			m_Pipeline = PipelineCompute::Create(state);
		}

		if (!m_CompositePipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/composite.comp", ShaderType::Compute);
			m_CompositePipeline = PipelineCompute::Create(state);
		}

		if (!m_BlurPipeline)
		{
			PipelineComputeState state;
			state.ComputeShader = Shader::Create("volumetrics/blur.comp", ShaderType::Compute);
			m_BlurPipeline = PipelineCompute::Create(state);
		}
	}
}
