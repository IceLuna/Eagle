#include "egpch.h"
#include "LayerStack.h"

namespace Eagle
{
	LayerStack::LayerStack()
	{
		
	}
	LayerStack::~LayerStack()
	{
		for (Layer* layer : m_Layers)
		{
			layer->OnDetach();
			delete layer;
		}
	}
	void LayerStack::PushLayer(Layer* layer)
	{
		m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
		++m_LayerInsertIndex;
	}
	bool LayerStack::PopLayer(Layer* layer)
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
	void LayerStack::PushLayout(Layer* layer)
	{
		m_Layers.emplace_back(layer);
	}
	bool LayerStack::PopLayout(Layer* layer)
	{
		auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), layer);

		if (it != m_Layers.end())
		{
			m_Layers.erase(it);
			return true;
		}
		return false;
	}
}