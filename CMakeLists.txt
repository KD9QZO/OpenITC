cmake_minimum_required(VERSION 3.20)
project(OpenITC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTLSDR REQUIRED IMPORTED_TARGET librtlsdr)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
find_package(OpenGL REQUIRED)

# The legacy `openitc decode` path drives the Windows-only reference s2p.exe
# over UDP + ZeroMQ, so ZeroMQ and Winsock are only needed on Windows.
if(WIN32)
  pkg_check_modules(LIBZMQ REQUIRED IMPORTED_TARGET libzmq)
endif()

# Map tiles are fetched with WinHTTP on Windows and libcurl elsewhere. Without
# libcurl the GUI still builds; the map just uses cached tiles only.
if(NOT WIN32)
  find_package(CURL QUIET)
  if(NOT CURL_FOUND)
    message(WARNING "libcurl not found: openitc-gui will build without online map tiles")
  endif()
endif()

# ---- core: SDR capture + native PHY + decode engine (no UI) ----
add_library(openitc_core STATIC
  src/rtl_source.cpp
  src/dsp.cpp
  src/itc.cpp
  src/parse.cpp
  src/repository.cpp
  src/engine.cpp
  src/store.cpp
)
target_include_directories(openitc_core PUBLIC src)
target_link_libraries(openitc_core PUBLIC PkgConfig::RTLSDR PkgConfig::SQLITE3)
target_compile_options(openitc_core PRIVATE -Wall -Wextra)

# ---- console front-end ----
add_executable(openitc
  src/main.cpp
  src/decode.cpp
  src/native.cpp
)
target_link_libraries(openitc PRIVATE openitc_core)
if(WIN32)
  target_link_libraries(openitc PRIVATE PkgConfig::LIBZMQ ws2_32)
endif()
target_compile_options(openitc PRIVATE -Wall -Wextra)

# ---- vendored UI libraries (Dear ImGui + ImPlot) ----
add_library(imgui STATIC
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/backends/imgui_impl_glfw.cpp
  third_party/imgui/backends/imgui_impl_opengl3.cpp
  third_party/implot/implot.cpp
  third_party/implot/implot_items.cpp
)
target_include_directories(imgui PUBLIC
  third_party/imgui
  third_party/imgui/backends
  third_party/implot
)
target_link_libraries(imgui PUBLIC PkgConfig::GLFW OpenGL::GL)

# ---- GUI front-end ----
add_executable(openitc-gui WIN32
  src/gui/main_gui.cpp
  src/gui/app.cpp
  src/gui/panels.cpp
  src/gui/tiles.cpp
)
target_include_directories(openitc-gui PRIVATE third_party/stb)
target_link_libraries(openitc-gui PRIVATE openitc_core imgui)
if(WIN32)
  target_link_libraries(openitc-gui PRIVATE winhttp)
elseif(CURL_FOUND)
  target_link_libraries(openitc-gui PRIVATE CURL::libcurl)
  target_compile_definitions(openitc-gui PRIVATE OPENITC_HAVE_CURL=1)
endif()
if(APPLE)
  target_compile_definitions(imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()
target_compile_options(openitc-gui PRIVATE -Wall -Wextra)
