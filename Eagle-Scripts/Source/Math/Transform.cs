using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rotator
    {
        public Quat Rotation;

        public static implicit operator Rotator(Quat value)
        {
            Rotator rotator = new Rotator();
            rotator.Rotation = value;
            return rotator;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Location;
        public Rotator Rotation;
        public Vector3 Scale;

        public static Transform Unit()
        {
            Transform result = new Transform();
            result.Location = new Vector3(0);
            result.Rotation = Quat.Unit();
            result.Scale = new Vector3(1);

            return result;
        }
    }
}
