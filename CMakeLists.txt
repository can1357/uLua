cmake_minimum_required(VERSION 3.16)

project(
  ulua
  VERSION 0.2.0
  DESCRIPTION "Modern C++20 bindings for Lua 5.1 and LuaJIT"
  LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ULUA_BUILD_EXAMPLES "Build ulua example programs" ON)
option(ULUA_BUILD_TESTS "Build ulua test programs" ON)
option(ULUA_INSTALL "Install ulua headers and CMake package files" ON)
option(ULUA_NO_ACCEL "Disable LuaJIT internal-header acceleration" OFF)

set(ULUA_SUPPORTED_LUA_PROVIDER "" CACHE STRING "Lua provider selected for ulua (LuaJIT or Lua51)")
set_property(CACHE ULUA_SUPPORTED_LUA_PROVIDER PROPERTY STRINGS LuaJIT Lua51)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(ULUA_SUPPORTED_LUA_PROVIDER)
  set(_ulua_provider_candidates "${ULUA_SUPPORTED_LUA_PROVIDER}")
else()
  set(_ulua_provider_candidates LuaJIT Lua51)
endif()

set(_ulua_dependency_target "")
set(_ulua_provider "")
foreach(_ulua_candidate IN LISTS _ulua_provider_candidates)
  if(_ulua_candidate STREQUAL "LuaJIT")
    find_package(LuaJIT QUIET)
    if(LuaJIT_FOUND)
      set(_ulua_provider LuaJIT)
      set(_ulua_dependency_target LuaJIT::LuaJIT)
      break()
    endif()
  elseif(_ulua_candidate STREQUAL "Lua51")
    find_package(Lua51 QUIET)
    if(Lua51_FOUND)
      set(_ulua_provider Lua51)
      set(_ulua_dependency_target Lua51::Lua51)
      break()
    endif()
  else()
    message(FATAL_ERROR "Unsupported ULUA_SUPPORTED_LUA_PROVIDER='${_ulua_candidate}'. Expected LuaJIT or Lua51.")
  endif()
endforeach()

if(NOT _ulua_dependency_target)
  message(FATAL_ERROR
    "ulua requires LuaJIT or Lua 5.1. Set ULUA_SUPPORTED_LUA_PROVIDER explicitly or provide CMAKE_PREFIX_PATH for the dependency."
  )
endif()

add_library(ulua INTERFACE)
add_library(ulua::ulua ALIAS ulua)

target_compile_features(ulua INTERFACE cxx_std_20)
target_include_directories(ulua
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(ulua INTERFACE ${_ulua_dependency_target})
if(ULUA_NO_ACCEL)
  target_compile_definitions(ulua INTERFACE ULUA_NO_ACCEL=1)
endif()

if(ULUA_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ULUA_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()

if(ULUA_INSTALL)
  install(TARGETS ulua EXPORT uluaTargets)
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/uluaConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uluaConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/uluaConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ulua"
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/uluaConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/uluaConfigVersion.cmake"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindLuaJIT.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ulua"
  )

  install(
    EXPORT uluaTargets
    NAMESPACE ulua::
    FILE uluaTargets.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ulua"
  )
endif()

message(STATUS "ulua configured with ${_ulua_provider}")
