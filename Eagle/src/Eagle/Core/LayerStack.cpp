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
		layer->OnAttach();
	}
	void LayerStack::PopLayer(Layer* layer)
	{
		auto start = m_Layers.begin();
		auto end = m_Layers.begin() + m_LayerInsertIndex;
		auto it = std::find(start, end, layer);

		if (it != end)
		{
			layer->OnDetach();
			m_Layers.erase(it);
			--m_LayerInsertIndex;
		}
	}
	void LayerStack::PushLayout(Layer* layer)
	{
		m_Layers.emplace_back(layer);
		layer->OnAttach();
	}
	void LayerStack::PopLayout(Layer* layer)
	{
		auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), layer);

		if (it != m_Layers.end())
		{
			layer->OnDetach();
			m_Layers.erase(it);
		}
	}
}