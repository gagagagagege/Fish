#pragma once

#include"includings.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class playground2D : public Fish::Layer {
public:
	playground2D();
	virtual ~playground2D() = default;

	void OnEvent(Fish::Event& e) override;
	void OnUpdate(Fish::Timestep ts) override;

	virtual void OnImGuiRender() override;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

private:
	Fish::Ref<Fish::Shader> m_Shader = nullptr;
	Fish::Ref<Fish::VertexBuffer> m_SquareVB = nullptr;
	Fish::Ref<Fish::IndexBuffer>  m_SquareIB = nullptr;
	Fish::OrthographicCameraController m_CameraController;
	glm::vec4 m_SquareColor = { 0.2f, 0.3f, 0.8f, 1.0f };
};
