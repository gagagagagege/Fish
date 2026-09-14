#pragma once
//#include "Hazel/Debug/Instrumentor.h"                         GITHUB

#include<iostream>
#include <functional>
#include"Core/macro.h"

namespace Fish {
	enum class EventType {
		None = 0,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		AppTick, AppUpdate, AppRender,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	enum EventCategory {
		None = 0,
		EventCategoryApplication = BIT(0),
		EventCategoryInput = BIT(1),
		EventCategoryKeyboard = BIT(2),
		EventCategoryMouse = BIT(3),
		EventCategoryMouseButton = BIT(4)
	};

#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }\
								virtual EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

	class Event {
	public:
		bool Handled = false;           
	    Event() = default;
		virtual ~Event() = default;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual int GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }
		bool IsInCategory(EventCategory category)
		{
			return GetCategoryFlags() & category;
		}
	};
	class EventDispatcher {
	public:
		EventDispatcher(Event& event) :m_Event(event) {}

		template<typename T, typename F>
		bool Dispatch(const F& func) {
			if (m_Event.GetEventType() == T::GetStaticType()) {
				m_Event.Handled |= func(static_cast<T&>(m_Event));
				return true;
			}
			else return false;
		}
	private:
		Event& m_Event; //Dispatcher本质是临时中介，成员变量引用，让传入的数据本体能直接被操作
	};

	inline  std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}

}
