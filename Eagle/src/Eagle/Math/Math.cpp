#include "egpch.h"
#include "Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Eagle::Math
{
	bool DecomposeTransformMatrix(const glm::mat4& transformMatrix, glm::vec3& outLocation, glm::vec3& outRotation, glm::vec3& outScale)
	{
		// From glm::decompose in matrix_decompose.inl

		using namespace glm;
		using T = float;

		mat4 LocalMatrix(transformMatrix);

		// Normalize the matrix.
		if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>()))
			return false;

		// First, isolate perspective.  This is the messiest.
		if (
			epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
			epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>()))
		{
			// Clear the perspective partition
			LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
			LocalMatrix[3][3] = static_cast<T>(1);
		}

		// Next take care of translation (easy).
		outLocation = vec3(LocalMatrix[3]);
		LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

		vec3 Row[3] = { glm::vec3(0.f) };

		// Now get scale and shear.
		for (length_t i = 0; i < 3; ++i)
			for (length_t j = 0; j < 3; ++j)
				Row[i][j] = LocalMatrix[i][j];

		// Compute X scale factor and normalize first row.
		outScale.x = length(Row[0]);
		Row[0] = detail::scale(Row[0], static_cast<T>(1));
		outScale.y = length(Row[1]);
		Row[1] = detail::scale(Row[1], static_cast<T>(1));
		outScale.z = length(Row[2]);
		Row[2] = detail::scale(Row[2], static_cast<T>(1));

		// At this point, the matrix (in rows[]) is orthonormal.
		// Check for a coordinate system flip.  If the determinant
		// is -1, then negate the matrix and the scaling factors.
#if 0
		Pdum3 = cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
		if (dot(Row[0], Pdum3) < 0)
		{
			for (length_t i = 0; i < 3; i++)
			{
				scale[i] *= static_cast<T>(-1);
				Row[i] *= static_cast<T>(-1);
			}
		}
#endif

		outRotation.y = asin(-Row[0][2]);
		if (cos(outRotation.y) != 0) {
			outRotation.x = atan2(Row[1][2], Row[2][2]);
			outRotation.z = atan2(Row[0][1], Row[0][0]);
		}
		else {
			outRotation.x = atan2(-Row[2][0], Row[1][1]);
			outRotation.z = 0;
		}


		return true;
	}

	Transform DecomposeTransformMatrix(const glm::mat4& transformMatrix)
	{
		static glm::vec3 notUsed1;
		static glm::vec4 notUsed2;
		Transform result;
		[[maybe_unused]] const bool bSuccess = glm::decompose(transformMatrix, result.Scale3D, result.Rotation.GetQuat(), result.Location, notUsed1, notUsed2);
		//EG_CORE_ASSERT(bSuccess);
		return result;
	}

	glm::mat4 ToTransformMatrix(const Transform& transform)
	{
		glm::mat4 result = glm::translate(glm::mat4(1), transform.Location);
		result *= transform.Rotation.ToMat4();
		result = glm::scale(result, transform.Scale3D);
		return result;
	}

	static glm::vec3 GetSafeNormal2D(glm::vec3 v, float tolerance = 0.001f, const glm::vec3& resultIfZero = glm::vec3(0))
	{
		// Ignore up direction (Y)
		const float squareSum = v.x * v.x + v.z * v.z;

		if (squareSum == 1.0)
		{
			return glm::vec3(v.x, 0.f, v.z);
		}
		else if (squareSum < tolerance)
		{
			return resultIfZero;
		}

		const float scale = glm::inversesqrt(squareSum);
		return glm::vec3(v.x * scale, 0.f, v.z * scale);
	}

	float CalculateDirection(const glm::vec3& velocity, const Rotator& rotation)
	{
		if (IsNearlyZero(velocity))
			return 0.f;

		const glm::vec3 forwardVector = GetForwardVector(rotation);
		const glm::vec3 rightVector = GetRightVector(rotation);
		const glm::vec3 normVelocity = GetSafeNormal2D(velocity);

		// Clamp is required since it can return smth like `1.00001` in which case `acos` fails
		const float forwardCosAngle = glm::clamp(glm::dot(forwardVector, normVelocity), -1.f, 1.f);
		float forwardDeltaDegree = glm::degrees(glm::acos(forwardCosAngle));

		// depending on where right vector is, flip it
		const float rightCosAngle = glm::dot(rightVector, normVelocity);
		if (rightCosAngle < 0.f)
		{
			forwardDeltaDegree *= -1.f;
		}

		return forwardDeltaDegree;
	}
}