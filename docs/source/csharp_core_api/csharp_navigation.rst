.. _csharp_navigation_guide:

C# Navigation
=============

Provides an API for Navigation Meshes and Crowds.

.. code-block:: csharp
	
    public enum AgentObstacleAvoidanceQuality
    {
        Low, Medium, Good, High
    };

    public enum AgentTargetState
    {
        None = 0,
        Failed,
        Valid,
        Requesting,
        WaitingForQueue,
        WaitingForPath,
        Velocity,
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct AgentSettings
    {
        public float AgentRadius;
        public float AgentHeight;
        public float MaxAcceleration;
        public float MaxSpeed;
        public float SeparationWeight;
        public AgentObstacleAvoidanceQuality ObstacleAvoidanceQuality;
        public bool bAnticipateTurns;
        public bool bOptimizeVis;
        public bool bOptimizeTopo;
        public bool bSeparation;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct CrowdSettings
    {
        public uint MaxAgents;
        public float MaxAgentRadius;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct NavMeshSettings
    {
        public AABB AABB;
        public uint MaxQueryNodes;
        public uint ExpectedLayersPerTile;
        public uint MaxLayers;
        public uint MaxObstacles;
        public uint TileSize;
        public float CellSize;
        public float CellHeight;
        public float MaxSlope;
        public float AgentHeight;
        public float AgentMaxClimb;
        public float AgentRadius;
        public float EdgeMaxLen;
        public float EdgeMaxError;
        public float RegionMinSize;
        public float RegionMergeSize;
        public uint VertsPerPoly;
        public uint BorderSize;
        public bool FilterLowHangingObstacles;
        public bool FilterLedgeSpans;
        public bool FilterWalkableLowHeightSpans;
    };

    public class Navigation
    {
        // These return true on success
        public static bool FindDistanceToWall(Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance);
        public static bool FindRandomPoint(out Vector3 outRandomPoint);
        public static bool FindRandomPointInCircle(Vector3 pos, float radius, out Vector3 outRandomPoint);

        public static bool IsValidPoint(Vector3 pos);

        public static Vector3[] FindStraightPath(Vector3 start, Vector3 end, uint maxPolys = 256);
        public static Vector3[] FindSmoothPath(Vector3 start, Vector3 end, uint maxPolys = 256, uint maxSmooth = 2048);
    }

    public class CrowdNavigation
    {
        // Makes all crowd agents move towards the target
        public static void SetMoveTarget(Vector3 pos);
        public static void ResetMoveTarget();
    }

