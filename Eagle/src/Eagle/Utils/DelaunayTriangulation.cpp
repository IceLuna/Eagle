#include "egpch.h"
#include "DelaunayTriangulation.h"

namespace Eagle::Delaunay
{
    struct Edge
    {
        Edge(const Vertex& a, const Vertex& b)
            : V0(a), V1(b)
        {}

        bool operator==(const Edge& other) const
        {
            return V0 == other.V0 && V1 == other.V1;
        }

        Edge Inverse() const
        {
            return Edge{ V1, V0 };
        }

        Vertex V0, V1;
    };

    // Create a triangle that bounds the given vertices, with room to spare
    static Triangle CreateBoundingTriangle(const std::vector<Vertex>& vertices)
    {
        // NOTE: There's a bit of a heuristic here. If the bounding triangle
        // is too large and you see overflow/underflow errors. If it is too small
        // you end up with a non-convex hull.

        double minx = std::numeric_limits<float>::max();
        double miny = std::numeric_limits<float>::max();
        double maxx = std::numeric_limits<float>::lowest();
        double maxy = std::numeric_limits<float>::lowest();

        for (const auto& vertex : vertices)
        {
            if (vertex.Coord.x < minx)
                minx = vertex.Coord.x;
            if (vertex.Coord.y < miny)
                miny = vertex.Coord.y;
            if (vertex.Coord.x > maxx)
                maxx = vertex.Coord.x;
            if (vertex.Coord.y > maxy)
                maxy = vertex.Coord.y;
        }

        // Add some room to spare
        const double extraCoef = 10.0;
        const double dx = (maxx - minx) * extraCoef;
        const double dy = (maxy - miny) * extraCoef;

        Vertex stv0 = { minx - dx, miny - dy * 3 };
        Vertex stv1 = { minx - dx, maxy + dy };
        Vertex stv2 = { maxx + dx * 3, maxy + dy };

        return { stv0, stv1, stv2 };
    }

    // Remove duplicate edges from an array
    static std::vector<Edge> UniqueEdges(const std::vector<Edge>& edges)
    {
        // TODO: This is O(n^2), make it O(n) with a hash or some such
        std::vector<Edge> uniqueEdges;
        uniqueEdges.reserve(edges.size());

        for (size_t i = 0; i < edges.size(); ++i)
        {
            const auto& edge1 = edges[i];
            bool bUnique = true;

            for (size_t j = 0; j < edges.size(); ++j)
            {
                if (i == j)
                    continue;

                const auto& edge2 = edges[j];
                if (edge1 == edge2 || (edge1.Inverse() == edge2))
                {
                    bUnique = false;
                    break;
                }
            }

            if (bUnique)
                uniqueEdges.push_back(edge1);
        }

        return uniqueEdges;
    }

    // Update triangulation with a vertex
    static void AddVertex(std::vector<Triangle>& triangles, const Vertex& vertex)
    {
        std::vector<Edge> edges;
        edges.reserve(triangles.size() * 3);

        // Remove triangles with circumcircles containing the vertex
        triangles.erase(
            std::remove_if(triangles.begin(), triangles.end(), [&edges, &vertex](const Triangle& triangle)
            {
                if (triangle.InCircumcircle(vertex))
                {
                    edges.push_back(Edge{ triangle.V[0], triangle.V[1] });
                    edges.push_back(Edge{ triangle.V[1], triangle.V[2] });
                    edges.push_back(Edge{ triangle.V[2], triangle.V[0] });
                    return true;
                }

                return false;
            }),
            triangles.end()
        );

        edges = UniqueEdges(edges);

        // Create new triangles from the unique edges and new vertex
        for (const auto& edge : edges)
        {
            triangles.push_back({ edge.V0, edge.V1, vertex });
        };
    }

