using System;

namespace Eagle
{
    // Can be used to override the name that should be displayed in UI
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class UINameAttribute : Attribute
    {
        public string Name { get; }

        public UINameAttribute(string name)
        {
            Name = name;
        }
    }

    // If specified, help marker will appear near the field in UI
    [AttributeUsage(AttributeTargets.Class | AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class TooltipAttribute : Attribute
    {
        public string Text { get; }

        public TooltipAttribute(string text)
        {
            Text = text;
        }
    }
}
