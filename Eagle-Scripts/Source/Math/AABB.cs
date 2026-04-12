using System;
using System.Runtime.InteropServices;

namespace Eagle
{
    [StructLayout(LayoutKind.Sequential)]
    public struct AABB : IEquatable<AABB>
    {
        public Vector3 Min;
        public Vector3 Max;

        public AABB(Vector3 min, Vector3 max)
        {
            Min = min;
            Max = max;
        }

        public static AABB Default()
        {
            AABB result = new AABB();
            result.Min = new Vector3(float.MinValue);
            result.Max = new Vector3(float.MaxValue);
            return result;
        }

        public Vector3 Center() { return (Min + Max) * 0.5f; }
		public Vector3 Size() { return Max - Min; }
		public Vector3 Extents() { return Size() * 0.5f; }
		public float Length() { return Mathf.Length(Extents()); }

		public void Grow(AABB other)
		{
			Min = Mathf.Min(Min, other.Min);
			Max = Mathf.Max(Max, other.Max);
		}

		public void Grow(Vector3 p)
		{
			Min = Mathf.Min(Min, p);
			Max = Mathf.Max(Max, p);
		}

		public bool Contains(Vector3 p)
		{
			return
                (p.X >= Min.X && p.X <= Max.X) &&
                (p.Y >= Min.Y && p.Y <= Max.Y) &&
                (p.Z >= Min.Z && p.Z <= Max.Z);
		}

		public float MinSide()
		{
			Vector3 extents = Size();
			return Mathf.Min(extents.X, Mathf.Min(extents.Y, extents.Z));
		}

		public float MaxSide()
		{
			Vector3 extents = Size();
			return Mathf.Max(extents.X, Mathf.Max(extents.Y, extents.Z));
		}

        public static bool Overlap(AABB a, AABB b)
        {
            return (a.Min.X <= b.Max.X && a.Max.X >= b.Min.X) &&
                (a.Min.Y <= b.Max.Y && a.Max.Y >= b.Min.Y) &&
                (a.Min.Z <= b.Max.Z && a.Max.Z >= b.Min.Z);
        }

        public override bool Equals(object obj) => obj is AABB other && this.Equals(other);

        public bool Equals(AABB right) => Min == right.Min && Max == right.Max;

        public static bool operator ==(AABB left, AABB right) => left.Equals(right);
        public static bool operator !=(AABB left, AABB right) => !(left == right);

        public override string ToString()
        {
            return "AABB[" + Min + ", " + Max + "]";
        }

        public override int GetHashCode() => (Min, Max).GetHashCode();
    }
}
