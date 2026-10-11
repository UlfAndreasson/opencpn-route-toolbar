cmake_minimum_required(VERSION 3.16)

project(route_toolbar_pi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/opencpn-libs/api-18/CMakeLists.txt")
  message(FATAL_ERROR "OpenCPN API submodule missing. Run: git submodule update --init --recursive")
endif()

add_subdirectory(opencpn-libs/api-18 EXCLUDE_FROM_ALL)

add_library(route_toolbar_pi SHARED
  src/route_toolbar_pi.cpp
  src/route_toolbar_dialog.cpp
)

target_include_directories(route_toolbar_pi PRIVATE include)
target_link_libraries(route_toolbar_pi PRIVATE ${wxWidgets_LIBRARIES} ocpn::api)
target_compile_definitions(route_toolbar_pi PRIVATE MAKING_PLUGIN)

if(APPLE)
  target_link_options(route_toolbar_pi PRIVATE "LINKER:-undefined,dynamic_lookup")
endif()

set_target_properties(route_toolbar_pi PROPERTIES
  OUTPUT_NAME "route_toolbar_pi"
)

install(TARGETS route_toolbar_pi
  LIBRARY DESTINATION lib/opencpn
  RUNTIME DESTINATION lib/opencpn
)
