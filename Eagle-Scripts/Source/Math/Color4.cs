using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Color4 : IEquatable<Color4>
    {
        public float R;
        public float G;
        public float B;
        public float A;

        public Color4(Vector4 vec)
        {
            R = vec.X;
            G = vec.Y;
            B = vec.Z;
            A = vec.W;
        }

        public Color4(float scalar)
        {
            R = G = B = A = scalar;
        }

        public Color4(float x, float y, float z, float w)
        {
            R = x;
            G = y;
            B = z;
            A = w;
        }

        public static Color4 operator +(Color4 left, Color4 right)
        {
            return new Color4(left.R + right.R, left.G + right.G, left.B + right.B, left.A + right.A);
        }

        public static Color4 operator +(Color4 left, float scalar)
        {
            return new Color4(left.R + scalar, left.G + scalar, left.B + scalar, left.A + scalar);
        }

        public static Color4 operator +(float scalar, Color4 right)
        {
            return new Color4(right.R + scalar, right.G + scalar, right.B + scalar, right.A + scalar);
        }

        public static Color4 operator -(Color4 left, Color4 right)
        {
            return new Color4(left.R - right.R, left.G - right.G, left.B - right.B, left.A - right.A);
        }

        public static Color4 operator -(Color4 left, float scalar)
        {
            return new Color4(left.R - scalar, left.G - scalar, left.B - scalar, left.A - scalar);
        }

        public static Color4 operator -(float scalar, Color4 right)
        {
            return new Color4(scalar - right.R, scalar - right.G, scalar - right.B, scalar - right.A);
        }

        public static Color4 operator -(Color4 vector)
        {
            return new Color4(-vector.R, -vector.G, -vector.B, -vector.A);
        }

        public static Color4 operator *(Color4 left, Color4 right)
        {
            return new Color4(left.R * right.R, left.G * right.G, left.B * right.B, left.A * right.A);
        }

        public static Color4 operator *(Color4 left, float scalar)
        {
            return new Color4(left.R * scalar, left.G * scalar, left.B * scalar, left.A * scalar);
        }

        public static Color4 operator *(float scalar, Color4 right)
        {
            return new Color4(right.R * scalar, right.G * scalar, right.B * scalar, right.A * scalar);
        }

        public static Color4 operator /(Color4 left, Color4 right)
        {
            return new Color4(left.R / right.R, left.G / right.G, left.B / right.B, left.A / right.A);
        }

        public static Color4 operator /(Color4 left, float scalar)
        {
            return new Color4(left.R / scalar, left.G / scalar, left.B / scalar, left.A / scalar);
        }

        public static Color4 operator /(float scalar, Color4 right)
        {
            return new Color4(scalar / right.R, scalar / right.G, scalar / right.B, scalar / right.A);
        }

        public override bool Equals(object obj) => obj is Color4 other && this.Equals(other);

        public bool Equals(Color4 right) => R == right.R && G == right.G && B == right.B && A == right.A;

        public static bool operator ==(Color4 left, Color4 right) => left.Equals(right);
        public static bool operator !=(Color4 left, Color4 right) => !(left == right);

        public override string ToString()
        {
            return "Color4[" + R + ", " + G + ", " + B + ", " + A + "]";
        }

        public override int GetHashCode() => (R, G, B, A).GetHashCode();
    }
}
