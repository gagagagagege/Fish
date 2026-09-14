#include"fspch.h"
#include "Log.h"
namespace Fish {
	std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
	std::shared_ptr<spdlog::logger> Log::s_ClientLogger;
	void Log::Init(){
		spdlog::set_pattern("%^[%T] %n:%v%$");
		s_CoreLogger = spdlog::stdout_color_mt("Fish");
		s_CoreLogger->set_level(spdlog::level::trace);
		
		s_ClientLogger = spdlog::stdout_color_mt("Playground");
		s_ClientLogger->set_level(spdlog::level::trace);
	}
	Log::Log() {

	}
	Log::~Log() {

	}
}