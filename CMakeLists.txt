cmake_minimum_required(VERSION 3.16)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _version)
string(STRIP "${_version}" _version)
set(AUTOTYPE_VERSION "${_version}" CACHE STRING "Version shown in the DLL and installer")
project(AutoType VERSION ${AUTOTYPE_VERSION} LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT for every target, so the IME DLL has no runtime dependency and the test links against the same CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(autotype_core STATIC src/core/Engine.cpp)
target_include_directories(autotype_core PUBLIC src/core)

add_executable(engine_test tests/engine_test.cpp)
target_link_libraries(engine_test PRIVATE autotype_core)
add_executable(pending_word_test tests/pending_word_test.cpp)
target_link_libraries(pending_word_test PRIVATE autotype_core)

if(WIN32)
  configure_file(src/overlay/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/Version.h @ONLY)
  configure_file(src/overlay/Model.rc.in ${CMAKE_CURRENT_BINARY_DIR}/Model.rc @ONLY)
  add_executable(AutoTypePreview WIN32 src/overlay/Main.cpp ${CMAKE_CURRENT_BINARY_DIR}/Model.rc)
  target_include_directories(AutoTypePreview PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(AutoTypePreview PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
  target_link_libraries(AutoTypePreview PRIVATE autotype_core user32 gdi32 shell32 ole32 oleaut32 uiautomationcore)
  add_executable(win32_input_host WIN32 tests/win32_input_host.cpp)
  target_compile_definitions(win32_input_host PRIVATE UNICODE _UNICODE)
  target_link_libraries(win32_input_host PRIVATE ole32 uuid user32 gdi32)
  configure_file(src/tsf/AutoType.rc.in ${CMAKE_CURRENT_BINARY_DIR}/AutoType.rc @ONLY)
  add_library(AutoType SHARED
    src/tsf/Dll.cpp
    src/tsf/TextService.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/AutoType.rc
    src/tsf/AutoType.def)
  set_target_properties(AutoType PROPERTIES PREFIX "")
  target_link_libraries(AutoType PRIVATE autotype_core ole32 uuid advapi32 user32)
  target_compile_definitions(AutoType PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
  if(MSVC)
    target_compile_options(AutoTypePreview PRIVATE /utf-8)
    target_compile_options(AutoType PRIVATE /utf-8)
    target_compile_options(autotype_core PRIVATE /utf-8)
    target_compile_options(engine_test PRIVATE /utf-8)
    target_compile_options(pending_word_test PRIVATE /utf-8)
  else()
    target_link_options(AutoType PRIVATE -static)
  endif()
endif()
