C# Audio
========

C# Audio API allows you to load and play sounds. It has two main classes: ``Sound2D`` and ``Sound3D`` that are derived from ``Sound`` class.

`RollOffModel` enum
-------------------
It allows you to specify how 3D sounds attenuate as the distance between the listener and the sound increases.
It can be used with ``AudioComponent``.

.. code-block:: csharp

    public enum RollOffModel
    {
        Linear, Inverse, LinearSquare, InverseTapered,
        Default = Inverse
    }

``Linear`` roll off model means that a sounds will follow a linear roll off model where `MinDistance` = full volume, `MaxDistance` = silence.

``Inverse`` roll off model means that a sounds will follow an inverse roll off model where `MinDistance` = full volume, `MaxDistance` = where sound stops attenuating, and rolloff is fixed according to the global roll off factor.

``LinearSquare`` roll off model means that a sounds will follow a linear-square roll off model where `MinDistance` = full volume, `MaxDistance` = silence.

``InverseTapered`` roll off model means that a sounds will follow the inverse roll off model at distances close to `MinDistance` and a linear-square roll off close to `MaxDistance`.

`Reverb Preset` enum
-------------------
It allows you to simulate different types of environments.
It can be used with ``ReverbComponent``.

.. code-block:: csharp

    public enum ReverbPreset
    {
        Generic, PaddedCell, Room, Bathroom, LivingRoom, StoneRoom, Auditorium, ConcertHall,
        Cave, Arena, Hangar, CarpettedHallway, Hallway, StoneCorridor, Alley, Forest, City, Mountains,
        Quarry, Plain, ParkingLot, SewerPipe, UnderWater
    }

`Sound Settings`
----------------
``Sound Settings`` struct is used when creating a sound, either `2D` or `3D`.

.. code-block:: csharp

	//@ VolumeMultiplier. Gets multiplied by `Audio` volume to determine final volume. 0 = silent, 1 = full. Negative level inverts the signal. Values larger than 1 amplify the signal.
	//@ Pan. -1 = Completely on the left. +1 = Completely on the right
	//@ Pitch. Any value between 0 and 10. Gets multiplied by `Audio` pitch to determine final pitch.
	//@ LoopCount. -1 = Loop Endlessly; 0 = Play once; 1 = Play twice, etc...
	//@ FFTSamples. Number of samples in the output of spectrum data. Must be the power of 2 between 64 and 8192
	//@ FFTType. Defines the method that will be used to calculate the spectrum data. https://www.fmod.com/docs/2.00/api/core-api-common-dsp-effects.html#fmod_dsp_fft
	//@ IsStreaming. When you stream a sound, you can only have one instance of it playing at any time.
	//	           This limitation exists because there is only one decode buffer per stream.
	//	           As a rule of thumb, streaming is great for music tracks, voice cues, and ambient tracks,
	//	           while most sound effects should be loaded into memory
	//@ bEnableFFT. If set to true, you can get the spectrum data of the sound
    public struct SoundSettings
    {
        public SoundSettings(float volume, float pan = 0f, float pitch = 1f, int loopCount = -1, uint fftSamples = 256, FFTWindowType fftType = FFTWindowType.Rect,
            bool bLooping = false, bool bMuted = false, bool bStreaming = false, bool bEnableFFT = false);

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

`Sound` class
-------------
It's a base class of other `Sound` classes.

.. code-block:: csharp

    abstract public class Sound
    {
        public void Play();
        public void Stop();
        public void SetPaused(bool bPaused);
        public void SetPosition(uint ms); // Seek position
        public uint GetPosition(); // Seek position
        public bool IsPlaying();
        public void SetSettings(SoundSettings settings);
        public SoundSettings GetSettings();

        public void SetFFTEnabled(bool bEnable);
		public bool IsFFTEnabled();

		public void SetFFTSamples(uint samples):
		public uint GetFFTSamples();

		public void SetFFTType(FFTWindowType type);
		public FFTWindowType GetFFTType();

		// channelIndex. Allows to get data from a specific audio channel. Starts from 0. If -1, get average over all channels
        public bool GetSpectrumData(ref float[] data, int channelIndex = -1);

        public float GetSampleRate();

        public int GetChannelsCount();
    }


`Sound2D` class
---------------
It allows you to create 2D sounds.

.. code-block:: csharp

    public class Sound2D : Sound
    {
        public Sound2D(AssetAudio asset, SoundSettings settings);
    }

    
`Sound3D` class
---------------
It allows you to create 3D sounds that have world position and velocity.

.. code-block:: csharp

    public class Sound3D : Sound
    {
        public Sound3D(AssetAudio asset, Vector3 position, RollOffModel rollOff, SoundSettings settings);
        
        // The minimum distance is the point at which the sound starts attenuating.
        // If the listener is any closer to the source than the minimum distance,
        // the sound will play at full volume.
        public void SetMinDistance(float minDistance);
        
        // The maximum distance is the point at which the sound stops
        // attenuatingand its volume remains constant(a volume which is not
        // necessarily zero)
        public void SetMaxDistance(float maxDistance);
        
        public void SetMinMaxDistance(float minDistance, float maxDistance);
        
        public void SetWorldPosition(Vector3 position);
        
        public void SetVelocity(Vector3 velocity);
        
        public void SetRollOffModel(RollOffModel rollOff);
        
        public float GetMinDistance();
        
        public float GetMaxDistance();
        
        public Vector3 GetWorldPosition();
        
        public Vector3 GetVelocity();
        
        public RollOffModel GetRollOffModel();
    }
