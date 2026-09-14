#pragma once
#include<memory>
// 静态库不需要 dllexport/dllimport，以下代码注释保留以便将来切回动态库
// #ifdef FS_PLATFORM_WINDOWS
//   #ifdef FS_BUILD_DLL
//     #define FS_API __declspec(dllexport)
//   #else
//     #define FS_API __declspec(dllimport)
//   #endif
// #endif

#ifdef FS_DEBUG
  #define FS_ENABLE_ASSERTS
#endif

#ifdef FS_ENABLE_ASSERTS
  #define FS_ASSERT(x, ...) { if(!(x)) { FS_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
  #define FS_CORE_ASSERT(x, ...) { if(!(x)) { FS_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
  #define FS_ASSERT(x, ...) 
  #define FS_CORE_ASSERT(x, ...) 
#endif


#define BIT(x) (1 << x)
#define FS_BIND_EVENT_FN(fn) std::bind(&fn,this,std::placeholders::_1) 

namespace Fish {
	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T>
	using Scope = std::unique_ptr<T>;

}