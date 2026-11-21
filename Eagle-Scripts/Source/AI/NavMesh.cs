using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
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
        [UIName("Waiting for queue")] WaitingForQueue,
        [UIName("Waiting for path")] WaitingForPath,
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

    // The agent type's settings from the project settings. The name isn't included, so that getting it doesn't allocate (see `NavAgentType.GetName`)
    [StructLayout(LayoutKind.Sequential)]
    public struct NavAgentTypeInfo
    {
        public uint ID;
        public float Radius;    // The distance to erode/shrink the walkable area away from obstructions
        public float Height;    // Minimum floor to 'ceiling' height that will still allow the floor area to be considered walkable
        public float MaxClimb;  // Maximum ledge height that is considered to still be traversable
        public float MaxSlope;  // Degrees. The maximum slope that is considered walkable
        public Color3 DebugColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct NavMeshSettings
    {
        public AABB Bounds;
        public uint MaxQueryNodes;
        public uint ExpectedLayersPerTile;
        public uint MaxLayers;
        public uint MaxObstacles;
        public uint TileSize;
        public float CellSize;
        public float CellHeight;
        public float EdgeMaxError;
        public uint BorderSize;

        // Extents around a point that's searched for the nav mesh: the start/end of a path, a crowd agent's move target, etc.
        // Points farther from the nav mesh are treated as being off it.
        // Calculated automatically from the agent type (see `NavAgentTypeInfo`) unless `OverrideSearchExtents` is set.
        // Also, each query call can override it by providing `searchHalfExtents` input arg. If it's not provided, then this will be used.
        // Must be positive.
        public Vector3 SearchHalfExtents;
        public bool OverrideSearchExtents;

        public bool FilterLowHangingObstacles;
        public bool FilterLedgeSpans;
        public bool FilterWalkableLowHeightSpans;
    };

    // The nav area's settings from the project settings
    [StructLayout(LayoutKind.Sequential)]
    public struct NavAreaInfo
    {
        public uint ID;
        public float Cost; // Agent types can override it
        public Color3 DebugColor;
        public bool bBlocked; // Agents can't walk on it. Agent types and nav meshes at runtime can override it
    }

    // A nav area from the project settings which represents a kind of walkable surface (mud, road, etc...) that's more or less expensive to walk on.
    // The path search counts a meter in an area as its `Cost` in meters, so agents go around expensive areas unless the detour is longer.
    // Surfaces are marked by `NavigationAreaVolumeComponent` and by the nav area of colliders (`BaseColliderComponent.SetNavArea`)
    [StructLayout(LayoutKind.Sequential)]
    public struct NavArea : IEquatable<NavArea>
    {
        public readonly uint ID;

        internal const uint DefaultID = 0;
        internal const uint InvalidID = uint.MaxValue;

        public NavArea(uint id)
        {
            ID = id;
        }

        public static NavArea Default => new NavArea(DefaultID);
        public static NavArea Invalid => new NavArea(InvalidID);

        // Returns `Invalid` if the project doesn't have an area with this name
        public static NavArea FromName(string name)
        {
            return new NavArea(name == null ? InvalidID : GetIDByName_Native(name));
        }

        public bool IsDefault() { return ID == DefaultID; }

        // Returns true if the project has this area
        public bool IsValid()
        {
            return ID != InvalidID && IsValid_Native(ID);
        }

        // Returns null if the project doesn't have this area
        public string GetName()
        {
            return GetName_Native(ID);
        }

        // Returns false if the project doesn't have this area
        public bool GetInfo(out NavAreaInfo info)
        {
            info = new NavAreaInfo();
            return GetInfo_Native(ID, out info.ID, out info.Cost, out info.DebugColor, out info.bBlocked);
        }

        public bool Equals(NavArea other) => ID == other.ID;
        public override bool Equals(object obj) => obj is NavArea other && Equals(other);
        public override int GetHashCode() => ID.GetHashCode();
        public static bool operator ==(NavArea left, NavArea right) => left.ID == right.ID;
        public static bool operator !=(NavArea left, NavArea right) => left.ID != right.ID;

        public override string ToString()
        {
            if (ID == InvalidID)
                return "<Invalid>";

            string name = GetName();
            return name != null ? $"{name} (ID: {ID})" : $"<Unknown> (ID: {ID})";
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetIDByName_Native(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(uint id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetName_Native(uint id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetInfo_Native(uint id, out uint areaID, out float cost, out Color3 debugColor, out bool bBlocked);
    }

    // An agent type from the project settings. Each nav mesh is built for one agent type, and a scene has at most one nav mesh per agent type.
    // `Auto` isn't really an agent type. It asks for the type to be picked automatically where it makes sense.
    // For example, a crowd agent's type (`Auto` picks a nav mesh by its size), and the `fallbackAgentType` of `NavMesh.GetNavMeshFor` and `Navigation.MoveToTarget` (the default agent type).
    // Elsewhere, it's treated like a type that the project doesn't have.
    [StructLayout(LayoutKind.Sequential)]
    public struct NavAgentType : IEquatable<NavAgentType>
    {
        public readonly uint ID;

        internal static uint DefaultID => NavAgentType.GetDefaultID_Native();
        internal const uint AutoID = 0;
        internal const uint InvalidID = uint.MaxValue;

        public NavAgentType(uint id)
        {
            ID = id;
        }

        public static NavAgentType Auto => new NavAgentType(AutoID);
        public static NavAgentType Invalid => new NavAgentType(InvalidID);

        // The project's default agent type
        public static NavAgentType GetDefault()
        {
            return new NavAgentType(GetDefaultID_Native());
        }

        // Returns `Invalid` if the project doesn't have an agent type with this name
        public static NavAgentType FromName(string name)
        {
            return new NavAgentType(name == null ? InvalidID : GetIDByName_Native(name));
        }

        // Picks the best nav mesh based on the agent settings: the smallest nav mesh of the scene whose agent type fits the agent's radius and height; the largest one if none fits.
        // If the scene has no built nav mesh, returns a NavMesh for the default agent type. Its queries fail until it's built.
        public static NavMesh GetNavMeshAuto(AgentSettings settings)
        {
            return new NavMesh(ResolveAuto_Native(settings.AgentRadius, settings.AgentHeight));
        }

        public bool IsAuto() { return ID == AutoID; }

        // Returns true if the project has this agent type. `Auto` isn't one of them, so it's false for it
        public bool IsValid()
        {
            return ID != InvalidID && ID != AutoID && IsValid_Native(ID);
        }

        // Returns null if the project doesn't have this agent type
        public string GetName()
        {
            return GetName_Native(ID);
        }

        // Returns false if the project doesn't have this agent type
        public bool GetInfo(out NavAgentTypeInfo info)
        {
            info = new NavAgentTypeInfo();
            return GetInfo_Native(ID, out info.ID, out info.DebugColor, out info.Radius, out info.Height, out info.MaxClimb, out info.MaxSlope);
        }

        // This agent type's override, or if it's not set, then the area's cost.
        // Unknown areas cost same as Default
        public float GetAreaCost(NavArea area)
        {
            return GetAreaCost_Native(ID, area.ID);
        }

        // Whether agents of this type can't walk on an area.
        // Runtime blocking of nav meshes isn't included (see `NavMesh.IsAreaBlocked`). Unknown areas are the same as Default
        public bool IsAreaBlocked(NavArea area)
        {
            return IsAreaBlocked_Native(ID, area.ID);
        }

        public bool Equals(NavAgentType other) => ID == other.ID;
        public override bool Equals(object obj) => obj is NavAgentType other && Equals(other);
        public override int GetHashCode() => ID.GetHashCode();
        public static bool operator ==(NavAgentType left, NavAgentType right) => left.ID == right.ID;
        public static bool operator !=(NavAgentType left, NavAgentType right) => left.ID != right.ID;

        public override string ToString()
        {
            if (ID == AutoID)
                return "<Auto>";
            if (ID == InvalidID)
                return "<Invalid>";

            string name = GetName();
            return name != null ? $"{name} (ID: {ID})" : $"<Unknown> (ID: {ID})";
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetDefaultID_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetIDByName_Native(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValid_Native(uint id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string GetName_Native(uint id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetInfo_Native(uint id, out uint agentTypeID, out Color3 debugColor, out float radius, out float height, out float maxClimb, out float maxSlope);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint ResolveAuto_Native(float agentRadius, float agentHeight);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetAreaCost_Native(uint agentTypeID, uint areaID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsAreaBlocked_Native(uint agentTypeID, uint areaID);
    }

    // The scene's nav mesh of an agent type (see `GetNavMeshByName`, `GetNavMeshByID`, `GetNavMesh`, `GetNavMeshFor`).
    // It refers to the nav mesh by its agent type, so it stays usable when the nav mesh is rebuilt.
    // Queries fail (and log an error) while the scene doesn't have a built nav mesh of the type, see `IsValid`
    public sealed class NavMesh
    {
        private readonly uint m_AgentTypeID;

        internal NavMesh(uint agentTypeID)
        {
            m_AgentTypeID = agentTypeID;
        }

        public NavAgentType GetAgentType() { return new NavAgentType(m_AgentTypeID); }

        // Whether agents can't walk on an area.
        // It doesn't need the nav mesh to be built. Unknown areas are the same as Default
        public bool IsAreaBlocked(NavArea area) { return IsAreaBlocked_Native(m_AgentTypeID, area.ID); }

        // Blocks or unblocks an area for every agent on this nav mesh.
        // Agents plan their paths again. An agent standing in an area when it's blocked is moved to the nearest point it can walk on, if there's one nearby.
        public void SetAreaBlocked(NavArea area, bool bBlocked) { SetAreaBlocked_Native(m_AgentTypeID, area.ID, bBlocked); }
        public void ResetAreaBlocked(NavArea area) { ResetAreaBlocked_Native(m_AgentTypeID, area.ID); } // Back to the project settings
        public void ResetAreasBlocked() { ResetAreasBlocked_Native(m_AgentTypeID); }

        // The area of the nav mesh at the nearest point within `searchHalfExtents` of `point`, blocked areas included.
        // Returns false if there's no nav mesh there
        public bool GetAreaAt(Vector3 point, out NavArea area, Vector3? searchHalfExtents = null)
        {
            bool bCustomExtents = Navigation.ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            bool bFound = GetAreaAt_Native(m_AgentTypeID, ref point, out uint areaID, bCustomExtents, ref extents);
            area = new NavArea(areaID);
            return bFound;
        }

        // True if the scene has a built nav mesh of this agent type
        public bool IsValid() { return HasNavMesh_Native(m_AgentTypeID); }
        public bool IsValidPoint(Vector3 point) { return Navigation.IsValidPoint_Native(m_AgentTypeID, ref point, out _); }
        public bool IsValidPoint(Vector3 point, out Vector3 navMeshPoint) { return Navigation.IsValidPoint_Native(m_AgentTypeID, ref point, out navMeshPoint); }
        public bool HasCompletePath(Vector3 from, Vector3 to, Vector3? searchHalfExtents = null) { return Navigation.HasCompletePath(m_AgentTypeID, from, to, searchHalfExtents); }
        public bool CanReach(Vector3 from, Vector3 to, Vector3? searchHalfExtents = null) { return CanReach(from, to, out _, searchHalfExtents); }
        public bool CanReach(Vector3 from, Vector3 to, out Vector3 standPoint, Vector3? searchHalfExtents = null) { return IsValidPoint(to, out standPoint) && HasCompletePath(from, standPoint, searchHalfExtents); }

        // Returns false if the nav mesh isn't built
        public bool GetSettings(out NavMeshSettings settings) { return Navigation.GetSettings_Native(m_AgentTypeID, out settings); }
        public bool GetSearchHalfExtents(out Vector3 searchHalfExtents) { return Navigation.GetSearchHalfExtents_Native(m_AgentTypeID, out searchHalfExtents); }
        
        public bool FindNearestPoint(Vector3 point, out Vector3 nearestPoint, Vector3? searchHalfExtents = null) { return Navigation.FindNearestPoint(m_AgentTypeID, point, out nearestPoint, searchHalfExtents); }
        public bool FindDistanceToWall(Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance, Vector3? searchHalfExtents = null) { return Navigation.FindDistanceToWall(m_AgentTypeID, pos, maxRadius, out outHitPos, out outHitNormal, out outHitDistance, searchHalfExtents); }
        public bool FindRandomPoint(out Vector3 outRandomPoint) { return Navigation.FindRandomPoint_Native(m_AgentTypeID, out outRandomPoint); }
        public bool FindRandomPointInCircle(Vector3 pos, float radius, out Vector3 outRandomPoint, Vector3? searchHalfExtents = null) { return Navigation.FindRandomPointInCircle(m_AgentTypeID, pos, radius, out outRandomPoint, searchHalfExtents); }
        public Vector3[] FindStraightPath(Vector3 start, Vector3 end, uint maxPolys = 256, Vector3? searchHalfExtents = null) { return Navigation.FindStraightPath(m_AgentTypeID, start, end, maxPolys, searchHalfExtents); }
        public Vector3[] FindSmoothPath(Vector3 start, Vector3 end, uint maxPolys = 256, uint maxSmooth = 2048, Vector3? searchHalfExtents = null) { return Navigation.FindSmoothPath(m_AgentTypeID, start, end, maxPolys, maxSmooth, searchHalfExtents); }

        // Sets the move target of all agents of this nav mesh's crowd
        public bool SetCrowdMoveTarget(Vector3 target, Vector3? searchHalfExtents = null)
        {
            bool bCustomExtents = Navigation.ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return SetCrowdMoveTarget_Native(m_AgentTypeID, ref target, bCustomExtents, ref extents);
        }

        // Resets the move target of all agents of this nav mesh's crowd
        public void ResetCrowdMoveTarget()
        {
            ResetCrowdMoveTarget_Native(m_AgentTypeID);
        }

        public override string ToString()
        {
            return $"NavMesh({ GetAgentType() })";
        }

        // Returns null if the project doesn't have an agent type with this ID
        public static NavMesh GetNavMeshByID(uint agentTypeID)
        {
            return GetNavMesh(new NavAgentType(agentTypeID));
        }

        // Returns null if the project doesn't have an agent type with this name
        public static NavMesh GetNavMeshByName(string agentTypeName)
        {
            return GetNavMesh(NavAgentType.FromName(agentTypeName));
        }

        // Returns null if the project doesn't have the agent type.
        // To resolve `Auto`, use `GetNavMeshFor(entity, fallbackAgentType)` or `GetDefaultNavMesh()`
        public static NavMesh GetNavMesh(NavAgentType agentType)
        {
            return agentType.IsValid() ? new NavMesh(agentType.ID) : null;
        }

        // The nav mesh of the project's default agent type
        public static NavMesh GetDefaultNavMesh()
        {
            return new NavMesh(NavAgentType.GetDefaultID_Native());
        }

        // The nav mesh that `entity` navigates on (the same one `Navigation.MoveToTarget` moves it on):
        //     1. If the entity has a `NavigationCrowdAgentComponent`, the nav mesh its agent is on or would be on.
        //        It doesn't fall back, even if the scene doesn't have that nav mesh;
        //     2. Otherwise, `fallbackAgentType` if it's not `Auto`. If the project doesn't have it, the queries of the returned nav mesh fail;
        //     3. Otherwise, the nav mesh of the default agent type.
        //
        // Never returns null, but the scene might not have the nav mesh built in which case it returns NavMesh for the default agent type and its queries fail (see `NavMesh.IsValid`).
        // For a crowd agent with `NavAgentType.Auto`, the nav mesh can change when nav meshes are built or its settings change, so get it again instead of keeping it
        public static NavMesh GetNavMeshFor(Entity entity, NavAgentType fallbackAgentType = default)
        {
            // A crowd agent always moves on its own nav mesh
            if (entity != null && entity.HasComponent<NavigationCrowdAgentComponent>())
            {
                NavigationCrowdAgentComponent agent = entity.GetComponent<NavigationCrowdAgentComponent>();
                NavAgentType agentType = agent.GetAgentType();
                if (!agentType.IsAuto())
                    return new NavMesh(agentType.ID);

                // The nav mesh it's on...
                NavMesh navMesh = agent.GetNavMesh();
                if (navMesh != null)
                    return navMesh;

                // ...or the one it would be on
                return NavAgentType.GetNavMeshAuto(agent.GetSettings());
            }

            // As is, even if the project doesn't have it, so that queries with it fail instead of using another nav mesh
            return fallbackAgentType.IsAuto() ? GetDefaultNavMesh() : new NavMesh(fallbackAgentType.ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasNavMesh_Native(uint agentTypeID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool SetCrowdMoveTarget_Native(uint agentTypeID, ref Vector3 target, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetCrowdMoveTarget_Native(uint agentTypeID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetAreaAt_Native(uint agentTypeID, ref Vector3 point, out uint areaID, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsAreaBlocked_Native(uint agentTypeID, uint areaID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetAreaBlocked_Native(uint agentTypeID, uint areaID, bool bBlocked);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetAreaBlocked_Native(uint agentTypeID, uint areaID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetAreasBlocked_Native(uint agentTypeID);
    }
}
