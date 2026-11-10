cmake_minimum_required(VERSION 3.24)
project(SkAiBrowser VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Sk/Ai native shell targets Windows x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CEF_ROOT)
  if(DEFINED ENV{CEF_ROOT})
    set(CEF_ROOT "$ENV{CEF_ROOT}")
  else()
    message(FATAL_ERROR "Set CEF_ROOT to an extracted CEF Windows x64 distribution.")
  endif()
endif()

# CEF 138+ uses the bootstrap architecture for the Chromium sandbox.
# Do not build a regular EXE with the obsolete cef_sandbox.lib approach.
set(USE_SANDBOX ON CACHE BOOL "Use the supported CEF Windows bootstrap sandbox" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
include("${CEF_ROOT}/cmake/cef_macros.cmake")

add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

add_library(SkAi SHARED
  src/main.cpp
  src/skai_app.cpp
  src/skai_client.cpp
  src/skai_window.cpp
  src/skai_app.h
  src/skai_client.h
  src/skai_window.h
)

target_include_directories(SkAi PRIVATE src "${CEF_INCLUDE_PATH}")
SET_LIBRARY_TARGET_PROPERTIES(SkAi)

target_compile_definitions(SkAi PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)

target_link_libraries(SkAi PRIVATE
  libcef_lib
  libcef_dll_wrapper
  ${CEF_STANDARD_LIBS}
  shell32
  ole32
)

target_compile_options(SkAi PRIVATE /W4 /permissive-)
set_target_properties(SkAi PROPERTIES
  PREFIX ""
  OUTPUT_NAME "SkAi"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# bootstrap.exe loads SkAi.dll and passes the sandbox handle into RunWinMain.
COPY_SINGLE_FILE(SkAi "${CEF_BINARY_DIR}/bootstrap.exe" "$<TARGET_FILE_DIR:SkAi>/SkAi.exe")
COPY_FILES(SkAi "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "$<TARGET_FILE_DIR:SkAi>")
COPY_FILES(SkAi "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:SkAi>")

add_custom_command(TARGET SkAi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/ui"
          "$<TARGET_FILE_DIR:SkAi>/ui"
  VERBATIM
)
