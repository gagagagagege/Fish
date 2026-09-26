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

        // OPENGL 遗留:ImGui 层原来是无条件建的,但 ImGuiLayer::OnAttach 走的是
        // ImGui_ImplGlfw_InitForOpenGL + ImGui_ImplOpenGL3_Init(ImGuiLayer.cpp:48-49),
        // 而窗口现在是 GLFW_NO_API、没有 GL context。归 M6(ImGui 接 Vulkan 后端)。
        //m_ImGuiLayer = new ImGuiLayer();
        //PushOverlay(m_ImGuiLayer);
    }


    Application::~Application() {
        // 先销毁 layer,再关渲染后端。
        //
        // layer 里握着 GPU 资源(顶点缓冲、索引缓冲、贴图、shader),它们的析构
        // 要调 vkDestroy*。而 m_LayerStack 是 Application 的成员,成员析构发生在
        // 析构函数体【之后】—— 不显式清一次,顺序就是"设备先没,资源后销毁",
        // 结果全泄漏。验证层会在 vkDestroyDevice 时报
        // VUID-vkDestroyDevice-device-05137,而正常退出时它只打一遍,很容易漏看。
        //
        // 中间那次 WaitIdle 不能省:最后一帧的命令缓冲还在飞,它引用着 layer 的
        // 顶点缓冲和贴图。不等就销毁,验证层报 VUID-vkDestroyBuffer-buffer-00922。
        Renderer::WaitIdle();
        m_LayerStack.Clear();
        Renderer::Shutdown();
    }

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
                // 先全部更新,再画一帧。layer 的 Submit 只入队,不碰命令缓冲
                // —— 命令缓冲只在 DrawFrame 里开着。
                for (Layer* layer : m_LayerStack) {
                    layer->OnUpdate(timestep);
                }
                Renderer::DrawFrame();
            }

            // OPENGL 遗留:ImGui 层暂时不建,判空跳过。归 M6。
            if (m_ImGuiLayer) {
                m_ImGuiLayer->Begin();
                for (Layer* layer : m_LayerStack)
                    layer->OnImGuiRender();
                m_ImGuiLayer->End();
            }

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
        Renderer::NotifyWindowResized();

        return false;
    }
}