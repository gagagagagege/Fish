#pragma once

#ifdef FS_PLATFORM_WINDOWS

extern Fish::Application* Fish::CreateApplication();

int main(int argc, char** argv) {

	Fish::Log::Init();
	FS_CORE_WARN("Log");
	int a = 5;
	FS_Client_INFO("Hello Var={0}", a);

	auto app = Fish::CreateApplication();
	app->run();
	delete app;
}

#endif