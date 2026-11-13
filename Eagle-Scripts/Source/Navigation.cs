using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    public enum MoveToTargetResult
    {
        InProgress,
        Arrived,
        NoPath,
    }

    // State for `Navigation.MoveToTarget`. Used by crowd agents to keep their move request.
    // Requesting a new target every frame throws away Detour's path search, and the agent never arrives.
    public class MoveToTargetState
    {
        public void Reset()
        {
            bHasRequest = false;
            Result = MoveToTargetResult.InProgress;
        }

        public MoveToTargetResult Result { get; internal set; } = MoveToTargetResult.InProgress;

        internal bool bHasRequest = false;
        internal Vector3 RequestedTarget = new Vector3(0f);
    }

    // Navigation queries. The static queries use the nav mesh of the project's default agent type.
    // So, it's recommended to use `NavMesh` class instead. But if there's just one NavMesh in the project, this can be used as well.
    // Other nav meshes are available via static functions inside `NavMesh` class (for example, `NavMesh.GetNavMesh`)
    public class Navigation
    {
        // Finds the nearest point on the nav mesh within `searchHalfExtents` of `point`.
        // Returns false if there's none
        public static bool FindNearestPoint(Vector3 point, out Vector3 nearestPoint, Vector3? searchHalfExtents = null)
        {
            return FindNearestPoint(NavAgentType.DefaultID, point, out nearestPoint, searchHalfExtents);
        }

        // Returns true if an agent standing on the nav mesh can occupy/reach `point`.
        // That means within the agent's horizontal reach, at most the agent type's `Height` above the nav mesh and at most its `Max Climb` below it
        // It doesn't check whether an agent can get there from somewhere else (see `CanReach`).
        public static bool IsValidPoint(Vector3 point)
        {
            return IsValidPoint_Native(NavAgentType.DefaultID, ref point, out _);
        }

        // Also returns the nav mesh point an agent would stand on to occupy `point`
        public static bool IsValidPoint(Vector3 point, out Vector3 navMeshPoint)
        {
            return IsValidPoint_Native(NavAgentType.DefaultID, ref point, out navMeshPoint);
        }

        // Returns true if an agent can walk from `from` to `to` along the nav mesh.
        // Very long paths might be reported as unreachable if the search runs out of the nav mesh's Max Query Nodes.
        // This function first snaps `to` to the nearest polygon inside the search box, then checks for a full path to that polygon.
        // The snapped point can be a long way from the target in ways an agent can't reach across.
        // So, to check that an agent can get to a point and touch it, use `CanReach`
        public static bool HasCompletePath(Vector3 from, Vector3 to, Vector3? searchHalfExtents = null)
        {
            return HasCompletePath(NavAgentType.DefaultID, from, to, searchHalfExtents);
        }

        // Returns true if an agent at `from` can walk to a spot from which it can occupy/reach `to`.
        // `searchHalfExtents` is used to snap `from`
        public static bool CanReach(Vector3 from, Vector3 to, Vector3? searchHalfExtents = null)
        {
            return CanReach(from, to, out _, searchHalfExtents);
        }

        // Also returns that spot at the nav mesh where the agent would stand on
        public static bool CanReach(Vector3 from, Vector3 to, out Vector3 standPoint, Vector3? searchHalfExtents = null)
        {
            // The path must go to the exact spot `IsValidPoint` picked. Snapping `to` again could pick another surface
            return IsValidPoint(to, out standPoint) && HasCompletePath(from, standPoint, searchHalfExtents);
        }

        // Gets the nav mesh's search extents that are actually used (what queries use when `searchHalfExtents` is null).
        // Returns false if there's no nav mesh
        public static bool GetSearchHalfExtents(out Vector3 searchHalfExtents)
        {
            return GetSearchHalfExtents_Native(NavAgentType.DefaultID, out searchHalfExtents);
        }

        // Gets the settings of the scene's nav mesh of the default agent type.
        // Returns false if there's no nav mesh
        public static bool GetSettings(out NavMeshSettings settings)
        {
            return GetSettings_Native(NavAgentType.DefaultID, out settings);
        }

        // Finds the distance from `pos` (snapped to the nav mesh) to the nearest nav mesh edge within `maxRadius`.
        // Returns true if success
        public static bool FindDistanceToWall(Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance, Vector3? searchHalfExtents = null)
        {
            return FindDistanceToWall(NavAgentType.DefaultID, pos, maxRadius, out outHitPos, out outHitNormal, out outHitDistance, searchHalfExtents);
        }

        // Returns true if success
        public static bool FindRandomPoint(out Vector3 outRandomPoint)
        {
            return FindRandomPoint_Native(NavAgentType.DefaultID, out outRandomPoint);
        }

        // Finds a random nav mesh point that can be reached from `pos` (snapped to the nav mesh), within `radius` of it.
        // Returns true if success
        public static bool FindRandomPointInCircle(Vector3 pos, float radius, out Vector3 outRandomPoint, Vector3? searchHalfExtents = null)
        {
            return FindRandomPointInCircle(NavAgentType.DefaultID, pos, radius, out outRandomPoint, searchHalfExtents);
        }

        // Returns an empty array if there's no path.
        // If `end` can't be reached, the path goes as close to it as possible
        public static Vector3[] FindStraightPath(Vector3 start, Vector3 end, uint maxPolys = 256, Vector3? searchHalfExtents = null)
        {
            return FindStraightPath(NavAgentType.DefaultID, start, end, maxPolys, searchHalfExtents);
        }

        // Returns an empty array if there's no path.
        // If `end` can't be reached, the path goes as close to it as possible
        public static Vector3[] FindSmoothPath(Vector3 start, Vector3 end, uint maxPolys = 256, uint maxSmooth = 2048, Vector3? searchHalfExtents = null)
        {
            return FindSmoothPath(NavAgentType.DefaultID, start, end, maxPolys, maxSmooth, searchHalfExtents);
        }

        internal static bool FindNearestPoint(uint agentTypeID, Vector3 point, out Vector3 nearestPoint, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return FindNearestPoint_Native(agentTypeID, ref point, out nearestPoint, bCustomExtents, ref extents);
        }

        internal static bool HasCompletePath(uint agentTypeID, Vector3 from, Vector3 to, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return HasCompletePath_Native(agentTypeID, ref from, ref to, bCustomExtents, ref extents);
        }

        internal static bool FindDistanceToWall(uint agentTypeID, Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return FindDistanceToWall_Native(agentTypeID, ref pos, maxRadius, out outHitPos, out outHitNormal, out outHitDistance, bCustomExtents, ref extents);
        }

        internal static bool FindRandomPointInCircle(uint agentTypeID, Vector3 pos, float radius, out Vector3 outRandomPoint, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return FindRandomPointInCircle_Native(agentTypeID, ref pos, radius, out outRandomPoint, bCustomExtents, ref extents);
        }

        internal static Vector3[] FindStraightPath(uint agentTypeID, Vector3 start, Vector3 end, uint maxPolys, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return FindStraightPath_Native(agentTypeID, ref start, ref end, maxPolys, bCustomExtents, ref extents);
        }

        internal static Vector3[] FindSmoothPath(uint agentTypeID, Vector3 start, Vector3 end, uint maxPolys, uint maxSmooth, Vector3? searchHalfExtents)
        {
            bool bCustomExtents = ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return FindSmoothPath_Native(agentTypeID, ref start, ref end, maxPolys, maxSmooth, bCustomExtents, ref extents);
        }

        internal static bool ToNativeSearchExtents(Vector3? searchHalfExtents, out Vector3 extents)
        {
            if (!searchHalfExtents.HasValue)
            {
                extents = new Vector3(0f);
                return false;
            }

            extents = searchHalfExtents.Value;
            if (extents.X <= 0f || extents.Y <= 0f || extents.Z <= 0f)
                throw new ArgumentOutOfRangeException(nameof(searchHalfExtents), "Search extents must be positive");

            return true;
        }

        // Needs to be called every frame. Returns true when finished (arrived, or there's no path).
        // Arrived means within `acceptanceDistance` on XZ, with the target within the agent's reach vertically (so the origin doesn't need to be at the feet).
        // If the target can't be reached, the entity goes as close as possible and waits there.
        // - An entity with a valid `NavigationCrowdAgentComponent` is moved by it: its move target is set, and `moveSpeed` becomes the agent's Max Speed.
        //   It moves on its agent's nav mesh, `fallbackAgentType` isn't used. If the agent isn't valid (for example, the scene doesn't have a nav mesh of its type),
        //   it doesn't move, and the result is `NoPath`.
        //   Pass a `MoveToTargetState` to follow a moving target. Without one, a new target is only requested when the agent has none.
        // - Other entities move on the nav mesh of `fallbackAgentType` (`Auto` means the default agent type). If the scene doesn't have it, they don't move, and the result is `NoPath`.
        // - An entity with a dynamic (non-kinematic) rigid body is moved by setting its horizontal velocity, so that physics keeps it on the ground.
        // - Other entities are teleported along the path.
        // Call `StopMoving` if you stop calling this before it returns true.
        public static bool MoveToTarget(Entity entity, Vector3 targetLocation, float acceptanceDistance, float ts, float moveSpeed, float rotationSpeed,
            MoveToTargetState state = null, NavAgentType fallbackAgentType = default)
        {
            if (entity.HasComponent<NavigationCrowdAgentComponent>())
            {
                NavigationCrowdAgentComponent agent = entity.GetComponent<NavigationCrowdAgentComponent>();
                if (agent.IsValid())
                    return MoveCrowdAgentToTarget(entity, agent, targetLocation, acceptanceDistance, ts, moveSpeed, rotationSpeed, state);

                // It doesn't fall back to another way of moving, so that it's noticeable that something's wrong
                state?.Reset();
                SetResult(state, MoveToTargetResult.NoPath);
                return true;
            }

            NavMesh navMesh = NavMesh.GetNavMeshFor(entity, fallbackAgentType);
            if (!navMesh.IsValid())
            {
                StopMoving(entity);
                SetResult(state, MoveToTargetResult.NoPath);
                return true;
            }

            Vector3 location = entity.WorldLocation;

            if (HasArrived(navMesh, location, targetLocation, acceptanceDistance))
            {
                StopMoving(entity);
                SetResult(state, MoveToTargetResult.Arrived);
                return true;
            }

            Vector3[] path = navMesh.FindSmoothPath(location, targetLocation);
            if (path.Length == 0)
            {
                StopMoving(entity);
                SetResult(state, MoveToTargetResult.NoPath);
                return true;
            }

            // Path heights are approximate. The nav mesh has no detail mesh, so on slopes its surface can be far above or below the real ground.
            // So path points are compared and followed on XZ only.
            Vector3 nextPoint = path[0];
            if (HorizontalLength2(nextPoint - location) < 0.01f && path.Length > 1)
            {
                nextPoint = path[1];
            }

            Vector3 horizontalDir = nextPoint - location;
            horizontalDir.Y = 0f;
            float horizontalLength = Mathf.Length(horizontalDir);
            if (horizontalLength < 0.0001f)
            {
                // At the end of the path but not arrive. The target is unreachable, or too far above or below the nav mesh
                StopMoving(entity);
                return false;
            }
            horizontalDir /= horizontalLength;

            RigidBodyComponent rigidBody = GetDynamicRigidBody(entity);
            if (rigidBody != null)
            {
                // Teleporting a dynamic body fights the physics (and can push it into thin colliders). Its vertical velocity is left to physics
                Vector3 velocity = rigidBody.GetLinearVelocity();
                rigidBody.SetLinearVelocity(new Vector3(horizontalDir.X * moveSpeed, velocity.Y, horizontalDir.Z * moveSpeed));
            }
            else
            {
                // No physics to keep it on the ground, so it follows the path's heights
                Vector3 dir = Mathf.Normalize(nextPoint - location);
                entity.WorldLocation = location + dir * (moveSpeed * ts);
            }
            entity.WorldRotation = Mathf.Slerp(entity.WorldRotation, Mathf.LookAtY(horizontalDir), ts * rotationSpeed);

            return false;
        }

        // Stops the movement started by `MoveToTarget`.
        // Resets a crowd agent's move target, or the horizontal velocity of a dynamic rigid body.
        public static void StopMoving(Entity entity)
        {
            NavigationCrowdAgentComponent agent = GetCrowdAgent(entity);
            if (agent != null)
            {
                agent.ResetMoveTarget();
                return;
            }

            RigidBodyComponent rigidBody = GetDynamicRigidBody(entity);
            if (rigidBody != null)
            {
                Vector3 velocity = rigidBody.GetLinearVelocity();
                rigidBody.SetLinearVelocity(new Vector3(0f, velocity.Y, 0f));
            }
        }

        private static bool MoveCrowdAgentToTarget(Entity entity, NavigationCrowdAgentComponent agent, Vector3 targetLocation, float acceptanceDistance, float ts, float moveSpeed, float rotationSpeed, MoveToTargetState state)
        {
            AgentSettings settings = agent.GetSettings();
            if (!agent.GetLocation(out Vector3 location))
                location = entity.WorldLocation;

            if (HasArrived(agent.GetNavMesh(), location, targetLocation, acceptanceDistance, settings.AgentHeight))
            {
                agent.ResetMoveTarget();
                state?.Reset();
                SetResult(state, MoveToTargetResult.Arrived);
                return true;
            }

            if (settings.MaxSpeed != moveSpeed)
            {
                settings.MaxSpeed = moveSpeed;
                agent.SetSettings(settings);
            }

            AgentTargetState targetState = agent.GetTargetState();
            bool bHasTarget = targetState != AgentTargetState.None && targetState != AgentTargetState.Failed && targetState != AgentTargetState.Velocity;
            bool bRequest = !bHasTarget;
            if (state != null)
            {
                // A new move target is requested when the target moves farther than this since the last request.
                // Near the target, half the acceptance distance, so that the agent always arrives at it.
                // Far away, 10% of the distance, so that Detour can finish its path search (a new request throws the pending one away)
                float updateDistance = Math.Max(0.5f * acceptanceDistance, 0.1f * (float)Math.Sqrt(HorizontalLength2(targetLocation - location)));
                bRequest |= !state.bHasRequest || HorizontalLength2(targetLocation - state.RequestedTarget) > (updateDistance * updateDistance);
            }

            if (bRequest)
            {
                if (!agent.SetMoveTarget(targetLocation))
                {
                    // The target is off the nav mesh
                    agent.ResetMoveTarget();
                    state?.Reset();
                    SetResult(state, MoveToTargetResult.NoPath);
                    return true;
                }

                if (state != null)
                {
                    state.bHasRequest = true;
                    state.RequestedTarget = targetLocation;
                }
            }

            // The crowd moves the entity, but doesn't rotate it. Face the direction of movement
            if (agent.GetVelocity(out Vector3 velocity))
            {
                velocity.Y = 0f;
                float speed = Mathf.Length(velocity);
                if (speed > 0.01f)
                    entity.WorldRotation = Mathf.Slerp(entity.WorldRotation, Mathf.LookAtY(velocity / speed), ts * rotationSpeed);
            }

            return false;
        }

        private static NavigationCrowdAgentComponent GetCrowdAgent(Entity entity)
        {
            if (entity == null || !entity.HasComponent<NavigationCrowdAgentComponent>())
                return null;

            NavigationCrowdAgentComponent agent = entity.GetComponent<NavigationCrowdAgentComponent>();
            return agent.IsValid() ? agent : null;
        }

        private static RigidBodyComponent GetDynamicRigidBody(Entity entity)
        {
            if (entity == null || !entity.HasComponent<RigidBodyComponent>())
                return null;

            RigidBodyComponent rigidBody = entity.GetComponent<RigidBodyComponent>();
            if (rigidBody.GetBodyType() != PhysicsBodyType.Dynamic || rigidBody.IsKinematic())
                return null;

            return rigidBody;
        }

        private static void SetResult(MoveToTargetState state, MoveToTargetResult result)
        {
            if (state != null)
                state.Result = result;
        }

        // Arrived within `acceptanceDistance` on XZ, and the target is at most the agent type's `Height` above the nav mesh under `location`
        // and at most its `Max Climb` below it on the same surface. Not in 3D, because an entity's origin isn't necessarily at its feet.
        // `agentHeight` overrides the agent type's one (crowd agents have their own). Falls back to 3D if there's no nav mesh under `location`
        private static bool HasArrived(NavMesh navMesh, Vector3 location, Vector3 targetLocation, float acceptanceDistance, float? agentHeight = null)
        {
            Vector3 toTarget = targetLocation - location;
            float acceptanceDistance2 = acceptanceDistance * acceptanceDistance;
            if (HorizontalLength2(toTarget) > acceptanceDistance2)
                return false;

            if (navMesh == null || !navMesh.IsValid() || !navMesh.GetAgentType().GetInfo(out NavAgentTypeInfo agentInfo) || !navMesh.FindNearestPoint(location, out Vector3 navLocation))
                return Mathf.Length2(toTarget) <= acceptanceDistance2;

            float heightAboveNavMesh = targetLocation.Y - navLocation.Y;
            if (heightAboveNavMesh > (agentHeight ?? agentInfo.Height) || heightAboveNavMesh < -agentInfo.MaxClimb)
                return false;

            // The target must be on the same surface where it snaps to the nav mesh, it's within `Max Climb` of the entity's nav mesh point.
            // Otherwise, standing on the floor right below a ramp counts as arriving at a target on the ramp.
            // If it's not the same surface, the entity keeps following the path to the target's point
            if (navMesh.FindNearestPoint(targetLocation, out Vector3 targetNavLocation) && Math.Abs(targetNavLocation.Y - navLocation.Y) > agentInfo.MaxClimb)
                return false;

            return true;
        }

        private static float HorizontalLength2(Vector3 v)
        {
            return v.X * v.X + v.Z * v.Z;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindDistanceToWall_Native(uint agentTypeID, ref Vector3 pos, float maxRadius, out Vector3 outHitPos, out Vector3 outHitNormal, out float outHitDistance, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindRandomPoint_Native(uint agentTypeID, out Vector3 outRandomPoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindRandomPointInCircle_Native(uint agentTypeID, ref Vector3 pos, float radius, out Vector3 outRandomPoint, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool FindNearestPoint_Native(uint agentTypeID, ref Vector3 point, out Vector3 nearestPoint, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsValidPoint_Native(uint agentTypeID, ref Vector3 point, out Vector3 navMeshPoint);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool HasCompletePath_Native(uint agentTypeID, ref Vector3 from, ref Vector3 to, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetSearchHalfExtents_Native(uint agentTypeID, out Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetSettings_Native(uint agentTypeID, out NavMeshSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3[] FindStraightPath_Native(uint agentTypeID, ref Vector3 start, ref Vector3 end, uint maxPolys, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3[] FindSmoothPath_Native(uint agentTypeID, ref Vector3 start, ref Vector3 end, uint maxPolys, uint maxSmooth, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);
    }

    public class CrowdNavigation
    {
        // Controls all existing agents of all nav meshes (see `NavMesh.SetCrowdMoveTarget` for the agents of one nav mesh).
        // `target` is snapped to each nav mesh that has agents with `searchHalfExtents`. If `searchHalfExtents` is null, extents settings from the nav mesh is used.
        // Returns false if the scene doesn't have a nav mesh, or if it failed for any of the nav meshes with agents (their agents keep the current target)
        public static bool SetMoveTarget(Vector3 target, Vector3? searchHalfExtents = null)
        {
            bool bCustomExtents = Navigation.ToNativeSearchExtents(searchHalfExtents, out Vector3 extents);
            return SetMoveTarget_Native(ref target, bCustomExtents, ref extents);
        }

        // Resets the move target of all agents of all nav meshes
        public static void ResetMoveTarget()
        {
            ResetMoveTarget_Native();
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool SetMoveTarget_Native(ref Vector3 pos, bool bCustomSearchExtents, ref Vector3 searchHalfExtents);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ResetMoveTarget_Native();
    }
}
