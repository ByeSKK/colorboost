cmake_minimum_required(VERSION 3.20)
project(ColorBoost VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "ColorBoost must be built on Windows with MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT: the exe runs without installing the VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ---- nlohmann/json ----------------------------------------------------------
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)

# ---- Dear ImGui -------------------------------------------------------------
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.91.5
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(nlohmann_json imgui)

# ImGui has no CMakeLists, so build it as a static library ourselves.
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(imgui PRIVATE /utf-8 /MP)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler dwmapi)

# ---- Application ------------------------------------------------------------
add_executable(ColorBoost WIN32
  src/main.cpp
  src/App.cpp
  src/AppUI.cpp
  src/DesktopCapture.cpp
  src/Renderer.cpp
  src/Monitors.cpp
  src/Settings.cpp
  src/Utils.cpp
  src/App.h
  src/DesktopCapture.h
  src/Renderer.h
  src/Monitors.h
  src/Settings.h
  src/Localization.h
  src/Utils.h
)

target_include_directories(ColorBoost PRIVATE src)

target_compile_definitions(ColorBoost PRIVATE
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  _WIN32_WINNT=0x0A00 WINVER=0x0A00
)

target_compile_options(ColorBoost PRIVATE /utf-8 /W4 /permissive- /MP /Zc:__cplusplus)

target_link_libraries(ColorBoost PRIVATE
  imgui
  nlohmann_json::nlohmann_json
  d3d11 dxgi d3dcompiler dwmapi
  shcore shell32 advapi32 user32 gdi32 ole32 uuid
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ColorBoost)