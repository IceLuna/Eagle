#include "egpch.h"
#include "LayerStack.h"

namespace Eagle
{
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
		//layer->OnAttach();??
		m_Layers.insert(m_Layers.begin() + m_LayerInsertIndex, layer); //Insert -> Emplace
		++m_LayerInsertIndex;
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
		//layer->OnAttach();??
		m_Layers.emplace_back(layer);
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