#include "Layer3D.h"
#include "Core/EntryPoint.h"

class Playground : public Fish::Application
{
public:
	Playground()
	{
		PushLayer(new Layer3D());
	}
	~Playground() = default;
};

Fish::Application* Fish::CreateApplication()
{
	return new Playground;
}
