#pragma once

#include "VidWrappers/Image.h"
#include "VidWrappers/Buffer.h"

#include "Tasks/RendererTask.h"
#include "Tasks/RenderMeshesTask.h" 
#include "Tasks/RenderSpritesTask.h" 
#include "Tasks/RenderLinesTask.h" 
#include "Tasks/RenderBillboardsTask.h" 
#include "Tasks/ShadowPassTask.h" 
#include "Tasks/PBRPassTask.h" 
#include "Tasks/LightsManagerTask.h" 
#include "Tasks/RenderTextUnlitTask.h" 
#include "Tasks/RenderTextLitTask.h" 
#include "Tasks/SSAOTask.h" 
#include "Tasks/GTAOTask.h" 

namespace Eagle
{
	class StaticMeshComponent;
	class SpriteComponent;
	class BillboardComponent;
	class TextComponent;

	class PointLightComponent;
	class SpotLightComponent;
	class DirectionalLightComponent;

	class Texture2D;
	class StaticMesh;
	class Material;
	class TextureCube;
	
	class Camera;

	struct GBuffer
	{
		Ref<Image> AlbedoRoughness; // Albedo Roughness
		Ref<Image> Geometry_Shading_Normals;
		Ref<Image> Emissive;
		Ref<Image> MaterialData; // R: Metallness; G: AO
		Ref<Image> ObjectID;
		Ref<Image> Depth;
		Ref<Image> Motion;

		void Resize(const glm::uvec3& size)
		{
			AlbedoRoughness->Resize(size);
			MaterialData->Resize(size);
			Geometry_Shading_Normals->Resize(size);
			Emissive->Resize(size);
			ObjectID->Resize(size);
			Depth->Resize(size);
			if (Motion)
				Motion->Resize(size);
		}

		void Init(const glm::uvec3& size);
		void InitOptional(const OptionalGBuffers& optional);
	};

	class SceneRenderer : public std::enable_shared_from_this<SceneRenderer>
	{
	public:
		SceneRenderer(const glm::uvec2 size, const SceneRendererSettings& options = {});

		void Render(const Camera* camera, const glm::mat4& viewMat, glm::vec3 viewPosition);

		//---------------------------------- Render functions ----------------------------------
		// For these functions, Renderer copies required data from components
		// If 'bDirty' is false, passed data is ignored and last state is used to render.
		// Else buffers are cleared and required data from components is copied
		void SetMeshes(const std::vector<const StaticMeshComponent*>& meshes, bool bDirty) { m_RenderMeshesTask->SetMeshes(meshes, bDirty); }
		void SetSprites(const std::vector<const SpriteComponent*>& sprites, bool bDirty) { m_RenderSpritesTask->SetSprites(sprites, bDirty); }
		void SetPointLights(const std::vector<const PointLightComponent*>& pointLights, bool bDirty) { m_LightsManagerTask->SetPointLights(pointLights, bDirty); }
		void SetSpotLights(const std::vector<const SpotLightComponent*>& spotLights, bool bDirty) { m_LightsManagerTask->SetSpotLights(spotLights, bDirty); }
		void SetTexts(const std::vector<const TextComponent*>& texts, bool bDirty) { m_RenderUnlitTextTask->SetTexts(texts, bDirty); m_RenderLitTextTask->SetTexts(texts, bDirty); }
		//--------------------------------------------------------------------------------------
		//---------------------------------- Render functions ----------------------------------
		void SetBillboards(const std::vector<const BillboardComponent*>& billboards) { m_RenderBillboardsTask->SetBillboards(billboards); }
		void SetDebugLines(const std::vector<RendererLine>& lines) { m_RenderLinesTask->SetDebugLines(lines); }
		void AddAdditionalBillboard(const Transform& worldTransform, const Ref<Texture2D>& texture, int entityID = -1) { m_RenderBillboardsTask->AddAdditionalBillboard(worldTransform, texture, entityID); } // For internal usage

