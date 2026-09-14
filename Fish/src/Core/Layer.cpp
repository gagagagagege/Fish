#include "Layer.h"
#include "fspch.h"
namespace Fish {

    Layer::Layer(const std::string& debugName):m_DebugName(debugName){}

    Layer::~Layer(){}

    void Layer::OnAttach() {}

    void Layer::OnDetach() {}

    void Layer::OnUpdate(Timestep ts) {}

    void Layer::OnImGuiRender() {}

    void Layer::OnEvent(Event& event) {}
}