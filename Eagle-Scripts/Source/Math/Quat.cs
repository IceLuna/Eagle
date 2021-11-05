using System;
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

        public Quat(float w, float y, float z, float x)
        {
            W = w;
            X = x;
            Y = y;
            Z = z;
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

        //TODO: EulerAngles; FromEulerAngles

        public static Quat Unit()
        {
            return new Quat(1f, 0f, 0f, 0f);
        }

        public static Quat operator*(Quat left, Quat right)
        {
            Quat p = left;
            Quat q = right;
            Quat result = new Quat();

            result.W = p.W * q.W - p.X * q.X - p.Y * q.Y - p.Z * q.Z;
            result.X = p.W * q.X + p.X * q.W + p.Y * q.Z - p.Z * q.Y;
            result.Y = p.W * q.Y + p.Y * q.W + p.Z * q.X - p.X * q.Z;
            result.Z = p.W * q.Z + p.Z * q.W + p.X * q.Y - p.Y * q.X;

            return result;
        }

        public static Quat operator/ (Quat left, float scalar)
        {
            return new Quat(left.W / scalar, left.X / scalar, left.Y / scalar, left.Z / scalar);
        }

        public override bool Equals(object obj) => obj is Quat other && this.Equals(other);

        public bool Equals(Quat right) => X == right.X && Y == right.Y && Z == right.Z && W == right.W;

        public static bool operator ==(Quat left, Quat right) => left.Equals(right);
        public static bool operator !=(Quat left, Quat right) => !(left == right);

        public override string ToString()
        {
            return "Quat[" + X + ", " + Y + ", " + Z + ", " + W + "]";
        }

        public override int GetHashCode() => (X, Y, Z, W).GetHashCode();
    }
}