		// `directionalLight` can be set to nullptr to disable directional light
		void SetDirectionalLight(const DirectionalLightComponent* directionalLight) { m_LightsManagerTask->SetDirectionalLight(directionalLight); }

		// Instead of using `SetMeshes` and triggering all buffers recollection/uploading
		// This function can be used to update transforms of meshes that were already set
		void UpdateMeshesTransforms(const std::set<const StaticMeshComponent*>& meshes) { m_RenderMeshesTask->SetTransforms(meshes); }
		void UpdateSpritesTransforms(const std::set<const SpriteComponent*>& sprites) { m_RenderSpritesTask->SetTransforms(sprites); }
		//--------------------------------------------------------------------------------------

		void SetSkybox(const Ref<TextureCube>& cubemap);
		const Ref<TextureCube>& GetSkybox() const { return m_Cubemap; }

		void SetOptions(const SceneRendererSettings& options);
		void SetViewportSize(const glm::uvec2 size);
		glm::uvec2 GetViewportSize() const { return m_Size; }
		float GetAspectRatio() const { return float(m_Size.x) / float(m_Size.y); }

		// ----------- Getters from other tasks -----------
		// TODO: Implement a proper Render graph with input-output connections between tasks
		const auto& GetMeshes() const { return m_RenderMeshesTask->GetMeshes(); }

		const auto& GetPointLights() const { return m_LightsManagerTask->GetPointLights(); }
		const auto& GetSpotLights() const { return m_LightsManagerTask->GetSpotLights(); }
		const auto& GetDirectionalLight() const { return m_LightsManagerTask->GetDirectionalLight(); }
		bool HasDirectionalLight() const { return m_LightsManagerTask->HasDirectionalLight(); }

		const Ref<Buffer>& GetPointLightsBuffer() const { return m_LightsManagerTask->GetPointLightsBuffer(); }
		const Ref<Buffer>& GetSpotLightsBuffer() const { return m_LightsManagerTask->GetSpotLightsBuffer(); }
		const Ref<Buffer>& GetDirectionalLightBuffer() const { return m_LightsManagerTask->GetDirectionalLightBuffer(); }

		const Ref<Buffer>& GetMeshVertexBuffer() const { return m_RenderMeshesTask->GetVertexBuffer(); }
		const Ref<Buffer>& GetMeshInstanceVertexBuffer() const { return m_RenderMeshesTask->GetInstanceVertexBuffer(); }
		const Ref<Buffer>& GetMeshIndexBuffer() const { return m_RenderMeshesTask->GetIndexBuffer(); }
		const Ref<Buffer>& GetMeshTransformsBuffer() const { return m_RenderMeshesTask->GetMeshTransformsBuffer(); }

		const std::vector<RenderSpritesTask::QuadVertex>& GetSpritesVertices() const { return m_RenderSpritesTask->GetVertices(); }
		const Ref<Buffer>& GetSpritesVertexBuffer() const { return m_RenderSpritesTask->GetVertexBuffer(); }
		const Ref<Buffer>& GetSpritesIndexBuffer() const { return m_RenderSpritesTask->GetIndexBuffer(); }
		const Ref<Buffer>& GetSpritesTransformsBuffer() const { return m_RenderSpritesTask->GetSpritesTransformsBuffer(); }

