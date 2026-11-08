// Types that newer C# features need, but that .NET Framework 4.7.2 (the scripts' target) doesn't have.
namespace System.Runtime.CompilerServices
{
	public static class IsExternalInit
	{
	}

	[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Field | AttributeTargets.Property, AllowMultiple = false, Inherited = false)]
	public sealed class RequiredMemberAttribute : Attribute
	{
	}

	[AttributeUsage(AttributeTargets.All, AllowMultiple = true, Inherited = false)]
	public sealed class CompilerFeatureRequiredAttribute : Attribute
	{
		public const string RefStructs = nameof(RefStructs);
		public const string RequiredMembers = nameof(RequiredMembers);

		public CompilerFeatureRequiredAttribute(string featureName)
		{
			FeatureName = featureName;
		}

		public string FeatureName { get; }

		public bool IsOptional { get; set; }
	}
}

namespace System.Diagnostics.CodeAnalysis
{
	// Marks a constructor that sets all required members, so that callers don't have to
	[AttributeUsage(AttributeTargets.Constructor, AllowMultiple = false, Inherited = false)]
	public sealed class SetsRequiredMembersAttribute : Attribute
	{
	}
}
