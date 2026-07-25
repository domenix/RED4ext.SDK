cmake_minimum_required(VERSION 3.21)

include(CMakeDependentOption)

project(
  RED4ext.SDK
  VERSION 1.0.0
  LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# General configuration / options / variables
# -----------------------------------------------------------------------------
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
elseif(CMAKE_CXX_STANDARD LESS 20)
  message(FATAL_ERROR "RED4ext.SDK requires C++20 or higher.")
endif()

if(PROJECT_IS_TOP_LEVEL)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

option(RED4EXT_HEADER_ONLY "Use the header only version of the library." OFF)
cmake_dependent_option(RED4EXT_USE_PCH "Use precompiled headers to speed up compilation time." OFF "NOT RED4EXT_HEADER_ONLY" OFF)

# The GpuApi headers wrap Direct3D 12 (<d3d12.h>, <dxgi1_6.h>, <wrl/*>) and describe the
# Windows renderer. There is no equivalent on other platforms, so they are excluded by
# default off Windows rather than stubbed.
option(RED4EXT_BUILD_GPUAPI "Build the GpuApi (Direct3D 12) portion of the SDK." ${WIN32})

set(RED4EXT_CMAKE_DIR "${PROJECT_SOURCE_DIR}/cmake")
set(RED4EXT_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include/")
set(RED4EXT_VENDOR_DIR "${PROJECT_SOURCE_DIR}/vendor/")

# -----------------------------------------------------------------------------
# Options related to warnings
# -----------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
  option(RED4EXT_EXTRA_WARNINGS "Enable extra warnings." OFF)
  if(RED4EXT_EXTRA_WARNINGS)
    if(MSVC)
      add_compile_options(/W4)
    else()
      add_compile_options(-Wall -Wextra)
    endif()
  endif()

  option(RED4EXT_TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors." OFF)
  if(RED4EXT_TREAT_WARNINGS_AS_ERRORS)
    if(MSVC)
      add_compile_options(/WX)
    else()
      add_compile_options(-Werror)
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Main library
# -----------------------------------------------------------------------------
configure_file(
  "${RED4EXT_CMAKE_DIR}/Version.hpp.in"
  "${RED4EXT_INCLUDE_DIR}/RED4ext/Version.hpp"
  @ONLY
)

# Compiler options needed to build the SDK's Windows-oriented sources with a
# non-MSVC compiler. Measured on Apple clang 21 / arm64:
#   -fdeclspec          accepts the 870 machine-generated __declspec(align(0x10))
#   -fms-extensions     accepts the remaining MSVC-isms
#   -Wno-unknown-pragmas silences the 50 `#pragma warning(...)` directives
# `-mms-bitfields` was measured to make no difference and is deliberately not set.
set(RED4EXT_NON_MSVC_OPTIONS "")
if(NOT MSVC)
  set(RED4EXT_NON_MSVC_OPTIONS -fdeclspec -fms-extensions -Wno-unknown-pragmas)
endif()

if(RED4EXT_HEADER_ONLY)
  add_library(RED4ext.SDK INTERFACE)

  target_include_directories(
    RED4ext.SDK
    INTERFACE
      "$<BUILD_INTERFACE:${RED4EXT_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )

  if(RED4EXT_BUILD_GPUAPI)
    target_include_directories(
      RED4ext.SDK
      INTERFACE "$<BUILD_INTERFACE:${RED4EXT_VENDOR_DIR}/D3D12MemAlloc>"
    )
  endif()

  target_compile_options(RED4ext.SDK INTERFACE ${RED4EXT_NON_MSVC_OPTIONS})
else()
  file(GLOB_RECURSE RED4EXT_HEADER_FILES "${RED4EXT_INCLUDE_DIR}/*.hpp")
  file(GLOB_RECURSE RED4EXT_SRC_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")

  if(NOT RED4EXT_BUILD_GPUAPI)
    list(FILTER RED4EXT_HEADER_FILES EXCLUDE REGEX "/RED4ext/GpuApi/")
    list(FILTER RED4EXT_SRC_FILES EXCLUDE REGEX "/src/GpuApi/")
  endif()

  get_property(RED4EXT_USE_FOLDERS GLOBAL PROPERTY USE_FOLDERS)
  if(RED4EXT_USE_FOLDERS)
    source_group(CMake REGULAR_EXPRESSION cmake_pch.*)

    source_group(
      TREE "${RED4EXT_INCLUDE_DIR}/RED4ext"
      FILES ${RED4EXT_HEADER_FILES}
    )

    source_group(
      TREE "${PROJECT_SOURCE_DIR}/src"
      FILES ${RED4EXT_SRC_FILES}
    )
  endif()

  add_library(
    RED4ext.SDK
    STATIC
      ${RED4EXT_HEADER_FILES}
      ${RED4EXT_SRC_FILES}
  )

  target_include_directories(
    RED4ext.SDK
    PUBLIC
      "$<BUILD_INTERFACE:${RED4EXT_INCLUDE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )

  if(RED4EXT_BUILD_GPUAPI)
    target_include_directories(
      RED4ext.SDK
      PUBLIC "$<BUILD_INTERFACE:${RED4EXT_VENDOR_DIR}/D3D12MemAlloc>"
    )
  endif()

  target_compile_definitions(
    RED4ext.SDK
      PUBLIC
        RED4EXT_STATIC_LIB
  )

  if(WIN32)
    target_compile_definitions(RED4ext.SDK PRIVATE WIN32_LEAN_AND_MEAN)
  endif()

  target_compile_options(RED4ext.SDK PUBLIC ${RED4EXT_NON_MSVC_OPTIONS})

  if(RED4EXT_USE_PCH)
    set(RED4EXT_PCH_FILE "${PROJECT_BINARY_DIR}/red4ext_pch.hpp")
    set(RED4EXT_PCH_FILE_IN "${RED4EXT_CMAKE_DIR}/pch.hpp.in")

    configure_file(
      "${RED4EXT_PCH_FILE_IN}"
      "${RED4EXT_PCH_FILE}"
      @ONLY
    )

    target_precompile_headers(RED4ext.SDK PRIVATE "${RED4EXT_PCH_FILE}")
  endif()
endif()

add_library(RED4ext::SDK ALIAS RED4ext.SDK)
add_library(RED4ext::RED4ext.SDK ALIAS RED4ext.SDK)

# -----------------------------------------------------------------------------
# Examples
# -----------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
  option(RED4EXT_BUILD_EXAMPLES "Build plugin examples." OFF)
  if(RED4EXT_BUILD_EXAMPLES)
    add_subdirectory(examples)
  endif()
endif()

# -----------------------------------------------------------------------------
# Install
# -----------------------------------------------------------------------------
if(NOT CMAKE_SKIP_INSTALL_RULES)
  option(RED4EXT_INSTALL "Generate the install target." ${PROJECT_IS_TOP_LEVEL})
  if(RED4EXT_INSTALL)
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    set(RED4EXT_EXPORT_NAME RED4ext)
    set(RED4EXT_EXPORT_TARGETS "${RED4EXT_EXPORT_NAME}Targets")

    set(RED4EXT_CMAKE_CONFIG_DIR "${PROJECT_BINARY_DIR}")

    set(RED4EXT_CMAKE_PROJECT_CONFIG_FILE "${RED4EXT_CMAKE_CONFIG_DIR}/${RED4EXT_EXPORT_NAME}Config.cmake")
    set(RED4EXT_CMAKE_PROJECT_CONFIG_FILE_IN "${RED4EXT_CMAKE_DIR}/${RED4EXT_EXPORT_NAME}Config.cmake.in")
    set(RED4EXT_CMAKE_PROJECT_VERSION_FILE "${RED4EXT_CMAKE_CONFIG_DIR}/${RED4EXT_EXPORT_NAME}ConfigVersion.cmake")

    set(RED4EXT_INSTALL_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${RED4EXT_EXPORT_NAME}")

    configure_package_config_file(
      "${RED4EXT_CMAKE_PROJECT_CONFIG_FILE_IN}"
      "${RED4EXT_CMAKE_PROJECT_CONFIG_FILE}"
      INSTALL_DESTINATION "${RED4EXT_INSTALL_CONFIG_DIR}"
    )

    write_basic_package_version_file(
      "${RED4EXT_CMAKE_PROJECT_VERSION_FILE}"
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY ExactVersion
    )

    install(
      DIRECTORY "${RED4EXT_INCLUDE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(
      TARGETS RED4ext.SDK
      EXPORT ${RED4EXT_EXPORT_TARGETS}
      INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
      PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    install(
      FILES
        "${RED4EXT_CMAKE_PROJECT_CONFIG_FILE}"
        "${RED4EXT_CMAKE_PROJECT_VERSION_FILE}"
      DESTINATION "${RED4EXT_INSTALL_CONFIG_DIR}"
    )

    install(
      EXPORT ${RED4EXT_EXPORT_TARGETS}
      NAMESPACE RED4ext::
      DESTINATION "${RED4EXT_INSTALL_CONFIG_DIR}"
    )

    # -------------------------------------------------------------------------
    # pkg-config
    # -------------------------------------------------------------------------
    set(RED4EXT_PKG_CONFIG_FILE "${RED4EXT_CMAKE_CONFIG_DIR}/${RED4EXT_EXPORT_NAME}.pc")
    set(RED4EXT_PKG_CONFIG_FILE_IN "${RED4EXT_CMAKE_DIR}/${RED4EXT_EXPORT_NAME}.pc.in")
    set(RED4EXT_INSTALL_PKG_CONFIG_DIR "${RED4EXT_INSTALL_CONFIG_DIR}/pkgconfig")

    if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(RED4EXT_PKG_CONFIG_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        set(RED4EXT_PKG_CONFIG_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()

    if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(RED4EXT_PKG_CONFIG_LIB_DIR "${CMAKE_INSTALL_LIBDIR}")
    else()
        set(RED4EXT_PKG_CONFIG_LIB_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    endif()

    configure_file(
      "${RED4EXT_PKG_CONFIG_FILE_IN}"
      "${RED4EXT_PKG_CONFIG_FILE}"
      @ONLY
    )

    install(
      FILES "${RED4EXT_PKG_CONFIG_FILE}"
      DESTINATION "${RED4EXT_INSTALL_PKG_CONFIG_DIR}"
    )
  endif()
endif()
