
using System;

namespace Eagle
{
    public static class Mathf
    {
        public static float Clamp(float value, float min, float max)
        {
            if (value < min)
                return min;
            else if (value > max)
                return max;
            else
                return value;
        }

        public static Vector2 Clamp(in Vector2 value, float min, float max)
        {
            Vector2 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            return result;
        }

        public static Vector2 Clamp(in Vector2 value, in Vector2 min, in Vector2 max)
        {
            Vector2 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            return result;
        }

        public static Vector3 Clamp(in Vector3 value, float min, float max)
        {
            Vector3 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            result.Z = Clamp(result.Z, min, max);
            return result;
        }

        public static Vector3 Clamp(in Vector3 value, in Vector3 min, in Vector3 max)
        {
            Vector3 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            result.Z = Clamp(result.Z, min.Z, max.Z);
            return result;
        }

        public static Vector4 Clamp(in Vector4 value, float min, float max)
        {
            Vector4 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            result.Z = Clamp(result.Z, min, max);
            result.W = Clamp(result.W, min, max);
            return result;
        }

        public static Vector4 Clamp(in Vector4 value, in Vector4 min, in Vector4 max)
        {
            Vector4 result = value;
            result.X = Clamp(result.X, min.X, max.X);
            result.Y = Clamp(result.Y, min.Y, max.Y);
            result.Z = Clamp(result.Z, min.Z, max.Z);
            result.W = Clamp(result.W, min.W, max.W);
            return result;
        }

        public static float Radians(float angle)
        {
            return ((float)Math.PI / 180f) * angle;
        }

	    public static Quat AngleAxis(float angle, Vector3 v)
	    {
		    float s = (float)Math.Sin(angle * 0.5f);

		    return new Quat((float)Math.Cos(angle * 0.5f), v * s);
	    }

        public static Vector3 Reflect(Vector3 v, Vector3 normal)
        {
            return v - 2f * Dot(v, normal) * normal;
        }

        public static float Length(Vector2 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static float Length(Vector3 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static float Length(Vector4 v)
        {
            return (float)Math.Sqrt(Dot(v, v));
        }

        public static Vector2 Normalize(Vector2 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static Vector3 Normalize(Vector3 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static Vector4 Normalize(Vector4 v)
        {
            float len = Length(v);
            return v / len;
        }

        public static float Dot(Vector2 lhs, Vector2 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y);
        public static float Dot(Vector3 lhs, Vector3 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z);
        public static float Dot(Vector4 lhs, Vector4 rhs) => (lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z + lhs.W * rhs.W);

        public static Vector2 Sign(Vector2 v) => new Vector2(Math.Sign(v.X), Math.Sign(v.Y));
        public static Vector3 Sign(Vector3 v) => new Vector3(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z));
        public static Vector4 Sign(Vector4 v) => new Vector4(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z), Math.Sign(v.W));
    }
}
