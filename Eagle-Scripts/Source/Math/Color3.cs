using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Color3 : IEquatable<Color3>
    {
        public float R;
        public float G;
        public float B;

        public Color3(Vector3 vec)
        {
            R = vec.X;
            G = vec.Y;
            B = vec.Z;
        }

        public Color3(float scalar)
        {
            R = G = B = scalar;
        }

        public Color3(float x, float y, float z)
        {
            R = x;
            G = y;
            B = z;
        }

        public static implicit operator Color3(Vector3 value)
        {
            return new Color3(value);
        }

        public static Color3 operator +(Color3 left, Color3 right)
        {
            return new Color3(left.R + right.R, left.G + right.G, left.B + right.B);
        }

        public static Color3 operator +(Color3 left, float scalar)
        {
            return new Color3(left.R + scalar, left.G + scalar, left.B + scalar);
        }

        public static Color3 operator +(float scalar, Color3 right)
        {
            return new Color3(right.R + scalar, right.G + scalar, right.B + scalar);
        }

        public static Color3 operator -(Color3 left, Color3 right)
        {
            return new Color3(left.R - right.R, left.G - right.G, left.B - right.B);
        }

        public static Color3 operator -(Color3 left, float scalar)
        {
            return new Color3(left.R - scalar, left.G - scalar, left.B - scalar);
        }

        public static Color3 operator -(float scalar, Color3 right)
        {
            return new Color3(scalar - right.R, scalar - right.G, scalar - right.B);
        }

        public static Color3 operator -(Color3 vector)
        {
            return new Color3(-vector.R, -vector.G, -vector.B);
        }

        public static Color3 operator *(Color3 left, Color3 right)
        {
            return new Color3(left.R * right.R, left.G * right.G, left.B * right.B);
        }

        public static Color3 operator *(Color3 left, float scalar)
        {
            return new Color3(left.R * scalar, left.G * scalar, left.B * scalar);
        }

        public static Color3 operator *(float scalar, Color3 right)
        {
            return new Color3(right.R * scalar, right.G * scalar, right.B * scalar);
        }

        public static Color3 operator /(Color3 left, Color3 right)
        {
            return new Color3(left.R / right.R, left.G / right.G, left.B / right.B);
        }

        public static Color3 operator /(Color3 left, float scalar)
        {
            return new Color3(left.R / scalar, left.G / scalar, left.B / scalar);
        }

        public static Color3 operator /(float scalar, Color3 right)
        {
            return new Color3(scalar / right.R, scalar / right.G, scalar / right.B);
        }

        public override bool Equals(object obj) => obj is Color3 other && this.Equals(other);

        public bool Equals(Color3 right) => R == right.R && G == right.G && B == right.B;

        public static bool operator ==(Color3 left, Color3 right) => left.Equals(right);
        public static bool operator !=(Color3 left, Color3 right) => !(left == right);

        public override string ToString()
        {
            return "Color3[" + R + ", " + G + ", " + B + "]";
        }

        public override int GetHashCode() => (R, G, B).GetHashCode();
    }
}
