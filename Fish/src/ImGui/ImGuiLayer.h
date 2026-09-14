#pragma once
#include"Core/Layer.h"
#include"Event/MouseEvent.h"
#include"Event/KeyEvent.h"
#include"Event/ApplicationEvent.h"

namespace Fish {
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnImGuiRender() override;

		void Begin();
		void End();
	private:
		float m_Time = 0.0f;
	};

}