#include "fspch.h"
#include "Application.h"
#include "Input.h"
#include"Renderer/Renderer.h"

namespace Fish {

#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

    Application* Application::Instance = nullptr;

    Application::Application(){

        FS_CORE_ASSERT(!Instance, "app already exists")
            Instance = this;
        m_Window = std::unique_ptr<Window>(Window::Create());
        Input::Init(static_cast<GLFWwindow*>(m_Window->GetNativeWindow()));

        Renderer::Init(m_Window->GetNativeWindow());

        //SetEventCallback把EventCallback设置成了OnEvent函数，每当执行到EventCallback，都会直接跳转到下面OnEvent的定义，所以window还是不知道application
        //因为没有发生替换，只是跳转
        //回调函数的意义就是在合适时机调用一个已经保存好的函数
        m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));

        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);
    }


    Application::~Application() {}

    void Application::PushLayer(Layer* layer) {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Layer* layer) {
        m_LayerStack.PushOverlay(layer);
        layer->OnAttach();
    }



    //不能把这个函数写成window的成员函数
    //OnEvent决定事件怎么处理，和具体的app强相关，如果它是 Window 成员函数，那么每个使用窗口的程序都得遵循同一种事件处理方式，扩展性会变差。
    // Application应当是处理事情的角色，而window只负责产生事件
    //窗口层依赖应用层
    //后续的OnEvent可以替换为别的处理事件的方式
    void Application::OnEvent(Event& e) {//对接收的事件做处理的函数        这个函数就是可以匹配EventCallbackFn的函数
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Application::OnWindowResize));
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
            (*--it)->OnEvent(e);
            if (e.Handled)
                break;
        }
    }

    void Application::run() {
        while (m_Running) {
            float time = (float)glfwGetTime();
            Timestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;

            if (!m_minimized) {
                for (Layer* layer : m_LayerStack) {
                    layer->OnUpdate(timestep);
                }
            }

            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();

            m_Window->OnUpdate();

        }
    }
    bool Application::OnWindowClose(WindowCloseEvent& e)
    {
        m_Running = false;
        return true;

    }
    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        if (e.GetWidth() == 0 || e.GetHeight() == 0) {
            m_minimized = true;
        }
        m_minimized = false;
        Renderer::onWindowResize(e.GetWidth(), e.GetHeight());

        return false;
    }
}