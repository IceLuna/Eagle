C# Audio
========

Currently, Audio API is far from being rich.

`Sound2D` class
---------------
It's a static class that contains just one function:

.. code-block:: csharp

    public static void Play(string filepath, float volume = 1f, int loopCount = -1)

    
`Sound3D` class
---------------
The same as for ``Sound2D``, it's a static class that contains just one function but it's a bit different.
As you can see, you can pass a world position for the sound to use.

.. code-block:: csharp

    // @loopCount. `-1` = `Loop Endlessly`; `0` = `Play once`; `1` = `Play twice`, etc...
    public static void Play(string filepath, Vector3 position, float volume = 1f, int loopCount = -1)

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

`ReverbPreset` enum
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
