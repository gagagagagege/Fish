#pragma once
#include<string>

namespace Fish {
	class Shader {
	public:
		virtual ~Shader() = default;

		virtual const std::string& GetName() const = 0;

		// 空实现而不是纯虚:Vulkan 里没有"绑定 shader"这个状态(管线在录制时才 bind),
		// 新子类不该为了满足接口去写空重写。等 Platform/OpenGL/ 整批下线时连这两个一起删。
		virtual void Bind()const {}
		virtual void UnBind()const {}
	};
}
