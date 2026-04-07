include(FetchContent)
include(CheckCXXSourceCompiles)

set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(doctest)

unset(ULUA_HAS_LUAJIT_PRIVATE_HEADERS CACHE)
if(LuaJIT_FOUND AND NOT ULUA_NO_ACCEL)
  set(CMAKE_REQUIRED_INCLUDES "${PROJECT_SOURCE_DIR}/include;${LuaJIT_INCLUDE_DIR}")
  check_cxx_source_compiles([=[
    #include <ulua/lua_api.hpp>
    #if !ULUA_JIT
    #error LuaJIT private headers unavailable
    #endif
    int main() { return 0; }
  ]=] ULUA_HAS_LUAJIT_PRIVATE_HEADERS)
  unset(CMAKE_REQUIRED_INCLUDES)
else()
  set(ULUA_HAS_LUAJIT_PRIVATE_HEADERS FALSE)
endif()

add_executable(test_ulua
  test_state.cpp
  test_stack.cpp
  test_types.cpp
  test_table.cpp
  test_function.cpp
  test_userdata.cpp
  test_coroutine.cpp)

target_link_libraries(test_ulua PRIVATE ulua::ulua doctest::doctest_with_main)
target_compile_definitions(test_ulua PRIVATE ULUA_NO_ACCEL=1)

add_test(NAME ulua.unit COMMAND test_ulua)

function(ulua_add_exec_test target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE ulua::ulua)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

ulua_add_exec_test(ulua-smoke smoke.cpp)
ulua_add_exec_test(ulua-advanced advanced.cpp)
if(ULUA_HAS_LUAJIT_PRIVATE_HEADERS)
  ulua_add_exec_test(ulua-ffi ffi.cpp)
endif()
