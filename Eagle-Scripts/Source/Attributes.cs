using System;

namespace Eagle
{
    // Can be used to override the name that should be displayed in UI
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class UINameAttribute : Attribute
    {
        public string Name { get; }

        public UINameAttribute(string name)
        {
            Name = name;
        }
    }

    // If specified, help marker will appear near the field in UI
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
    public class ToolTipAttribute : Attribute
    {
        public string Text { get; }

        public ToolTipAttribute(string text)
        {
            Text = text;
        }
    }
}
