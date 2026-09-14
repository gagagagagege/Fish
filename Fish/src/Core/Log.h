#pragma once
#include "macro.h"
#include <spdlog\spdlog.h>
#include <spdlog\fmt\ostr.h>
#include <spdlog\sinks\stdout_color_sinks.h>
namespace Fish {


	class /*FS_API*/ Log {
	public:
		Log();
		~Log();
		static void Init();
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};

}
#define FS_CORE_TRACE(...)       ::Fish::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define FS_CORE_INFO(...)        ::Fish::Log::GetCoreLogger()->info(__VA_ARGS__)
#define FS_CORE_WARN(...)        ::Fish::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define FS_CORE_ERROR(...)       ::Fish::Log::GetCoreLogger()->error(__VA_ARGS__)
#define FS_CORE_CRITICAL(...)    ::Fish::Log::GetCoreLogger()->critical(__VA_ARGS__)


#define FS_Client_TRACE(...)     ::Fish::Log::GetClientLogger()->trace(__VA_ARGS__)
#define FS_Client_INFO(...)      ::Fish::Log::GetClientLogger()->info(__VA_ARGS__)
#define FS_Client_WARN(...)      ::Fish::Log::GetClientLogger()->warn(__VA_ARGS__)
#define FS_Client_ERROR(...)     ::Fish::Log::GetClientLogger()->error(__VA_ARGS__)
#define FS_Client_CRITICAL(...)  ::Fish::Log::GetClientLogger()->critical(__VA_ARGS__)