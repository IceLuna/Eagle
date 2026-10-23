#include "egpch.h"
#include "Scene.h"

#include "Eagle/Physics/PhysicsScene.h"
#include "Eagle/AI/NavigationDebugDraw.h"
#include "Eagle/Classes/SkeletalMesh.h"

namespace Eagle
{
	namespace Utils
	{
		constexpr uint32_t s_SphereLinesCount = 24;
		constexpr float s_PI = glm::pi<float>();
		constexpr float s_2PI = 2.f * s_PI;

		// Builds an orthonormal basis (right, up, forward) from an arbitrary up vector
		void BuildBasisFromUp(const glm::vec3& up, glm::vec3& outRight, glm::vec3& outUp, glm::vec3& outForward)
		{
			outUp = glm::normalize(up);

			// Pick a reference axis unlikely to be parallel to up
			glm::vec3 reference = (glm::abs(outUp.y) < 0.999f) ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f);

			outRight = glm::normalize(glm::cross(reference, outUp));
			outForward = glm::cross(outUp, outRight);
		}

		void DrawSphere(std::vector<RendererLine>& buffer, const glm::vec3& center, const glm::vec3& color, float radius)
		{
			for (uint32_t i = 0; i < s_SphereLinesCount; ++i)
			{
				const float angle1 = (float(i) / s_SphereLinesCount) * s_2PI;
				const float angle2 = (float(i + 1) / s_SphereLinesCount) * s_2PI;
				const float cosAngle1 = glm::cos(angle1);
				const float cosAngle2 = glm::cos(angle2);
				const float sinAngle1 = glm::sin(angle1);
				const float sinAngle2 = glm::sin(angle2);
				constexpr float cos45 = 0.707106f;
				constexpr float cosMinus45 = -0.707106f;

				auto& line = buffer.emplace_back();
				line.Start.Location = center + radius * glm::vec3(cosAngle1, sinAngle1, 0.f);
				line.End.Location = center + radius * glm::vec3(cosAngle2, sinAngle2, 0.f);
				line.Start.Color = color;
				line.End.Color = color;

				auto& line2 = buffer.emplace_back();
				line2.Start.Location = center + radius * glm::vec3(0.f, cosAngle1, sinAngle1);
				line2.End.Location = center + radius * glm::vec3(0.f, cosAngle2, sinAngle2);
				line2.Start.Color = color;
				line2.End.Color = color;

				auto& line3 = buffer.emplace_back();
				line3.Start.Location = center + radius * glm::vec3(cos45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line3.End.Location = center + radius * glm::vec3(cos45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line3.Start.Color = color;
				line3.End.Color = color;

				auto& line4 = buffer.emplace_back();
				line4.Start.Location = center + radius * glm::vec3(cosMinus45 * sinAngle1, cosAngle1, sinAngle1 * cos45);
				line4.End.Location = center + radius * glm::vec3(cosMinus45 * sinAngle2, cosAngle2, sinAngle2 * cos45);
				line4.Start.Color = color;
				line4.End.Color = color;
			}
		}

		void DrawBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.FindBone(node.GetNameHash()); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				const glm::mat4 boneTransform = Math::ToTransformMatrix(bone);
				tr = baseTransform * boneTransform;
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);
				
				const glm::vec3 location = Math::DecomposeTransformMatrix(tr * child.Transformation).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawBones_Internal(buffer, child, currentPose, tr);
		}

		void DrawRagdollBones_Internal(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, const glm::mat4& worldTransform, const glm::mat4& baseTransform)
		{
			glm::mat4 tr;
			if (auto it = currentPose.FindBone(node.GetNameHash()); it != currentPose.Bones.end())
			{
				const auto& bone = it->second;
				tr = Math::ToTransformMatrix(bone); // Ragdoll is already a global transform
			}
			else
				tr = baseTransform * node.Transformation;

			const glm::vec3 parentLocation = Math::DecomposeTransformMatrix(worldTransform * tr).Location;
			for (const auto& child : node.Children)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = parentLocation;
				line.Start.Color = glm::vec3(0, 1, 0);

				const glm::vec3 location = Math::DecomposeTransformMatrix(worldTransform * (tr * child.Transformation)).Location;
				line.End.Location = location;
				line.End.Color = glm::vec3(1, 1, 0);
			}

