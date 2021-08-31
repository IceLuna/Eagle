using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3 : IEquatable<Vector3>
    {
        public float X;
        public float Y;
        public float Z;

        public Vector3(float scalar)
        {
            X = Y = Z = scalar;
        }

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static Vector3 operator +(Vector3 left, Vector3 right)
        {
            return new Vector3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
        }

        public static Vector3 operator +(Vector3 left, float scalar)
        {
            return new Vector3(left.X + scalar, left.Y + scalar, left.Z + scalar);
        }

        public static Vector3 operator +(float scalar, Vector3 right)
        {
            return new Vector3(right.X + scalar, right.Y + scalar, right.Z + scalar);
        }

        public static Vector3 operator -(Vector3 left, Vector3 right)
        {
            return new Vector3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
        }

        public static Vector3 operator -(Vector3 left, float scalar)
        {
            return new Vector3(left.X - scalar, left.Y - scalar, left.Z - scalar);
        }

        public static Vector3 operator -(float scalar, Vector3 right)
        {
            return new Vector3(scalar - right.X, scalar - right.Y, scalar - right.Z);
        }

        public static Vector3 operator -(Vector3 vector)
        {
            return new Vector3(-vector.X, -vector.Y, -vector.Z);
        }

        public static Vector3 operator *(Vector3 left, Vector3 right)
        {
            return new Vector3(left.X * right.X, left.Y * right.Y, left.Z * right.Z);
        }

        public static Vector3 operator *(Vector3 left, float scalar)
        {
            return new Vector3(left.X * scalar, left.Y * scalar, left.Z * scalar);
        }

        public static Vector3 operator *(float scalar, Vector3 right)
        {
            return new Vector3(right.X * scalar, right.Y * scalar, right.Z * scalar);
        }

        public static Vector3 operator /(Vector3 left, Vector3 right)
        {
            return new Vector3(left.X / right.X, left.Y / right.Y, left.Z / right.Z);
        }

        public static Vector3 operator /(Vector3 left, float scalar)
        {
            return new Vector3(left.X / scalar, left.Y / scalar, left.Z / scalar);
        }

        public static Vector3 operator /(float scalar, Vector3 right)
        {
            return new Vector3(scalar / right.X, scalar / right.Y, scalar / right.Z);
        }

        public override bool Equals(object obj) => obj is Vector3 other && this.Equals(other);

        public bool Equals(Vector3 right) => X == right.X && Y == right.Y && Z == right.Z;

        public static bool operator ==(Vector3 left, Vector3 right) => left.Equals(right);
        public static bool operator !=(Vector3 left, Vector3 right) => !(left == right);

        public override string ToString()
        {
            return "Vector3[" + X + ", " + Y + ", " + Z + "]";
        }

        public override int GetHashCode() => (X, Y, Z).GetHashCode();
    }
}
