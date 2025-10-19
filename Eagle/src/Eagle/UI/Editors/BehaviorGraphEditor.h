#pragma once

#include "GraphEditor.h"

namespace Eagle
{
    class BehaviorGraphEditor : public GraphEditor
    {
    public:
        BehaviorGraphEditor(const Ref<AssetBehaviorGraph>& asset, const std::string& name = "Behavior Graph Editor");

        // We'll draw it manually in `UIBehaviorGraph`
        void RenderLeftPanel() override {}

        GraphEditorSerializationData Save() override;

        void OnAddGraphPre() override;
        void OnAddGraphPost() override;

        const Ref<AssetBehaviorGraph>& GetBehaviorGraphAsset() const { return m_Asset; }

    private:
        void Deserialize();

        bool IgnoreChangedEvent() const { return m_bIgnoreChangedEvent; }

    private:
        Ref<AssetBehaviorGraph> m_Asset;
        Ref<class UIBehaviorGraph> m_Graph;
        bool m_GraphIsDirty = false;
    };
}
