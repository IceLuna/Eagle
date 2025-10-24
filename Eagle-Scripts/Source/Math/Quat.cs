using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Quat : IEquatable<Quat>
    {
        public float X;
        public float Y;
        public float Z;
        public float W;

        public Quat(float w, float x, float y, float z)
        {
            W = w;
            X = x;
            Y = y;
            Z = z;
        }

        public Quat(float w, Vector3 v)
        {
            W = w;
            X = v.X;
            Y = v.Y;
            Z = v.Z;
        }

        public static Quat Conjugate(Quat val)
        {
            return new Quat(val.W, -val.X, -val.Y, -val.Z);
        }

        public static float Dot(Quat left, Quat right)
        {
            return left.X * right.X + left.Y * right.Y + left.Z * right.Z + left.W * right.W;
        }

        public Quat Inverse()
        {
            return Conjugate(this) / Dot(this, this);
        }

        public float Lenght()
        {
            return (float)Math.Sqrt(Quat.Dot(this, this));
        }

        public void Normalize()
        {
            float oneOverLen = 1f / Lenght();
            W = W * oneOverLen;
            X = X * oneOverLen;
            Y = Y * oneOverLen;
            Z = Z * oneOverLen;
        }

        // Returns in radians. X - pitch, Y - yaw, Z - roll
        public Vector3 EulerAngles()
        {
            return EulerAngles_Native(ref this);
        }

        public static Quat FromEulerAngles(Vector3 radians)
        {
            return FromEulerAngles_Native(ref radians);

        }

        public static Quat Unit()
        {
            return new Quat(1f, 0f, 0f, 0f);
        }

        public static Quat operator*(Quat left, Quat right)
        {
            return Mul_Native(ref left, ref right);
        }

        public static Quat operator/ (Quat left, float scalar)
        {
            return new Quat(left.W / scalar, left.X / scalar, left.Y / scalar, left.Z / scalar);
        }

        public override bool Equals(object obj) => obj is Quat other && this.Equals(other);

        public bool Equals(Quat right) => X == right.X && Y == right.Y && Z == right.Z && W == right.W;

        public bool Equals(Quat right, float epsilon)
        {
            return
                Mathf.Abs(X - right.X) < epsilon &&
                Mathf.Abs(Y - right.Y) < epsilon &&
                Mathf.Abs(Z - right.Z) < epsilon &&
                Mathf.Abs(W - right.W) < epsilon;
        }

        public static bool operator ==(Quat left, Quat right) => left.Equals(right);
        public static bool operator !=(Quat left, Quat right) => !(left == right);

        public override string ToString()
        {
            return "Quat[" + X + ", " + Y + ", " + Z + ", " + W + "]";
        }

        public override int GetHashCode() => (X, Y, Z, W).GetHashCode();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Quat Mul_Native(ref Quat left, ref Quat right);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Vector3 EulerAngles_Native(ref Quat q);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Quat FromEulerAngles_Native(ref Vector3 rads);
    }
}
