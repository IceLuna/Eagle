#pragma once

#include "Layer.h"

namespace Eagle
{
	using LayerVector = std::vector<Ref<Layer>>;

	class LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(const Ref<Layer>& layer);
		bool PopLayer(const Ref<Layer>& layer);
		void PushLayout(const Ref<Layer>& layer);
		bool PopLayout(const Ref<Layer>& layer);

		LayerVector::iterator				begin()			{ return m_Layers.begin();  }
		LayerVector::iterator				end()			{ return m_Layers.end();    }
		LayerVector::reverse_iterator		rbegin()		{ return m_Layers.rbegin(); }
		LayerVector::reverse_iterator		rend()			{ return m_Layers.rend();   }

		LayerVector::const_iterator			begin()	  const { return m_Layers.begin();  }
		LayerVector::const_iterator			end()	  const { return m_Layers.end();	}
		LayerVector::const_reverse_iterator rbegin()  const { return m_Layers.rbegin(); }
		LayerVector::const_reverse_iterator rend()	  const { return m_Layers.rend();   }

		void clear() 
		{
			for(auto& layer: m_Layers)
				layer->OnDetach();
			m_Layers.clear(); 
			m_LayerInsertIndex = 0; 
		}

	private:
		LayerVector m_Layers;
		uint32_t m_LayerInsertIndex = 0;
	};
}
