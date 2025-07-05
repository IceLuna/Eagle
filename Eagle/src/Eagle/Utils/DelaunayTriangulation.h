#pragma once

#include <glm/glm.hpp>

namespace Eagle::Delaunay
{
	struct Vertex
	{
		Vertex() = default;
		Vertex(double x, double y) : Coord(x, y) {}

		bool operator== (const Vertex& other) const
		{
			return Coord == other.Coord;
		}

        glm::dvec2 Coord = glm::dvec2(0.0);
        const void* UserData = nullptr; // Can be used to associate some data with the point
	};

	struct Triangle
	{
		Triangle() = default;

		Triangle(const Vertex& a, const Vertex& b, const Vertex& c)
		{
			V[0] = a;
			V[1] = b;
			V[2] = c;
			CalculateCircumcircle();
		}

		void CalculateCircumcircle();

		bool InCircumcircle(const Vertex& v) const
		{
			const glm::dvec2 dxy = Center.Coord - v.Coord;
			const double distSquared = glm::dot(dxy, dxy);
			return distSquared <= RadiusSquared;
		}

		bool InTriangle(const Vertex& v) const;
		bool InTriangle(const glm::dvec3& buv) const;
		glm::dvec3 CalculateBarycentric(const Vertex& v) const;

		Vertex V[3];
		Vertex Center;
		double Radius = 0.0;
		double RadiusSquared = 0.0;
	};

	std::vector<Triangle> Triangulate(const std::vector<Vertex>& vertices);
}
