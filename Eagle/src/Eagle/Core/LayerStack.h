#pragma once

#include "Layer.h"

namespace Eagle
{
	using LayerVector = std::vector<Layer*>;

	class EAGLE_API LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer* layer);
		void PopLayer(Layer* layer);
		void PushLayout(Layer* layer);
		void PopLayout(Layer* layer);

		LayerVector::iterator				begin()			{ return m_Layers.begin();  }
		LayerVector::iterator				end()			{ return m_Layers.end();    }
		LayerVector::reverse_iterator		rbegin()		{ return m_Layers.rbegin(); }
		LayerVector::reverse_iterator		rend()			{ return m_Layers.rend();   }

		LayerVector::const_iterator			begin()	  const { return m_Layers.begin();  }
		LayerVector::const_iterator			end()	  const { return m_Layers.end();	}
		LayerVector::const_reverse_iterator rbegin()  const { return m_Layers.rbegin(); }
		LayerVector::const_reverse_iterator rend()	  const { return m_Layers.rend();   }

	private:
		LayerVector m_Layers;
		uint32_t m_LayerInsertIndex = 0;
	};
}