			for (const auto& child : node.Children)
				DrawRagdollBones_Internal(buffer, child, currentPose, worldTransform, tr);
		}

		void DrawBones(std::vector<RendererLine>& buffer, const BoneNode& node, const SkeletalPose& currentPose, bool bRagdoll, const glm::mat4& baseTransform)
		{
			if (bRagdoll)
				DrawRagdollBones_Internal(buffer, node, currentPose, baseTransform, baseTransform);
			else
				DrawBones_Internal(buffer, node, currentPose, baseTransform);
		}
	
		void DrawBox(std::vector<RendererLine>& buffer, AABB aabb, const Transform& worldTr, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			const glm::mat4 trMat = Math::ToTransformMatrix(worldTr);
			const size_t startIdx = buffer.size();

			for (glm::length_t i = 0; i < aabb.Min.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;

				line.End.Location = aabb.Min;
				line.End.Location[i] = aabb.Max[i];
			}

			for (glm::length_t i = 0; i < aabb.Max.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Max;

				line.End.Location = aabb.Max;
				line.End.Location[i] = aabb.Min[i];
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}
		
			for (size_t i = startIdx; i < buffer.size(); ++i)
			{
				auto& line = buffer[i];
				line.Start.Location = trMat * glm::vec4(line.Start.Location, 1.f);
				line.End.Location = trMat * glm::vec4(line.End.Location, 1.f);
			}
		}

		void DrawAABB(std::vector<RendererLine>& buffer, AABB aabb, const Transform& worldTr, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			const glm::mat4 trMat = Math::ToTransformMatrix(worldTr);
			const size_t startIdx = buffer.size();

			aabb.Transform(trMat);

			for (glm::length_t i = 0; i < aabb.Min.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;

				line.End.Location = aabb.Min;
				line.End.Location[i] = aabb.Max[i];
			}

			for (glm::length_t i = 0; i < aabb.Max.length(); ++i)
			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Max;

				line.End.Location = aabb.Max;
				line.End.Location[i] = aabb.Min[i];
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.y = aabb.Max.y;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.z = aabb.Max.z;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.x = aabb.Max.x;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.y = aabb.Max.y;
			}

			{
				auto& line = buffer.emplace_back();
				line.Start.Color = color;
				line.End.Color = color;
				line.Start.Location = aabb.Min;
				line.Start.Location.z = aabb.Max.z;

				line.End.Location = line.Start.Location;
				line.End.Location.x = aabb.Max.x;
			}
		}

		void DrawFrustum(std::vector<RendererLine>& lines, const Camera& camera, const Transform& tr, float aspect, const glm::vec3& color = glm::vec3(0, 1, 0))
		{
			auto AddLine = [](std::vector<RendererLine>& lines,
				const glm::vec3& a,
				const glm::vec3& b,
				const glm::vec3& color)
			{
				RendererLine l;
				l.Start.Location = a;
				l.Start.Color = color;
				l.End.Location = b;
				l.End.Color = color;
				lines.push_back(l);
			};

			const float fovY = camera.GetPerspectiveVerticalFOV();
			const float znear = camera.GetPerspectiveNearClip();
			const float zfar = camera.GetPerspectiveFarClip();

			const glm::vec3& pos = tr.Location;
			const glm::vec3 forward = Math::GetForwardVector(tr.Rotation);
			const glm::vec3 up = Math::GetUpVector(tr.Rotation);
			const glm::vec3 right = Math::GetRightVector(tr.Rotation);

			float tanHalfFov = tanf(fovY * 0.5f);

			float nearH = 2.0f * tanHalfFov * znear;
			float nearW = nearH * aspect;

			float farH = 2.0f * tanHalfFov * zfar;
			float farW = farH * aspect;

			glm::vec3 nc = pos + forward * znear;
			glm::vec3 fc = pos + forward * zfar;

			// Near corners
			glm::vec3 ntl = nc + (up * nearH * 0.5f) - (right * nearW * 0.5f);
			glm::vec3 ntr = nc + (up * nearH * 0.5f) + (right * nearW * 0.5f);
			glm::vec3 nbl = nc - (up * nearH * 0.5f) - (right * nearW * 0.5f);
			glm::vec3 nbr = nc - (up * nearH * 0.5f) + (right * nearW * 0.5f);

			// Far corners
			glm::vec3 ftl = fc + (up * farH * 0.5f) - (right * farW * 0.5f);
			glm::vec3 ftr = fc + (up * farH * 0.5f) + (right * farW * 0.5f);
			glm::vec3 fbl = fc - (up * farH * 0.5f) - (right * farW * 0.5f);
			glm::vec3 fbr = fc - (up * farH * 0.5f) + (right * farW * 0.5f);

			// Colors
			const glm::vec3 edgeColor = color; // (1, 1, 0);
			const glm::vec3 nearColor = color; // (0, 1, 1);
			const glm::vec3 farColor = color; // (0, 0.5f, 1);
			const glm::vec3 sliceColor = color; // (1, 0.5f, 0);
			const glm::vec3 diagColor = color; // (1, 0, 0);

			// Near
			AddLine(lines, ntl, ntr, edgeColor);
			AddLine(lines, ntr, nbr, edgeColor);
			AddLine(lines, nbr, nbl, edgeColor);
			AddLine(lines, nbl, ntl, edgeColor);

			// Far
			AddLine(lines, ftl, ftr, edgeColor);
			AddLine(lines, ftr, fbr, edgeColor);
			AddLine(lines, fbr, fbl, edgeColor);
			AddLine(lines, fbl, ftl, edgeColor);

			// Connections
			AddLine(lines, ntl, ftl, edgeColor);
			AddLine(lines, ntr, ftr, edgeColor);
			AddLine(lines, nbl, fbl, edgeColor);
			AddLine(lines, nbr, fbr, edgeColor);

			// Diagonals (orientation)
			AddLine(lines, ntl, nbr, diagColor);
			AddLine(lines, ntr, nbl, diagColor);
			AddLine(lines, ftl, fbr, diagColor);
			AddLine(lines, ftr, fbl, diagColor);

			// Near plane grid
			const int gridSteps = 1;
			for (int i = 1; i < gridSteps; i++)
			{
				float t = i / (float)gridSteps;
			
				glm::vec3 left = glm::mix(nbl, ntl, t);
				glm::vec3 right = glm::mix(nbr, ntr, t);
				AddLine(lines, left, right, nearColor);
			
				glm::vec3 bottom = glm::mix(nbl, nbr, t);
				glm::vec3 top = glm::mix(ntl, ntr, t);
				AddLine(lines, bottom, top, nearColor);
			}

			// Far plane grid
			for (int i = 1; i < gridSteps; i++)
			{
				float t = i / (float)gridSteps;
			
				glm::vec3 left = glm::mix(fbl, ftl, t);
				glm::vec3 right = glm::mix(fbr, ftr, t);
				AddLine(lines, left, right, farColor);
			
				glm::vec3 bottom = glm::mix(fbl, fbr, t);
				glm::vec3 top = glm::mix(ftl, ftr, t);
				AddLine(lines, bottom, top, farColor);
			}

			// Depth slices
			const int depthSlices = 3;
			for (int i = 1; i <= depthSlices; i++)
			{
				float t = i / (float)(depthSlices + 1);
				float z = glm::mix(znear, zfar, t);

				float h = 2.0f * tanHalfFov * z;
				float w = h * aspect;

				glm::vec3 center = pos + forward * z;

				glm::vec3 tl = center + (up * h * 0.5f) - (right * w * 0.5f);
				glm::vec3 tr = center + (up * h * 0.5f) + (right * w * 0.5f);
				glm::vec3 bl = center - (up * h * 0.5f) - (right * w * 0.5f);
				glm::vec3 br = center - (up * h * 0.5f) + (right * w * 0.5f);

				AddLine(lines, tl, tr, sliceColor);
				AddLine(lines, tr, br, sliceColor);
				AddLine(lines, br, bl, sliceColor);
				AddLine(lines, bl, tl, sliceColor);
			}
		}

		// Draws a capsule oriented along the up-axis (Y), consistent with PxControllerDesc::upDirection = (0,1,0)
		// 'radius' and 'halfHeight' should already include contact offset where relevant (see DrawCCTCapsule below)
		void DrawCapsule_CCT(std::vector<RendererLine>& buffer, const glm::vec3& center, const glm::vec3& up, float radius, float halfHeight)
		{
			constexpr glm::vec3 color = glm::vec3(0, 1, 0);

			glm::vec3 right, upAxis, forward;
			BuildBasisFromUp(up, right, upAxis, forward);

			const glm::vec3 topCenter = center + upAxis * halfHeight;
			const glm::vec3 bottomCenter = center - upAxis * halfHeight;

			for (uint32_t i = 0; i < s_SphereLinesCount; ++i)
			{
				const float angle1 = (float(i) / s_SphereLinesCount) * s_2PI;
				const float angle2 = (float(i + 1) / s_SphereLinesCount) * s_2PI;
				const float cosAngle1 = glm::cos(angle1);
				const float cosAngle2 = glm::cos(angle2);
				const float sinAngle1 = glm::sin(angle1);
				const float sinAngle2 = glm::sin(angle2);

				// Equatorial rings (in the right/forward plane, perpendicular to up)
				{
					auto& line = buffer.emplace_back();
					line.Start.Location = topCenter + radius * (cosAngle1 * right + sinAngle1 * forward);
					line.End.Location = topCenter + radius * (cosAngle2 * right + sinAngle2 * forward);
					line.Start.Color = color;
					line.End.Color = color;
				}
				{
					auto& line = buffer.emplace_back();
					line.Start.Location = bottomCenter + radius * (cosAngle1 * right + sinAngle1 * forward);
					line.End.Location = bottomCenter + radius * (cosAngle2 * right + sinAngle2 * forward);
					line.Start.Color = color;
					line.End.Color = color;
				}

				// Top hemisphere arcs (right/up plane and forward/up plane, upper half only)
				{
					const float a1 = (float(i) / s_SphereLinesCount) * s_PI;
					const float a2 = (float(i + 1) / s_SphereLinesCount) * s_PI;
					auto& line = buffer.emplace_back();
					line.Start.Location = topCenter + radius * (glm::cos(a1) * right + glm::sin(a1) * upAxis);
					line.End.Location = topCenter + radius * (glm::cos(a2) * right + glm::sin(a2) * upAxis);
					line.Start.Color = color;
					line.End.Color = color;

					auto& line2 = buffer.emplace_back();
					line2.Start.Location = topCenter + radius * (glm::sin(a1) * upAxis + glm::cos(a1) * forward);
					line2.End.Location = topCenter + radius * (glm::sin(a2) * upAxis + glm::cos(a2) * forward);
					line2.Start.Color = color;
					line2.End.Color = color;
				}

				// Bottom hemisphere arcs (lower half only)
				{
					const float a1 = s_PI + (float(i) / s_SphereLinesCount) * s_PI;
					const float a2 = s_PI + (float(i + 1) / s_SphereLinesCount) * s_PI;
					auto& line = buffer.emplace_back();
					line.Start.Location = bottomCenter + radius * (glm::cos(a1) * right + glm::sin(a1) * upAxis);
					line.End.Location = bottomCenter + radius * (glm::cos(a2) * right + glm::sin(a2) * upAxis);
					line.Start.Color = color;
					line.End.Color = color;

					auto& line2 = buffer.emplace_back();
					line2.Start.Location = bottomCenter + radius * (glm::sin(a1) * upAxis + glm::cos(a1) * forward);
					line2.End.Location = bottomCenter + radius * (glm::sin(a2) * upAxis + glm::cos(a2) * forward);
					line2.Start.Color = color;
					line2.End.Color = color;
				}
			}

			// Vertical side lines connecting the two rings (the cylinder body)
			constexpr int sideCount = 4;
			for (int i = 0; i < sideCount; ++i)
			{
				const float angle = (float(i) / sideCount) * s_2PI;
				const glm::vec3 offset = radius * (glm::cos(angle) * right + glm::sin(angle) * forward);
				auto& line = buffer.emplace_back();
				line.Start.Location = topCenter + offset;
				line.End.Location = bottomCenter + offset;
				line.Start.Color = color;
				line.End.Color = color;
			}
		}

		// Draws a box, half extents given directly (already inflated by contact offset where relevant)
		void DrawBox_CCT(std::vector<RendererLine>& buffer, const glm::vec3& center, const glm::vec3& up, const glm::vec3& halfExtent)
		{
			constexpr glm::vec3 color = glm::vec3(0, 1, 0);

			glm::vec3 right, upAxis, forward;
			BuildBasisFromUp(up, right, upAxis, forward);

			const glm::vec3 corners[8] =
			{
				center - right * halfExtent.x - upAxis * halfExtent.y - forward * halfExtent.z,
				center + right * halfExtent.x - upAxis * halfExtent.y - forward * halfExtent.z,
				center + right * halfExtent.x - upAxis * halfExtent.y + forward * halfExtent.z,
				center - right * halfExtent.x - upAxis * halfExtent.y + forward * halfExtent.z,
				center - right * halfExtent.x + upAxis * halfExtent.y - forward * halfExtent.z,
				center + right * halfExtent.x + upAxis * halfExtent.y - forward * halfExtent.z,
				center + right * halfExtent.x + upAxis * halfExtent.y + forward * halfExtent.z,
				center - right * halfExtent.x + upAxis * halfExtent.y + forward * halfExtent.z,
			};

			constexpr int edges[12][2] =
			{
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // bottom face
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // top face
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, // vertical edges
			};

			for (auto& edge : edges)
			{
				auto& line = buffer.emplace_back();
				line.Start.Location = corners[edge[0]];
				line.End.Location = corners[edge[1]];
				line.Start.Color = color;
				line.End.Color = color;
			}
		}

		// Convenience wrappers that pull the right numbers straight from the controller,
		// including contact offset inflation, so the debug draw matches what PhysX is actually colliding against.
		void DrawCCTCapsule(std::vector<RendererLine>& buffer, const CharacterControllerComponent& controller)
		{
			const glm::vec3 center = controller.GetControllerWorldLocation();

			const float contactOffset = controller.GetContactOffset();
			const float radius = controller.GetScaledCapsuleRadius() + contactOffset;
			const float halfHeight = controller.GetScaledCapsuleHeight() * 0.5f;

			DrawCapsule_CCT(buffer, center, controller.GetControllerUpDirection(), radius, halfHeight);
		}

		void DrawCCTBox(std::vector<RendererLine>& buffer, const CharacterControllerComponent& controller)
		{
			const glm::vec3 center = controller.GetControllerWorldLocation();

			const float contactOffset = controller.GetContactOffset();
			const glm::vec3 halfExtent = (controller.GetScaledBoxSize() * 0.5f) + contactOffset;

			const glm::vec3 upDir = controller.GetControllerUpDirection();
			const glm::vec3 up = glm::length2(upDir) < 1e-5 ? glm::vec3(0, 1, 0) : glm::normalize(upDir);

			DrawBox_CCT(buffer, center, up, halfExtent);
		}

		void DrawCCT(std::vector<RendererLine>& buffer, const CharacterControllerComponent& controller)
		{
			switch (controller.GetShapeType())
			{
			case CharacterControllerShape::Box:
				Utils::DrawCCTBox(buffer, controller);
				break;
			case CharacterControllerShape::Capsule:
				Utils::DrawCCTCapsule(buffer, controller);
				break;
			default:
				EG_CORE_ASSERT(!"Unknown type");
			}
		}

		void DrawMesh(std::vector<RendererLine>& buffer, const glm::mat4& tr, const std::vector<glm::vec3>& vertices, const std::vector<Index>& indices)
		{
			for (uint32_t i = 0; i < indices.size(); i += 3)
			{
				auto& line1 = buffer.emplace_back();
				line1.Start.Location = tr * glm::vec4(vertices[indices[i]], 1.f);
				line1.End.Location = tr * glm::vec4(vertices[indices[i + 1]], 1.f);

				auto& line2 = buffer.emplace_back();
				line2.Start.Location = tr * glm::vec4(vertices[indices[i]], 1.f);
				line2.End.Location = tr * glm::vec4(vertices[indices[i + 2]], 1.f);

				auto& line3 = buffer.emplace_back();
				line3.Start.Location = tr * glm::vec4(vertices[indices[i + 1]], 1.f);
				line3.End.Location = tr * glm::vec4(vertices[indices[i + 2]], 1.f);
			}
		}
	}

	// Returns false if the rest of RenderScene() must be skipped (keeps the early-outs the code had when it was inlined there)
	bool Scene::GatherDebugDrawData()
	{
		auto& rb = m_PhysicsScene->GetRenderBuffer();
		const uint32_t debugCollisionsLinesSize = rb.getNbLines();

		constexpr size_t linesPerDirLight = 3ull;
		size_t debugDirLightLinesCount = 0;
		auto dirLightsView = m_Registry.view<DirectionalLightComponent>();
		debugDirLightLinesCount = dirLightsView.size() * linesPerDirLight;

		m_DebugLinesToDraw.clear();
		m_DebugLinesToDraw.reserve(debugCollisionsLinesSize + m_UserDebugLines.size() + debugDirLightLinesCount);
		m_DebugTrianglesToDraw.clear();

		// Gather Debug data
		// Debug point lights attenuation radii
		{
			for (auto& lightEntityID : m_PointLightsDebugRadii)
			{
				Entity entity((entt::entity)lightEntityID, this);
				if (!entity.HasComponent<PointLightComponent>())
					return false;

				const auto& light = entity.GetComponent<PointLightComponent>();
				const glm::vec3& center = light.GetWorldTransform().Location;
				const float radius = light.GetRadius();
				Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(0, 1, 0), radius);
			}
		}

		// Debug spot lights attenuation distance
		{
			for (auto& lightEntityID : m_SpotLightsDebugRadii)
			{
				Entity entity((entt::entity)lightEntityID, this);
				if (!entity.HasComponent<SpotLightComponent>())
					return false;

				const auto& light = entity.GetComponent<SpotLightComponent>();

				const glm::vec3& location = light.GetWorldTransform().Location;
				const float distance = light.GetDistance();
				const glm::vec3 center = location + light.GetForwardVector() * distance;
				const glm::quat quat = light.GetWorldTransform().Rotation.GetQuat();
				const float innerRadius = distance * glm::tan(glm::radians(light.GetInnerCutOffAngle()));
				const float outerRadius = distance * glm::tan(glm::radians(light.GetOuterCutOffAngle()));

				for (uint32_t i = 0; i < Utils::s_SphereLinesCount; ++i)
				{
					const float angle1 = (float(i) / Utils::s_SphereLinesCount) * Utils::s_2PI;
					const float angle2 = (float(i + 1) / Utils::s_SphereLinesCount) * Utils::s_2PI;
					const float cosAngle1 = glm::cos(angle1);
					const float cosAngle2 = glm::cos(angle2);
					const float sinAngle1 = glm::sin(angle1);
					const float sinAngle2 = glm::sin(angle2);

					const glm::vec3 innerStart = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
					auto& innerCircleLine = m_DebugLinesToDraw.emplace_back();
					innerCircleLine.Start.Location = innerStart;
					innerCircleLine.End.Location = center + glm::rotate(quat, innerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));

					auto& toInnerLine = m_DebugLinesToDraw.emplace_back();
					toInnerLine.Start.Location = location;
					toInnerLine.End.Location = innerStart;

					const glm::vec3 outerStart = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle1, sinAngle1, 0.f));
					auto& outerCircleLine = m_DebugLinesToDraw.emplace_back();
					outerCircleLine.Start.Location = outerStart;
					outerCircleLine.End.Location = center + glm::rotate(quat, outerRadius * glm::vec3(cosAngle2, sinAngle2, 0.f));
					outerCircleLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
					outerCircleLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);

					auto& toOuterLine = m_DebugLinesToDraw.emplace_back();
					toOuterLine.Start.Location = location;
					toOuterLine.End.Location = outerStart;
					toOuterLine.Start.Color = glm::vec3(0.75, 0.75f, 0.f);
					toOuterLine.End.Color = glm::vec3(0.75, 0.75f, 0.f);
				}
			}
		}

		// Debug reverb boxes distance
		{
			for (auto& reverbEntityID : m_ReverbDebugBoxes)
			{
				Entity entity((entt::entity)reverbEntityID, this);
				if (!entity.HasComponent<ReverbComponent>())
					return false;

				const auto& reverb = entity.GetComponent<ReverbComponent>();
				const glm::vec3& center = reverb.GetReverb()->GetPosition();
				Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(0, 1, 0), reverb.GetMinDistance());
				Utils::DrawSphere(m_DebugLinesToDraw, center, glm::vec3(1, 0, 0), reverb.GetMaxDistance());
			}
		}

		// Debug dir lights direction
		{
			for (auto& lightEntityID : m_DirLightsDebugDirection)
			{
				Entity entity((entt::entity)lightEntityID, this);
				if (!entity.HasComponent<DirectionalLightComponent>())
					return false;

				const auto& dir = entity.GetComponent<DirectionalLightComponent>();
				const glm::vec3& location = dir.GetWorldTransform().Location;
				const glm::vec3 forward = dir.GetForwardVector();
				const glm::vec3 endLocation = location + forward * 0.2f;

				DrawArrow(location, endLocation, dir.GetUpVector());
			}
		}

		// Debug collisions
		if (debugCollisionsLinesSize)
		{
			const physx::PxDebugLine* physicsLines = rb.getLines();

			for (uint32_t i = 0; i < debugCollisionsLinesSize; ++i)
			{
				auto& line = physicsLines[i];
				RendererLine rendererLine;
				rendererLine.Start.Location = PhysXUtils::FromPhysXVector(line.pos0);
				rendererLine.End.Location = PhysXUtils::FromPhysXVector(line.pos1);
				m_DebugLinesToDraw.push_back(rendererLine);
			}
		}

		// Debug CCT
		{
			if (IsForcingShowCollision())
			{
				auto view = m_Registry.view<CharacterControllerComponent>();
				for (auto entity : view)
				{
					auto& component = view.get<CharacterControllerComponent>(entity);
					Utils::DrawCCT(m_DebugLinesToDraw, component);
				}
			}
			else
			{
				for (const auto& entID : m_CharacterControllerDebug)
				{
					Entity entity((entt::entity)entID, this);
					if (!entity.HasComponent<CharacterControllerComponent>())
						return false;
					Utils::DrawCCT(m_DebugLinesToDraw, entity.GetComponent<CharacterControllerComponent>());
				}
			}
		}

		// Bones
		if (bDrawBones)
		{
			auto view = m_Registry.view<SkeletalMeshComponent>();
			for (auto entity : view)
			{
				auto& skeletal = view.get<SkeletalMeshComponent>(entity);
				if (auto& asset = skeletal.GetMeshAsset())
					Utils::DrawBones(m_DebugLinesToDraw, asset->GetMesh()->GetSkeletalMeshInfo().RootBone, skeletal.LastPose, skeletal.IsRagdollEnabled(), Math::ToTransformMatrix(skeletal.GetWorldTransform()));
			}
		}

		// AABBs
		if (true)
		{
			if (bDrawMeshAABBs)
			{
				auto view = m_Registry.view<SkeletalMeshComponent>();
				for (auto entity : view)
				{
					const auto& skeletal = view.get<SkeletalMeshComponent>(entity);
					if (const auto& asset = skeletal.GetMeshAsset())
						Utils::DrawAABB(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), skeletal.GetWorldTransform());
				}
			}
			if (bDrawMeshAABBs)
			{
				auto view = m_Registry.view<StaticMeshComponent>();
				for (auto entity : view)
				{
					const auto& staticMesh = view.get<StaticMeshComponent>(entity);
					if (const auto& asset = staticMesh.GetMeshAsset())
						Utils::DrawAABB(m_DebugLinesToDraw, asset->GetMesh()->GetAABB(), staticMesh.GetWorldTransform());
				}
			}
			if (false)
			{
				const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
				auto view = m_Registry.view<CameraComponent>();
				for (auto entity : view)
				{
					const auto& camera = view.get<CameraComponent>(entity);
					Utils::DrawFrustum(m_DebugLinesToDraw, camera.Camera, camera.GetWorldTransform(), aspect);
				}
			}
			if (false)
			{
				auto view = m_Registry.view<ParticleSystemComponent>();
				for (auto entity : view)
				{
					const auto& system = view.get<ParticleSystemComponent>(entity);
					const auto& asset = system.GetAsset();
					if (!asset)
						continue;

					for (const auto& emitter : asset->GetEmitters())
					{
						Utils::DrawBox(m_DebugLinesToDraw, emitter.VisibilityAABB, system.GetWorldTransform());
					}
				}
			}
			if (bDrawNavMesh)
			{
				auto view = m_Registry.view<NavigationMeshComponent>();
				for (auto entity : view)
				{
					const auto& navigation = view.get<NavigationMeshComponent>(entity);
					const auto& settings = navigation.GetSettings();
					const auto& aabb = settings.AABB;
					Utils::DrawBox(m_DebugLinesToDraw, aabb, navigation.GetWorldTransform(), glm::vec3(1, 0, 0));

					AINavigation::DebugDraw debugDraw(m_DebugLinesToDraw, m_DebugTrianglesToDraw);
					navigation.GetNavMeshDebugDraw(&debugDraw);
				}
			}
			if (false)
			{
				if (m_CurrentNavMesh)
				{
					const glm::vec3 start = { 2.1703f, 0.f, -2.2557f };
					const glm::vec3 end = { -2.1192f, 0.f, 2.3706 };
					std::vector<glm::vec3> path = m_CurrentNavMesh->FindSmoothPath(start, end);
					if (!path.empty())
					{
						const size_t count = path.size();
						glm::vec3 startPos = path.front();
						for (size_t i = 1; i < count; i++)
						{
							RendererLine line;
							line.Start.Location = startPos;
							line.End.Location = path[i];
							DrawDebugLine(line);
							startPos = path[i];
						}
					}
				}
			}
			if (false)
			{
				auto view = m_Registry.view<BoxColliderComponent>();
				for (auto entity : view)
				{
					const auto& component = view.get<BoxColliderComponent>(entity);
					const auto& shape = component.GetShape();
					std::vector<glm::vec3> vertices;
					std::vector<uint32_t> indices;

					Transform pose = shape->GetLocalTransform();
					Transform tBody = shape->GetGlobalTransform();
					glm::mat4 t = Math::ToTransformMatrix(tBody + pose);

					shape->GetGeometry(vertices, indices);
					Utils::DrawMesh(m_DebugLinesToDraw, t, vertices, indices);
				}
			}
			if (false)
			{
				auto view = m_Registry.view<CapsuleColliderComponent>();
				for (auto entity : view)
				{
					const auto& component = view.get<CapsuleColliderComponent>(entity);
					const auto& shape = component.GetShape();
					const glm::vec3& location = component.GetWorldTransform().Location;

					const glm::vec3& scale = shape->GetColliderScale();
					const float& radius = scale.x;
					const float& height = scale.y;

					const glm::vec3 halfExtent1 = glm::vec3(radius, 0.f, radius);
					const glm::vec3 halfExtent2 = glm::vec3(radius, height, radius);
					AABB aabb(location - halfExtent1, location + halfExtent2);

					Utils::DrawBox(m_DebugLinesToDraw, aabb, {}, glm::vec3(0, 0, 1));
				}
			}
			for (const auto& [aabb, transform] : m_UserAABBs)
			{
				Utils::DrawAABB(m_DebugLinesToDraw, aabb, transform);
			}
			for (const auto& [aabb, transform] : m_UserBoxes)
			{
				Utils::DrawBox(m_DebugLinesToDraw, aabb, transform);
			}
			m_UserAABBs.clear();
			m_UserBoxes.clear();
		}

		AddTextUIBoundsDebugLines();

		// Append user provided lines
		m_DebugLinesToDraw.insert(m_DebugLinesToDraw.end(), m_UserDebugLines.begin(), m_UserDebugLines.end());
		m_DebugTrianglesToDraw.insert(m_DebugTrianglesToDraw.end(), m_UserDebugTriangles.begin(), m_UserDebugTriangles.end());
		// User provided lines need to provided each frame. So clear it.
		m_UserDebugLines.clear();
		m_UserDebugTriangles.clear();

		return true;
	}

	void Scene::AddTextUIBoundsDebugLines()
	{
		m_DebugLines2DToDraw.clear();

		Entity selectedEntity = TextUIDebugSelectedEntity.IsNull() ? Entity::Null : GetEntityByGUID(TextUIDebugSelectedEntity);
		if (!selectedEntity && !bDrawAllTextUIBounds)
			return;

		const glm::vec3 lineColor = glm::vec3(1.f, 0.78f, 0.f);
		const glm::vec3 dimLineColor = glm::vec3(0.5f, 0.39f, 0.f);

		// 3D text: debug lines in world space
		auto addText = [&](const TextComponent& text)
		{
			const Ref<const TextLayout> layoutRef = text.IsVisible() ? text.GetLayout() : nullptr;
			if (!layoutRef)
				return;

			const bool bSelected = text.Parent == selectedEntity;
			const TextLayout& layout = *layoutRef;
			const glm::vec2 origin = layout.GetAlignmentOrigin(text.GetAlignment());
			const glm::mat4 transform = Math::ToTransformMatrix(text.GetWorldTransform());
			auto addLine = [&](glm::vec2 start, glm::vec2 end, const glm::vec3& color)
			{
				auto& line = m_DebugLinesToDraw.emplace_back();
				line.Start.Location = glm::vec3(transform * glm::vec4(start - origin, 0.f, 1.f));
				line.End.Location = glm::vec3(transform * glm::vec4(end - origin, 0.f, 1.f));
				line.Start.Color = line.End.Color = color;
			};

			const glm::vec3& color = bSelected ? lineColor : dimLineColor;
			const glm::vec2 boxMin = layout.BoxMin;
			const glm::vec2 boxMax = layout.BoxMax;
			addLine(boxMin, glm::vec2(boxMax.x, boxMin.y), color);
			addLine(glm::vec2(boxMax.x, boxMin.y), boxMax, color);
			addLine(boxMax, glm::vec2(boxMin.x, boxMax.y), color);
			addLine(glm::vec2(boxMin.x, boxMax.y), boxMin, color);
			if (bSelected)
			{
				for (const auto& line : layout.Lines)
					addLine(glm::vec2(boxMin.x, line.BaselineY), glm::vec2(boxMax.x, line.BaselineY), dimLineColor);
			}
		};

		// 2D UI: drawn over the UI, in the same space as its vertices
		const glm::vec4 uiLineColor = glm::vec4(lineColor, 1.f);
		const glm::vec4 uiDimLineColor = glm::vec4(lineColor, 0.4f);
		const glm::vec4 uiAnchorColor = glm::vec4(0.31f, 0.78f, 1.f, 1.f);

		// `boxMin` & `boxMax` are relative to the pivot, in reference pixels (Y points down). `baselines` - Y of each text line, same space
		auto addUIElement = [&](glm::vec2 anchor, glm::vec2 offset, float rotationDegrees, glm::vec2 boxMin, glm::vec2 boxMax, bool bSelected, const std::vector<float>& baselines)
		{
			RendererDebugVertex2D vertex;
			vertex.Anchor = anchor;
			vertex.Offset = offset;
			vertex.Rotation = glm::radians(rotationDegrees);
			auto addLine = [&](glm::vec2 start, glm::vec2 end, const glm::vec4& color)
			{
				auto& line = m_DebugLines2DToDraw.emplace_back();
				line.Start = line.End = vertex;
				line.Start.Position = start;
				line.End.Position = end;
				line.Start.Color = line.End.Color = color;
			};

			const glm::vec4& color = bSelected ? uiLineColor : uiDimLineColor;
			addLine(boxMin, glm::vec2(boxMax.x, boxMin.y), color);
			addLine(glm::vec2(boxMax.x, boxMin.y), boxMax, color);
			addLine(boxMax, glm::vec2(boxMin.x, boxMax.y), color);
			addLine(glm::vec2(boxMin.x, boxMax.y), boxMin, color);
			if (!bSelected)
				return;

			for (float y : baselines)
				addLine(glm::vec2(boxMin.x, y), glm::vec2(boxMax.x, y), uiDimLineColor);

			// Pivot (cross)
			constexpr float crossSize = 6.f;
			addLine(glm::vec2(-crossSize, 0.f), glm::vec2(crossSize, 0.f), color);
			addLine(glm::vec2(0.f, -crossSize), glm::vec2(0.f, crossSize), color);

			// Anchor (diamond) and the offset from it to the pivot (line)
			vertex.Offset = glm::vec2(0.f);
			vertex.Rotation = 0.f;
			constexpr float anchorSize = 4.f;
			const glm::vec2 diamond[4] = { { -anchorSize, 0.f }, { 0.f, -anchorSize }, { anchorSize, 0.f }, { 0.f, anchorSize } };
			for (uint32_t i = 0; i < 4; ++i)
				addLine(diamond[i], diamond[(i + 1) % 4], uiAnchorColor);
			addLine(glm::vec2(0.f), glm::vec2(0.f), uiAnchorColor);
			m_DebugLines2DToDraw.back().End.Offset = offset;
		};

		auto addText2D = [&](const Text2DComponent& text)
		{
			const float fontSize = text.GetFontSize();
			const Ref<const TextLayout> layoutRef = (fontSize > 0.f && text.IsVisible()) ? text.GetLayout() : nullptr;
			if (!layoutRef)
				return;

			const bool bSelected = text.Parent == selectedEntity;
			const TextLayout& layout = *layoutRef;
			const glm::vec2 pivot = layout.GetBoxPoint(text.GetActualPivot());
			const glm::vec2 toPixels = glm::vec2(fontSize, -fontSize);
			auto toLocal = [&](glm::vec2 p) { return (p - pivot) * toPixels; };

			std::vector<float> baselines;
			if (bSelected)
			{
				baselines.reserve(layout.Lines.size());
				for (const auto& line : layout.Lines)
					baselines.push_back(toLocal(glm::vec2(0.f, line.BaselineY)).y);
			}
			addUIElement(text.GetAnchor(), text.GetOffset(), text.GetRotation(),
				toLocal(glm::vec2(layout.BoxMin.x, layout.BoxMax.y)), toLocal(glm::vec2(layout.BoxMax.x, layout.BoxMin.y)), bSelected, baselines);
		};

		auto addImage2D = [&](const Image2DComponent& image)
		{
			if (!image.IsVisible())
				return;

			const glm::vec2 size = image.GetSize();
			const glm::vec2 pivot = image.GetActualPivot() * size;
			addUIElement(image.GetAnchor(), image.GetOffset(), image.GetRotation(), -pivot, size - pivot, image.Parent == selectedEntity, {});
		};

		if (bDrawAllTextUIBounds)
		{
			auto texts = m_Registry.view<TextComponent>();
			for (auto entity : texts)
				addText(texts.get<TextComponent>(entity));
			auto images = m_Registry.view<Image2DComponent>();
			for (auto entity : images)
				addImage2D(images.get<Image2DComponent>(entity));
			auto texts2D = m_Registry.view<Text2DComponent>();
			for (auto entity : texts2D)
				addText2D(texts2D.get<Text2DComponent>(entity));
		}
		else
		{
			if (selectedEntity.HasComponent<TextComponent>())
				addText(selectedEntity.GetComponent<TextComponent>());
			if (selectedEntity.HasComponent<Image2DComponent>())
				addImage2D(selectedEntity.GetComponent<Image2DComponent>());
			if (selectedEntity.HasComponent<Text2DComponent>())
				addText2D(selectedEntity.GetComponent<Text2DComponent>());
		}
	}

	void Scene::DrawArrow(const glm::vec3& start, const glm::vec3& end, const glm::vec3& up)
	{
		const glm::vec3 dir = glm::normalize(end - start);

		RendererLine line;
		line.Start.Location = start;
		line.End.Location = end;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * 0.05f;
		m_UserDebugLines.push_back(line);

		line.Start.Location = start + dir * 0.15f + up * -0.05f;
		m_UserDebugLines.push_back(line);
	}

	void Scene::DrawCone(const glm::vec3& location, const glm::quat& rotation, float distance, float angleRad)
	{
		const glm::vec3 direction = Math::GetForwardVector(rotation);
		const glm::vec3 center = location + direction * distance;
		const glm::quat& quat = rotation;
		const float radius = distance * glm::tan(angleRad);

		for (uint32_t i = 0; i < Utils::s_SphereLinesCount; ++i)
		{
			const float angle1 = (float(i) / Utils::s_SphereLinesCount) * Utils::s_2PI;
			const float angle2 = (float(i + 1) / Utils::s_SphereLinesCount) * Utils::s_2PI;
			const float cosAngle1 = glm::cos(angle1);
			const float cosAngle2 = glm::cos(angle2);
			const float sinAngle1 = glm::sin(angle1);
			const float sinAngle2 = glm::sin(angle2);

			const glm::vec3 start = center + glm::rotate(quat, radius * glm::vec3(cosAngle1, sinAngle1, 0.f));
			auto& innerCircleLine = m_UserDebugLines.emplace_back();
			innerCircleLine.Start.Location = start;
			innerCircleLine.End.Location = center + glm::rotate(quat, radius * glm::vec3(cosAngle2, sinAngle2, 0.f));

			auto& toInnerLine = m_UserDebugLines.emplace_back();
			toInnerLine.Start.Location = location;
			toInnerLine.End.Location = start;
		}
	}

	void Scene::DrawSequencePath(const Ref<AssetSceneSequence>& asset, const Transform& base, float time, const GUID& highlightKeyID)
	{
		if (!asset)
			return;

		constexpr glm::vec3 pathColor = glm::vec3(1.f, 0.85f, 0.2f);
		constexpr glm::vec3 keyColor = glm::vec3(1.f, 1.f, 1.f);
		constexpr glm::vec3 highlightColor = glm::vec3(0.2f, 0.6f, 1.f);
		constexpr glm::vec3 cameraColor = glm::vec3(1.f, 0.3f, 0.3f);
		constexpr float keyMarkerSize = 0.15f;

		auto addLine = [this](const glm::vec3& a, const glm::vec3& b, const glm::vec3& color)
		{
			RendererLine line;
			line.Start.Location = a;
			line.Start.Color = color;
			line.End.Location = b;
			line.End.Color = color;
			DrawDebugLine(line);
		};

		const glm::quat baseRotation = base.Rotation.GetQuat();
		auto toWorld = [&](const glm::vec3& local)
		{
			return base.Location + (baseRotation * (local * base.Scale3D));
		};

		// Every camera's path is drawn, but only the live camera gets the bright path and the camera marker
		const SequenceCameraTrack* activeCamera = asset->ResolveActiveCamera(time);
		constexpr glm::vec3 inactivePathColor = glm::vec3(0.5f, 0.45f, 0.2f);

		std::vector<glm::vec3> points;
		for (const auto& cameraTrack : asset->GetCameraTracks())
		{
			if (!cameraTrack || !cameraTrack->IsEnabled())
				continue;

			EG_CORE_ASSERT(cameraTrack->GetType() == SequenceTrackType::Camera);

			const auto& locationChannel = cameraTrack->GetLocationChannel();
			if (locationChannel.IsEmpty())
				continue;

			// Path
			cameraTrack->GatherPathPoints(base, locationChannel.GetFirstKeyTime(), locationChannel.GetLastKeyTime(), 30u, points);
			const bool bActive = cameraTrack == activeCamera;
			for (size_t i = 1; i < points.size(); ++i)
				addLine(points[i - 1], points[i], bActive ? pathColor : inactivePathColor);

			// Key markers: a small 3D cross per location key
			for (const auto& key : locationChannel.GetKeys())
			{
				const glm::vec3 p = toWorld(key.Value);
				const glm::vec3 color = key.ID == highlightKeyID ? highlightColor : keyColor;
				const float size = key.ID == highlightKeyID ? keyMarkerSize * 2.f : keyMarkerSize;
				addLine(p - glm::vec3(size, 0.f, 0.f), p + glm::vec3(size, 0.f, 0.f), color);
				addLine(p - glm::vec3(0.f, size, 0.f), p + glm::vec3(0.f, size, 0.f), color);
				addLine(p - glm::vec3(0.f, 0.f, size), p + glm::vec3(0.f, 0.f, size), color);
			}

			if (!bActive)
				continue;

			// Camera at the playhead: position plus view direction
			SequenceEvalContext context;
			context.Base = base;
			context.Time = time;
			cameraTrack->Evaluate(context);
			if (context.Camera.bValid)
			{
				const auto& tr = context.Camera.WorldTransform;
				const glm::vec3 forward = Math::GetForwardVector(tr.Rotation);
				const glm::vec3 up = Math::GetUpVector(tr.Rotation);
				const glm::vec3 right = Math::GetRightVector(tr.Rotation);

				addLine(tr.Location, tr.Location + forward * 1.f, cameraColor);

				// Tiny "screen" rectangle in front of the camera so the roll is readable
				const glm::vec3 c = tr.Location + forward * 0.5f;
				const glm::vec3 r = right * 0.24f;
				const glm::vec3 u = up * 0.135f;
				addLine(c - r - u, c + r - u, cameraColor);
				addLine(c + r - u, c + r + u, cameraColor);
				addLine(c + r + u, c - r + u, cameraColor);
				addLine(c - r + u, c - r - u, cameraColor);
				addLine(c + u, c + u * 1.8f, cameraColor); // "up" tick
			}
		}
	}

	void Scene::DrawFrustum(const CameraComponent& camera)
	{
		const float aspect = float(m_ViewportWidth) / m_ViewportHeight;
		Utils::DrawFrustum(m_UserDebugLines, camera.Camera, camera.GetWorldTransform(), aspect);
	}
}
