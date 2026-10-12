cmake_minimum_required(VERSION 3.16)

project(route_toolbar_pi VERSION 0.5.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# OpenCPN 5.14.2 on macOS uses wxWidgets 3.2. Building this plugin against
# wxWidgets 3.3 can load successfully but crash when creating wx windows.
if(APPLE AND NOT wxWidgets_CONFIG_EXECUTABLE)
  find_program(WX_CONFIG_32 NAMES wx-config-3.2)
  if(WX_CONFIG_32)
    set(wxWidgets_CONFIG_EXECUTABLE "${WX_CONFIG_32}" CACHE FILEPATH "wxWidgets 3.2 wx-config" FORCE)
  endif()
endif()

find_package(wxWidgets REQUIRED COMPONENTS core base xml)
include(${wxWidgets_USE_FILE})

if(DEFINED wxWidgets_VERSION_STRING AND NOT wxWidgets_VERSION_STRING MATCHES "^3\\.2\\.")
  message(FATAL_ERROR
    "Route Toolbar must be built with wxWidgets 3.2 for OpenCPN 5.14.2. "
    "Detected wxWidgets ${wxWidgets_VERSION_STRING}. On macOS install wxwidgets@3.2 "
    "and configure with -DwxWidgets_CONFIG_EXECUTABLE=$(brew --prefix wxwidgets@3.2)/bin/wx-config-3.2")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/opencpn-libs/api-21/CMakeLists.txt")
  message(FATAL_ERROR "OpenCPN API submodule missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(opencpn-libs/api-21 EXCLUDE_FROM_ALL)

add_library(route_toolbar_pi SHARED
  src/route_toolbar_pi.cpp
  src/route_toolbar_dialog.cpp
  src/route_report.cpp
  src/route_report_pdf.cpp
  src/route_gpx.cpp
  src/route_pil.cpp
  src/route_turn.cpp
  src/route_context.cpp
  src/route_labels.cpp
)

target_include_directories(route_toolbar_pi PRIVATE include)
target_link_libraries(route_toolbar_pi PRIVATE ${wxWidgets_LIBRARIES} ocpn::api)
target_compile_definitions(route_toolbar_pi PRIVATE MAKING_PLUGIN)

if(APPLE)
  find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
  target_link_libraries(route_toolbar_pi PRIVATE ${OPENGL_FRAMEWORK})
  target_link_options(route_toolbar_pi PRIVATE "LINKER:-undefined,dynamic_lookup")

  # Plugins must use the wxWidgets runtime shipped inside OpenCPN.app rather
  # than loading a second Homebrew wxWidgets copy into the same process.
  add_custom_command(TARGET route_toolbar_pi POST_BUILD
    COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fix-macos-libs.sh" "$<TARGET_FILE:route_toolbar_pi>"
    VERBATIM
    COMMENT "Relinking wxWidgets dependencies to OpenCPN.app Frameworks and ad-hoc signing"
  )
endif()

set_target_properties(route_toolbar_pi PROPERTIES
  OUTPUT_NAME "route_toolbar_pi"
)

install(TARGETS route_toolbar_pi
  LIBRARY DESTINATION lib/opencpn
  RUNTIME DESTINATION lib/opencpn
)
