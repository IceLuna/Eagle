using System.Runtime.CompilerServices;

namespace Eagle
{
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
    public static class Sound2D
    {
        public static void Play(string filepath, float volume = 1f, int loopCount = -1)
        {
            Play_Native(filepath, volume, loopCount);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Play_Native(string filepath, float volume, int loopCount);
    }

    public static class Sound3D
    {
        public static void Play(string filepath, Vector3 position, float volume = 1f, int loopCount = -1)
        {
            Play_Native(filepath, in position, volume, loopCount);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Play_Native(string filepath, in Vector3 position, float volume, int loopCount);
    }
}