    void Triangle::CalculateCircumcircle()
    {
        // From: http://www.exaflop.org/docs/cgafaq/cga1.html
        const double A = V[1].Coord.x - V[0].Coord.x;
        const double B = V[1].Coord.y - V[0].Coord.y;
        const double C = V[2].Coord.x - V[0].Coord.x;
        const double D = V[2].Coord.y - V[0].Coord.y;

        const double E = A * (V[0].Coord.x + V[1].Coord.x) + B * (V[0].Coord.y + V[1].Coord.y);
        const double F = C * (V[0].Coord.x + V[2].Coord.x) + D * (V[0].Coord.y + V[2].Coord.y);

        const double G = 2.0 * (A * (V[2].Coord.y - V[1].Coord.y) - B * (V[2].Coord.x - V[1].Coord.x));

        double dx, dy;

        if (glm::abs(G) < 1e-6)
        {
            // Collinear - find extremes and use the midpoint
            const double minx = glm::min(V[0].Coord.x, glm::min(V[1].Coord.x, V[2].Coord.x));
            const double miny = glm::min(V[0].Coord.y, glm::min(V[1].Coord.y, V[2].Coord.y));
            const double maxx = glm::max(V[0].Coord.x, glm::max(V[1].Coord.x, V[2].Coord.x));
            const double maxy = glm::max(V[0].Coord.y, glm::max(V[1].Coord.y, V[2].Coord.y));

            Center.Coord.x = (minx + maxx) * 0.5;
            Center.Coord.y = (miny + maxy) * 0.5;

            dx = Center.Coord.x - minx;
            dy = Center.Coord.y - miny;
        }
        else
        {
            const double cx = (D * E - B * F) / G;
            const double cy = (A * F - C * E) / G;

            Center = Vertex{ cx, cy };

            dx = Center.Coord.x - V[0].Coord.x;
            dy = Center.Coord.y - V[0].Coord.y;
        }

        RadiusSquared = dx * dx + dy * dy;
        Radius = glm::sqrt(RadiusSquared);
    }

    bool Triangle::InTriangle(const Vertex& v) const
    {
        return InTriangle(CalculateBarycentric(v));
    }

    bool Triangle::InTriangle(const glm::dvec3& buv) const
    {
        return 0 <= buv.x && buv.x <= 1 && 0 <= buv.y && buv.y <= 1 && 0 <= buv.z && buv.z <= 1;
    }

    glm::dvec3 Triangle::CalculateBarycentric(const Vertex& v) const
    {
        const double dyV1V2 = V[1].Coord.y - V[2].Coord.y;
        const double dxV0V2 = V[0].Coord.x - V[2].Coord.x;
        const double dxV2V1 = V[2].Coord.x - V[1].Coord.x;
        const double dyV0V2 = V[0].Coord.y - V[2].Coord.y;
        const double dyV2V0 = -dyV0V2;
        const double dxVV2 = v.Coord.x - V[2].Coord.x;
        const double dyVV2 = v.Coord.y - V[2].Coord.y;

        double denominator = (dyV1V2 * dxV0V2 + dxV2V1 * dyV0V2);
        
        glm::dvec3 buv;
        buv.x = (dyV1V2 * dxVV2 + dxV2V1 * dyVV2) / denominator;
        buv.y = (dyV2V0 * dxVV2 + dxV0V2 * dyVV2) / denominator;
        buv.z = 1.0 - buv.x - buv.y;

        return buv;
    }

    std::vector<Triangle> Triangulate(const std::vector<Vertex>& vertices)
    {
        std::vector<Triangle> triangles;
        triangles.reserve(20);

        // First, create a "supertriangle" that bounds all vertices
        Triangle st = CreateBoundingTriangle(vertices);
        triangles.push_back(st);

        // Next, begin the triangulation one vertex at a time
        for (const auto& vertex : vertices)
        {
            // NOTE: This is O(n^2) - can be optimized by sorting vertices
            // along the x-axis and only considering triangles that have
            // potentially overlapping circumcircles
            AddVertex(triangles, vertex);
        };

        // Remove triangles that shared edges with "supertriangle"
        triangles.erase(
            std::remove_if(triangles.begin(), triangles.end(), [&st](const Triangle& triangle)
            {
                return (triangle.V[0] == st.V[0] || triangle.V[0] == st.V[1] || triangle.V[0] == st.V[2] ||
                        triangle.V[1] == st.V[0] || triangle.V[1] == st.V[1] || triangle.V[1] == st.V[2] ||
                        triangle.V[2] == st.V[0] || triangle.V[2] == st.V[1] || triangle.V[2] == st.V[2]);
            }),
            triangles.end()
        );

        return triangles;
    }
}
