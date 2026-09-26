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

		// 显式清掉所有 layer(delete 它们)。~LayerStack 自己也会做,
		// 但那个跑在 Application 析构体之后 —— 渲染后端那时已经没了,
		// 而 layer 里握着 GPU 资源。
		void Clear();

		std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_Layers.end(); }
	private:
		std::vector<Layer*>  m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	}; 
}