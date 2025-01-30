using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2 : IEquatable<Vector2>
    {
        public float X;
        public float Y;

        public Vector2(Vector2 v)
        {
            X = v.X;
            Y = v.Y;
        }

        public Vector2(float scalar)
        {
            X = Y = scalar;
        }

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public static Vector2 operator +(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X + right.X, left.Y + right.Y);
        }

        public static Vector2 operator +(Vector2 left, float scalar)
        {
            return new Vector2(left.X + scalar, left.Y + scalar);
        }

        public static Vector2 operator +(float scalar, Vector2 right)
        {
            return new Vector2(right.X + scalar, right.Y + scalar);
        }

        public static Vector2 operator -(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X - right.X, left.Y - right.Y);
        }

        public static Vector2 operator -(Vector2 left, float scalar)
        {
            return new Vector2(left.X - scalar, left.Y - scalar);
        }

        public static Vector2 operator -(float scalar, Vector2 right)
        {
            return new Vector2(scalar - right.X, scalar - right.Y);
        }

        public static Vector2 operator -(Vector2 vector)
        {
            return new Vector2(-vector.X, -vector.Y);
        }

        public static Vector2 operator *(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X * right.X, left.Y * right.Y);
        }

        public static Vector2 operator *(Vector2 left, float scalar)
        {
            return new Vector2(left.X * scalar, left.Y * scalar);
        }

        public static Vector2 operator *(float scalar, Vector2 right)
        {
            return new Vector2(right.X * scalar, right.Y * scalar);
        }

        public static Vector2 operator /(Vector2 left, Vector2 right)
        {
            return new Vector2(left.X / right.X, left.Y / right.Y);
        }

        public static Vector2 operator /(Vector2 left, float scalar)
        {
            return new Vector2(left.X / scalar, left.Y / scalar);
        }

        public static Vector2 operator /(float scalar, Vector2 right)
        {
            return new Vector2(scalar / right.X, scalar / right.Y);
        }

        public override bool Equals(object obj) => obj is Vector2 other && this.Equals(other);

        public bool Equals(Vector2 right) => X == right.X && Y == right.Y;

        public static bool operator ==(Vector2 left, Vector2 right) => left.Equals(right);
        public static bool operator !=(Vector2 left, Vector2 right) => !(left == right);

        public override string ToString()
        {
            return "Vector2[" + X + ", " + Y + "]";
        }

        public override int GetHashCode() => (X, Y).GetHashCode();
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct UVector2 : IEquatable<UVector2>
    {
        public uint X;
        public uint Y;

        public UVector2(UVector2 v)
        {
            X = v.X;
            Y = v.Y;
        }

        public UVector2(uint scalar)
        {
            X = Y = scalar;
        }

        public UVector2(uint x, uint y)
        {
            X = x;
            Y = y;
        }

        public static UVector2 operator +(UVector2 left, UVector2 right)
        {
            return new UVector2(left.X + right.X, left.Y + right.Y);
        }

        public static UVector2 operator +(UVector2 left, uint scalar)
        {
            return new UVector2(left.X + scalar, left.Y + scalar);
        }

        public static UVector2 operator +(uint scalar, UVector2 right)
        {
            return new UVector2(right.X + scalar, right.Y + scalar);
        }

        public static UVector2 operator -(UVector2 left, UVector2 right)
        {
            return new UVector2(left.X - right.X, left.Y - right.Y);
        }

        public static UVector2 operator -(UVector2 left, uint scalar)
        {
            return new UVector2(left.X - scalar, left.Y - scalar);
        }

        public static UVector2 operator -(uint  scalar, UVector2 right)
        {
            return new UVector2(scalar - right.X, scalar - right.Y);
        }

        public static UVector2 operator *(UVector2 left, UVector2 right)
        {
            return new UVector2(left.X * right.X, left.Y * right.Y);
        }

        public static UVector2 operator *(UVector2 left, uint scalar)
        {
            return new UVector2(left.X * scalar, left.Y * scalar);
        }

        public static UVector2 operator *(uint scalar, UVector2 right)
        {
            return new UVector2(right.X * scalar, right.Y * scalar);
        }

        public static UVector2 operator /(UVector2 left, UVector2 right)
        {
            return new UVector2(left.X / right.X, left.Y / right.Y);
        }

        public static UVector2 operator /(UVector2 left, uint scalar)
        {
            return new UVector2(left.X / scalar, left.Y / scalar);
        }

        public static UVector2 operator /(uint scalar, UVector2 right)
        {
            return new UVector2(scalar / right.X, scalar / right.Y);
        }

        public override bool Equals(object obj) => obj is UVector2 other && this.Equals(other);

        public bool Equals(UVector2 right) => X == right.X && Y == right.Y;

        public static bool operator ==(UVector2 left, UVector2 right) => left.Equals(right);
        public static bool operator !=(UVector2 left, UVector2 right) => !(left == right);

        public override string ToString()
        {
            return "UVector2[" + X + ", " + Y + "]";
        }

        public override int GetHashCode() => (X, Y).GetHashCode();
    }
}
