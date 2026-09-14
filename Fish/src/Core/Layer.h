#pragma once

#include"macro.h"
#include"Event/Event.h"
#include"Timestep.h"

namespace Fish {
	class /*FS_API*/ Layer {
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach();       //Stack中添加一层的操作
		virtual void OnDetach();       //
		virtual void OnUpdate(Timestep Ts);
		virtual void OnImGuiRender();
		virtual void OnEvent(Event& event);

		const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};
}