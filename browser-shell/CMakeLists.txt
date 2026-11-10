cmake_minimum_required(VERSION 3.24)
project(SkAiBrowser VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Sk/Ai native shell currently targets Windows x64 only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CEF_ROOT)
  if(DEFINED ENV{CEF_ROOT})
    set(CEF_ROOT "$ENV{CEF_ROOT}")
  else()
    message(FATAL_ERROR "Set CEF_ROOT to an extracted CEF binary distribution (Windows x64).")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory("${CEF_ROOT}" libcef_dll_wrapper)

add_executable(SkAi WIN32
  src/main.cpp
  src/skai_app.cpp
  src/skai_client.cpp
  src/skai_window.cpp
  src/skai_app.h
  src/skai_client.h
  src/skai_window.h
  resources/skai.rc
)

target_include_directories(SkAi PRIVATE src)
target_link_libraries(SkAi PRIVATE libcef_lib libcef_dll_wrapper)
target_compile_definitions(SkAi PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(SkAi PRIVATE /W4 /permissive-)

set_target_properties(SkAi PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# CEF runtime resources and the Sk/Ai web UI are copied beside the executable.
COPY_FILES(SkAi "${CEF_BINARY_FILES}" "${CEF_RESOURCE_FILES}")
add_custom_command(TARGET SkAi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "$<TARGET_FILE_DIR:SkAi>/ui"
)
