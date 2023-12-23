#include "egpch.h"
#include "LayerStack.h"

namespace Eagle
{
	LayerStack::~LayerStack()
	{
		for (auto& layer : m_Layers)
			layer->OnDetach();
	}

	void LayerStack::PushLayer(const Ref<Layer>& layer)
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		++m_LayerInsertIndex;
	}

	bool LayerStack::PopLayer(const Ref<Layer>& layer)
	{
		auto start = m_Layers.begin();
		auto end = m_Layers.begin() + m_LayerInsertIndex;
		auto it = std::find(start, end, layer);

		if (it != end)
		{
			m_Layers.erase(it);
			--m_LayerInsertIndex;
			return true;
		}
		return false;
	}
}