using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    public enum FFTWindowType
    {
        Rect,
		Triangle,
		Hamming,
		Hanning,
		Blackman,
		BlackmanHarris
    }

    //@ Volume. 0 = silent, 1 = full. Negative level inverts the signal. Values larger than 1 amplify the signal.
    //@ Pan. -1 = Completely on the left. +1 = Completely on the right
    //@ Pitch. Any value between 0 and 10. Gets multiplied by `Audio` pitch to determine final pitch.
    //@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
    //@ FFTSamples. Number of samples in the output of spectrum data. Must be the power of 2 between 64 and 8192.
    //@ FFTType. Defines the method that will be used to calculate the spectrum data. https://www.fmod.com/docs/2.00/api/core-api-common-dsp-effects.html#fmod_dsp_fft
    //@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
    //	           This limitation exists because there is only one decode buffer per stream.
    //	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
    //	           while most sound effects should be loaded into memory
    //@ bEnableFFT. If set to true, you can get the spectrum data of the sound

    [StructLayout(LayoutKind.Sequential)]
    public struct SoundSettings
    {
        public SoundSettings(float volume, float pan = 0f, float pitch = 1f, int loopCount = -1, uint fftSamples = 256, FFTWindowType fftType = FFTWindowType.Rect,
            bool bLooping = false, bool bMuted = false, bool bStreaming = false, bool bEnableFFT = false)
        {
            Volume = volume;
            Pan = pan;
            Pitch = pitch;
            LoopCount = loopCount;
            FFTSamples = fftSamples;
            FFTType = fftType;
            this.bLooping = bLooping;
            this.bStreaming = bStreaming;
            this.bMuted = bMuted;
            this.bEnableFFT = bEnableFFT;
        }

        public float Volume;
        public float Pan;
        public float Pitch;
        public int LoopCount;
        public uint FFTSamples;
        public FFTWindowType FFTType;
        public bool bLooping;
        public bool bStreaming;
        public bool bMuted;
		public bool bEnableFFT;
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

        public void SetFFTEnabled(bool bEnable)
		{
            SetFFTEnabled_Native(ID, bEnable);
        }

		public bool IsFFTEnabled()
        {
            return IsFFTEnabled_Native(ID);
        }

		public void SetFFTSamples(uint samples)
		{
            SetFFTSamples_Native(ID, samples);
        }

		public uint GetFFTSamples()
        {
            return GetFFTSamples_Native(ID);
        }

		public void SetFFTType(FFTWindowType type)
		{
            SetFFTType_Native(ID, type);
        }

		public FFTWindowType GetFFTType()
        {
            return GetFFTType_Native(ID);
        }

		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
        public bool GetSpectrumData(ref float[] data, int channelIndex = -1)
        {
            return GetSpectrumData(ID, data, channelIndex);
        }

        public float GetSampleRate()
        {
            return GetSampleRate_Native(ID);
        }

        public int GetChannelsCount()
        {
            return GetChannelsCount_Native(ID);
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

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool GetSpectrumData(GUID id, float[] data, int channelIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTEnabled_Native(GUID id, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool IsFFTEnabled_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTSamples_Native(GUID id, uint value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint GetFFTSamples_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void SetFFTType_Native(GUID id, FFTWindowType value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern FFTWindowType GetFFTType_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float GetSampleRate_Native(GUID id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int GetChannelsCount_Native(GUID id);
    }

    public class Sound2D : Sound
    {
        public Sound2D(AssetAudio asset, SoundSettings settings)
        {
            ID = Create_Native(asset.GetGUID(), ref settings);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GUID Create_Native(GUID assetID, ref SoundSettings settings);
    }

    public class Sound3D : Sound
    {
        public Sound3D(AssetAudio asset, Vector3 position, RollOffModel rollOff, SoundSettings settings)
        {
            ID = Create_Native(asset.GetGUID(), ref position, rollOff, ref settings);
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
        internal static extern GUID Create_Native(GUID assetID, ref Vector3 position, RollOffModel rollOff, ref SoundSettings settings);

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