		const std::vector<Ref<Image>>& GetPointLightShadowMaps() const { return m_ShadowPassTask->GetPointLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetSpotLightShadowMaps() const { return m_ShadowPassTask->GetSpotLightShadowMaps(); }
		const std::vector<Ref<Image>>& GetDirectionalLightShadowMaps() const { return m_ShadowPassTask->GetDirectionalLightShadowMaps(); }

		const std::vector<Ref<Sampler>>& GetPointLightShadowMapsSamplers() const { return m_ShadowPassTask->GetPointLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetSpotLightShadowMapsSamplers() const { return m_ShadowPassTask->GetSpotLightShadowMapsSamplers(); }
		const std::vector<Ref<Sampler>>& GetDirectionalLightShadowMapsSamplers() const { return m_ShadowPassTask->GetDirectionalLightShadowMapsSamplers(); }

		const Ref<Image>& GetSSAOResult() const { return m_SSAOTask->GetResult(); }
		const Ref<Image>& GetGTAOResult() const { return m_GTAOTask->GetResult(); }
		// ------------------------------------------------

		const GBuffer& GetGBuffer() const { return m_GBuffer; }
		GBuffer& GetGBuffer() { return m_GBuffer; }
		const Ref<Image>& GetOutput() const { return m_FinalImage; }
		const Ref<Image>& GetHDROutput() const { return m_HDRRTImage; }

		const SceneRendererSettings& GetOptions_RT() const { return m_Options_RT; }
		const SceneRendererSettings& GetOptions() const { return m_Options; }
		const glm::mat4& GetViewMatrix() const { return m_View; }
		const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }
		const glm::vec3 GetViewPosition() const { return m_ViewPos; }
		float GetPhotoLinearScale() const { return m_PhotoLinearScale; }

		// Prev frame data
		const glm::mat4& GetPrevViewMatrix() const { return m_PrevView; }
		const glm::mat4& GetPrevProjectionMatrix() const { return m_PrevProjection; }
		const glm::mat4& GetPrevViewProjection() const { return m_PrevViewProjection; }

		const std::vector<glm::mat4>& GetCascadeProjections() const { return m_CameraCascadeProjections; }
		const std::vector<float>& GetCascadeFarPlanes() const { return m_CameraCascadeFarPlanes; }

	private:
		void InitWithOptions();

	private:
		Scope<RenderMeshesTask> m_RenderMeshesTask;
		Scope<RenderSpritesTask> m_RenderSpritesTask;
		Scope<RenderTextLitTask> m_RenderLitTextTask;
		Scope<RenderTextUnlitTask> m_RenderUnlitTextTask;
		Scope<LightsManagerTask> m_LightsManagerTask;
		Scope<RenderLinesTask> m_RenderLinesTask;
		Scope<RenderBillboardsTask> m_RenderBillboardsTask;
		Scope<PBRPassTask> m_PBRPassTask;
		Scope<ShadowPassTask> m_ShadowPassTask;
		Scope<RendererTask> m_BloomTask;
		Scope<RendererTask> m_SkyboxPassTask;
		Scope<RendererTask> m_PostProcessingPassTask;
		Scope<SSAOTask> m_SSAOTask;
		Scope<GTAOTask> m_GTAOTask;
		
		GBuffer m_GBuffer;
		Ref<Image> m_FinalImage;
		Ref<Image> m_HDRRTImage; // Render target
		Ref<TextureCube> m_Cubemap;
		glm::mat4 m_View = glm::mat4(1.f);
		glm::mat4 m_Projection = glm::mat4(1.f);
		glm::mat4 m_ViewProjection = glm::mat4(1.f);
		glm::vec3 m_ViewPos = glm::vec3(0.f);

		// Prev frame data
		glm::mat4 m_PrevView = glm::mat4(1.f);
		glm::mat4 m_PrevProjection = glm::mat4(1.f);
		glm::mat4 m_PrevViewProjection = glm::mat4(1.f);

		std::vector<glm::mat4> m_CameraCascadeProjections = std::vector<glm::mat4>(EG_CASCADES_COUNT);
		std::vector<float> m_CameraCascadeFarPlanes = std::vector<float>(EG_CASCADES_COUNT);

		glm::uvec2 m_Size = { 1, 1 };
		float m_PhotoLinearScale = 1.f;
		SceneRendererSettings m_Options_RT; // Render thread
		SceneRendererSettings m_Options;

		uint32_t m_FrameIndex = 0;
	};
}
