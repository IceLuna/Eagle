using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    //@ Volume. 0.0 = Silence; 1.0 = Max Volume
    //@ Pan. -1 = Completely on the left. +1 = Completely on the right
    //@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
    //@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
    //	           This limitation exists because there is only one decode buffer per stream.
    //	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
    //	           while most sound effects should be loaded into memory

    [StructLayout(LayoutKind.Sequential)]
    public struct SoundSettings
    {
        public SoundSettings(float volume, float pan = 0f, int loopCount = -1, bool bLooping = false, bool bMuted = false, bool bStreaming = false)
        {
            Volume = volume;
            Pan = pan;
            LoopCount = loopCount;
            this.bLooping = bLooping;
            this.bStreaming = bStreaming;
            this.bMuted = bMuted;
        }

        public float Volume;
        public float Pan;
        public int LoopCount;
        public bool bLooping;
        public bool bStreaming;
        public bool bMuted;
    };

    public enum RollOffModel
    {
        Linear, Inverse, LinearSquare, InverseTapered,
        Default = Inverse
    }

    public enum ReverbPreset
    {
        Generic, PaddedCell, Room, Bathroom, LivingRoom, StoneRoom, Auditorium, ConcertHall,
        Cave, Arena, Hangar, CarpettedHallway, Hallway, StoneCorridor, Alley, Forest, City, Mountains,
        Quarry, Plain, ParkingLot, SewerPipe, UnderWater
    };

    abstract public class Sound
    {
        protected GUID ID;

        public void Play()
        {
            Play_Native(ID);
        }

        public void Stop()
        {
            Stop_Native(ID);
        }

        public void SetPaused(bool bPaused)
        {
            SetPaused_Native(ID, bPaused);
        }

        public void SetPosition(uint ms)
        {
            SetPosition_Native(ID, ms);
        }

        public uint GetPosition()
        {
            return GetPosition_Native(ID);
        }

        public bool IsPlaying()
        {
            return IsPlaying_Native(ID);
        }

        public void SetSettings(SoundSettings settings)
        {
            SetSettings_Native(ID, ref settings);
        }

        public SoundSettings GetSettings()
        {
            GetSettings_Native(ID, out SoundSettings settings);
            return settings;
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetSettings_Native(GUID id, ref SoundSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetSettings_Native(GUID id, out SoundSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Play_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Stop_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPaused_Native(GUID id, bool bPaused);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsPlaying_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetPosition_Native(GUID id, uint ms);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetPosition_Native(GUID id);
    }

    public class Sound2D : Sound
    {
        public Sound2D(string filepath, SoundSettings settings)
        {
            ID = Create_Native(filepath, ref settings);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath, ref SoundSettings settings);
    }

    public class Sound3D : Sound
    {
        public Sound3D(string filepath, Vector3 position, RollOffModel rollOff, SoundSettings settings)
        {
            ID = Create_Native(filepath, ref position, rollOff, ref settings);
        }

        //The minimum distance is the point at which the sound starts attenuating.
        //If the listener is any closer to the source than the minimum distance,
        //the sound will play at full volume.
        public void SetMinDistance(float minDistance)
        {
            SetMinDistance_Native(ID, minDistance);
        }

        //The maximum distance is the point at which the sound stops
        //attenuatingand its volume remains constant(a volume which is not
        //necessarily zero)
        public void SetMaxDistance(float maxDistance)
        {
            SetMaxDistance_Native(ID, maxDistance);
        }

        public void SetMinMaxDistance(float minDistance, float maxDistance)
        {
            SetMinMaxDistance_Native(ID, minDistance, maxDistance);
        }

        public void SetWorldPosition(Vector3 position)
        {
            SetWorldPosition_Native(ID, ref position);
        }

        public void SetVelocity(Vector3 velocity)
        {
            SetVelocity_Native(ID, ref velocity);
        }

        public void SetRollOffModel(RollOffModel rollOff)
        {
            SetRollOffModel_Native(ID, rollOff);
        }

        public float GetMinDistance()
        {
            return GetMinDistance_Native(ID);
        }

        public float GetMaxDistance()
        {
            return GetMaxDistance_Native(ID);
        }

        public Vector3 GetWorldPosition()
        {
            GetWorldPosition_Native(ID, out Vector3 position);
            return position;
        }

        public Vector3 GetVelocity()
        {
            GetVelocity_Native(ID, out Vector3 velocity);
            return velocity;
        }

        public RollOffModel GetRollOffModel()
        {
            return GetRollOffModel_Native(ID);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(string filepath, ref Vector3 position, RollOffModel rollOff, ref SoundSettings settings);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMinDistance_Native(GUID id, float min);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMaxDistance_Native(GUID id, float max);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetMinMaxDistance_Native(GUID id, float min, float max);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetWorldPosition_Native(GUID id, ref Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetVelocity_Native(GUID id, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetRollOffModel_Native(GUID id, RollOffModel rollOff);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMinDistance_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetMaxDistance_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetWorldPosition_Native(GUID id, out Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void GetVelocity_Native(GUID id, out Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern RollOffModel GetRollOffModel_Native(GUID id);
    }
}
