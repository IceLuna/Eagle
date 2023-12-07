
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

        public static float Lerp(float x, float y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector2 Lerp(Vector2 x, Vector2 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector3 Lerp(Vector3 x, Vector3 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static Vector4 Lerp(Vector4 x, Vector4 y, float alpha)
        {
            return x * (1f - alpha) + y * alpha;
        }

        public static float Step(float edge, float x) => x < edge ? 0.0f : 1.0f;

        public static Vector2 Step(Vector2 edge, Vector2 x) => new Vector2(Step(edge.X, x.X), Step(edge.Y, x.Y));

        public static Vector3 Step(Vector3 edge, Vector3 x) => new Vector3(Step(edge.X, x.X), Step(edge.Y, x.Y), Step(edge.Z, x.Z));

        public static Vector4 Step(Vector4 edge, Vector4 x) => new Vector4(Step(edge.X, x.X), Step(edge.Y, x.Y), Step(edge.Z, x.Z), Step(edge.W, x.W));

        public static float Floor(float x) => (float)Math.Floor(x);

        public static Vector2 Floor(Vector2 x) => new Vector2((float)Math.Floor(x.X), (float)Math.Floor(x.Y));

        public static Vector3 Floor(Vector3 x) => new Vector3((float)Math.Floor(x.X), (float)Math.Floor(x.Y), (float)Math.Floor(x.Z));

        public static Vector4 Floor(Vector4 x) => new Vector4((float)Math.Floor(x.X), (float)Math.Floor(x.Y), (float)Math.Floor(x.Z), (float)Math.Floor(x.W));

        public static float Abs(float x) => (float)Math.Abs(x);

        public static Vector2 Abs(Vector2 x) => new Vector2((float)Math.Abs(x.X), (float)Math.Abs(x.Y));

        public static Vector3 Abs(Vector3 x) => new Vector3((float)Math.Abs(x.X), (float)Math.Abs(x.Y), (float)Math.Abs(x.Z));

        public static Vector4 Abs(Vector4 x) => new Vector4((float)Math.Abs(x.X), (float)Math.Abs(x.Y), (float)Math.Abs(x.Z), (float)Math.Abs(x.W));

        public static float Fract(float x) => x - Floor(x);

        public static Vector2 Fract(Vector2 x) => x - Floor(x);

        public static Vector3 Fract(Vector3 x) => x - Floor(x);

        public static Vector4 Fract(Vector4 x) => x - Floor(x);

        // All components are in the range [0…1], including hue.
        public static Color3 RGB2HSV(Color3 c)
        {
            Vector4 K = new Vector4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
            Vector4 p = Lerp(new Vector4(c.B, c.G, K.W, K.Z), new Vector4(c.G, c.B, K.X, K.Y), Step(c.B, c.G));
            Vector4 q = Lerp(new Vector4(p.X, p.Y, p.W, c.R), new Vector4(c.R, p.Y, p.Z, p.X), Step(p.X, c.R));

            float d = q.X - Math.Min(q.W, q.Y);
            float e = 1.0e-10f;
            return new Vector3(Math.Abs(q.Z + (q.W - q.Y) / (6.0f * d + e)), d / (q.X + e), q.X);
        }

        // All components are in the range [0…1], including hue.
        public static Color3 HSV2RGB(Color3 c)
        {
            Vector3 K = new Vector3(1.0f, 2.0f / 3.0f, 1.0f / 3.0f);
            float Kw = 3f;
            Vector3 Kx = new Vector3(K.X);

            Vector3 fract = Fract(new Vector3(c.R) + K);
            Vector3 p = Mathf.Abs((fract * 6.0f - new Vector3(Kw)));
            return c.B * Lerp(Kx, Clamp(p - Kx, 0.0f, 1.0f), c.G);
        }

        // Input values are RGB colors. They're converted into HSV, lerped, and converted back to RGB
        public static Color3 ColorLerp(Color3 x, Color3 y, float alpha)
        {
            x = RGB2HSV(x);
            y = RGB2HSV(y);
            Vector3 result = (y - x) * alpha + x;
            return HSV2RGB(result);
        }

        // Maps `value` from range [minA; maxA] to range [minB; maxB]
        public static float MapRange(float value, float minA, float maxA, float minB, float maxB)
        {
            float alpha = (value - minA) / (maxA - minA);
            float result = alpha * (maxB - minB) + minB;
            return result;
        }

        public static float Sign(float v) => Math.Sign(v);
        public static Vector2 Sign(Vector2 v) => new Vector2(Math.Sign(v.X), Math.Sign(v.Y));
        public static Vector3 Sign(Vector3 v) => new Vector3(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z));
        public static Vector4 Sign(Vector4 v) => new Vector4(Math.Sign(v.X), Math.Sign(v.Y), Math.Sign(v.Z), Math.Sign(v.W));
    }
}
