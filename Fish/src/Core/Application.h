#pragma once
#include "macro.h"
#include"Window.h"
#include"../Event/ApplicationEvent.h"
#include"LayerStack.h"
#include"ImGui/ImGuiLayer.h"
#include"Timestep.h"

namespace Fish {
	class /*FS_API*/ Application {
	public:
		Application();
		virtual ~Application();
		
		void run();
		
		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		Window& GetWindow() { return *m_Window; }
		static Application& Get() { return *Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		bool OnWindowResize(WindowResizeEvent& e);

	private:
		bool m_Running = true;
		bool m_minimized = false;
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		static Application* Instance;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;
	};
	Application* CreateApplication();//undefined
}