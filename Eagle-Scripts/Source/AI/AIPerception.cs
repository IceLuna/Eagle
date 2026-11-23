using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    [Flags]
    public enum AISense : uint
    {
        Sight   = 1 << 0,
        Hearing = 1 << 1,
        Damage  = 1 << 2,
        All     = Sight | Hearing | Damage,
    }

    // Which entities count as a one.
    // For example, the AI's own body that doesn't block its sight
    public enum AIHierarchyScope : uint
    {
        Self,
        SelfAndChildren,
        PhysicsBody, // The nearest entity from this one up with a rigid body or a character controller, and its children. If there's none, the same as `SelfAndChildren`
    }

	// What was sensed
    public struct AIStimulus
    {
        public Vector3 Location; // Where it was sensed
        public float Strength; // Loudness for hearing, the amount for damage, 1 for sight
        public AISense Sense;
        public bool bSensed;  // False when a target is lost from sight
    }

    // Settings of something that has perception.
    [StructLayout(LayoutKind.Sequential)]
    public struct AIPerceptionSettings
    {
        // Sight. It sees what's within the range and the cone, if the ray to it isn't blocked
        public float SightRange;
        public float LoseSightRange; // Something that's seen is lost when it's further than this
        public float SightHalfAngle; // Degrees from the forward direction. So, 180 means all around
        public float SightInterval;  // Seconds between checks. Checks of different AIs are spread over the interval
        public float SightMaxAge;    // Seconds it's remembered after it's lost
        public CollisionGroup SightBlockedBy;

        // Hearing. A noise is heard within `HearingRange * loudness`
        public float HearingRange;
        public float HearingMaxAge; // Seconds it's remembered after it's heard

        // Damage.
        public float DamageMaxAge; // Seconds it's remembered after it caused damage

		// The AI's own body doesn't trigger/block the sight.
        public AIHierarchyScope IgnoreOwnBody;

        public bool bSight;
        public bool bHearing;
        public bool bDamage;
        public bool bDrawDebug;

		// If enabled, the max ages aren't used
        public bool bRememberSightForever;
        public bool bRememberHearingForever;
        public bool bRememberDamageForever;
    }

	// Settings of something that can be perceived.
    [StructLayout(LayoutKind.Sequential)]
    public struct AIPerceptionSourceSettings
    {
        public Vector3 SightOffset; // The point AIs look at, relative to the entity (for example, the chest)
        public AIHierarchyScope SeenWhenRayHits; // What the sight ray can hit for it to be seen
        public bool bSight;
    }

    public static class AIPerception
    {
        // @instigator. Who made the noise. If it's null, AIs remember it as an unknown source (see `AIPerceptionComponent.GetUnknownStimulus()`)
        public static void ReportNoise(Vector3 location, float loudness, Entity instigator = null)
        {
            ReportNoise_Native(location, loudness, instigator != null ? instigator.ID : GUID.Null());
        }

        // `victim` senses `instigator`, if it has `AIPerceptionComponent`.
        // `Entity.TakeDamage()` reports it, so this is for damage that doesn't go through it.
        // @instigator. Who caused it. If it's null, it's remembered as an unknown source (see `AIPerceptionComponent.GetUnknownStimulus()`).
        // @cameFrom. Where the damage came from. If it's null, it's the instigator's location, or the victim's own if there's no instigator
        public static void ReportDamage(Entity victim, Entity instigator, float amount, Vector3? cameFrom = null)
        {
            if (victim == null)
                return;

            Vector3 location = cameFrom ?? new Vector3(0f);
            ReportDamage_Native(victim.ID, instigator != null ? instigator.ID : GUID.Null(), amount, in location, cameFrom.HasValue);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ReportNoise_Native(in Vector3 location, float loudness, in GUID instigatorID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void ReportDamage_Native(in GUID victimID, in GUID instigatorID, float amount, in Vector3 cameFrom, bool bCameFrom);
    }
}
