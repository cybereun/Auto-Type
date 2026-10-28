cmake_minimum_required(VERSION 3.16)
project(AutoType LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT for every target, so the IME DLL has no runtime dependency and the test links against the same CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(autotype_core STATIC src/core/Engine.cpp)
target_include_directories(autotype_core PUBLIC src/core)

add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE autotype_core)

if(WIN32)
  add_library(AutoType SHARED
    src/tsf/Dll.cpp
    src/tsf/TextService.cpp
    src/tsf/AutoType.rc
    src/tsf/AutoType.def)
  set_target_properties(AutoType PROPERTIES PREFIX "")
  target_link_libraries(AutoType PRIVATE autotype_core ole32 uuid advapi32 user32)
  target_compile_definitions(AutoType PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(AutoType PRIVATE /utf-8)
    target_compile_options(autotype_core PRIVATE /utf-8)
    target_compile_options(engine_test PRIVATE /utf-8)
  else()
    target_link_options(AutoType PRIVATE -static)
  endif()
endif()
