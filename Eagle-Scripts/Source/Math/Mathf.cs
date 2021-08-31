
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

        public static Vector3 Clamp(in Vector3 value, float min, float max)
        {
            Vector3 result = value;
            result.X = Clamp(result.X, min, max);
            result.Y = Clamp(result.Y, min, max);
            result.Z = Clamp(result.Z, min, max);
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
    }
}
