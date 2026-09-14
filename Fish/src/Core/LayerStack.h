#pragma once

#include"Layer.h"
#include"macro.h"

#include<vector>

namespace Fish {

	class /*FS_API*/ LayerStack {
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverLay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }
	private:
		std::vector<Layer*>  m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	}; 
}