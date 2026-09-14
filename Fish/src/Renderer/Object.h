#pragma once
#include"glm/glm.hpp"
namespace Fish {
	class Object
	{
	public:
		Object();


	private:
		glm::vec4 Position;
		glm::vec4 Texture;
		glm::mat4 ModelMatrix;
		//etc.


	};


}

