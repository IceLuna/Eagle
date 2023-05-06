using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4 : IEquatable<Vector4>
    {
        public float X;
        public float Y;
        public float Z;
        public float W;

        public Vector4(Color4 color)
        {
            X = color.R;
            Y = color.G;
            Z = color.B;
            W = color.A;
        }

        public Vector4(float scalar)
        {
            X = Y = Z = W = scalar;
        }

        public Vector4(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public static Vector4 operator +(Vector4 left, Vector4 right)
        {
            return new Vector4(left.X + right.X, left.Y + right.Y, left.Z + right.Z, left.W + right.W);
        }

        public static Vector4 operator +(Vector4 left, float scalar)
        {
            return new Vector4(left.X + scalar, left.Y + scalar, left.Z + scalar, left.W + scalar);
        }

        public static Vector4 operator +(float scalar, Vector4 right)
        {
            return new Vector4(right.X + scalar, right.Y + scalar, right.Z + scalar, right.W + scalar);
        }

        public static Vector4 operator -(Vector4 left, Vector4 right)
        {
            return new Vector4(left.X - right.X, left.Y - right.Y, left.Z - right.Z, left.W - right.W);
        }

        public static Vector4 operator -(Vector4 left, float scalar)
        {
            return new Vector4(left.X - scalar, left.Y - scalar, left.Z - scalar, left.W - scalar);
        }

        public static Vector4 operator -(float scalar, Vector4 right)
        {
            return new Vector4(scalar - right.X, scalar - right.Y, scalar - right.Z, scalar - right.W);
        }

        public static Vector4 operator -(Vector4 vector)
        {
            return new Vector4(-vector.X, -vector.Y, -vector.Z, -vector.W);
        }

        public static Vector4 operator *(Vector4 left, Vector4 right)
        {
            return new Vector4(left.X * right.X, left.Y * right.Y, left.Z * right.Z, left.W * right.W);
        }

        public static Vector4 operator *(Vector4 left, float scalar)
        {
            return new Vector4(left.X * scalar, left.Y * scalar, left.Z * scalar, left.W * scalar);
        }

        public static Vector4 operator *(float scalar, Vector4 right)
        {
            return new Vector4(right.X * scalar, right.Y * scalar, right.Z * scalar, right.W * scalar);
        }

        public static Vector4 operator /(Vector4 left, Vector4 right)
        {
            return new Vector4(left.X / right.X, left.Y / right.Y, left.Z / right.Z, left.W / right.W);
        }

        public static Vector4 operator /(Vector4 left, float scalar)
        {
            return new Vector4(left.X / scalar, left.Y / scalar, left.Z / scalar, left.W / scalar);
        }

        public static Vector4 operator /(float scalar, Vector4 right)
        {
            return new Vector4(scalar / right.X, scalar / right.Y, scalar / right.Z, scalar / right.W);
        }

        public override bool Equals(object obj) => obj is Vector4 other && this.Equals(other);

        public bool Equals(Vector4 right) => X == right.X && Y == right.Y && Z == right.Z && W == right.W;

        public static bool operator ==(Vector4 left, Vector4 right) => left.Equals(right);
        public static bool operator !=(Vector4 left, Vector4 right) => !(left == right);

        public override string ToString()
        {
            return "Vector4[" + X + ", " + Y + ", " + Z + ", " + W + "]";
        }

        public override int GetHashCode() => (X, Y, Z, W).GetHashCode();
    }
}
