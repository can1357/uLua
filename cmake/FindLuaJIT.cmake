include(FindPackageHandleStandardArgs)

find_path(LuaJIT_INCLUDE_DIR
  NAMES luajit.h lua.hpp
  PATH_SUFFIXES include luajit-2.1 luajit-2.0 include/luajit-2.1 include/luajit-2.0
  HINTS
    ENV LuaJIT_ROOT
    ENV LUAJIT_ROOT
    /opt/homebrew
    /usr/local
    /usr
)

find_library(LuaJIT_LIBRARY
  NAMES luajit-5.1 luajit-5.1.2 luajit
  PATH_SUFFIXES lib lib64
  HINTS
    ENV LuaJIT_ROOT
    ENV LUAJIT_ROOT
    /opt/homebrew
    /usr/local
    /usr
)

find_package_handle_standard_args(LuaJIT
  REQUIRED_VARS LuaJIT_LIBRARY LuaJIT_INCLUDE_DIR
)

if(LuaJIT_FOUND AND NOT TARGET LuaJIT::LuaJIT)
  set(_ulua_luajit_link_deps "")
  if(LuaJIT_LIBRARY MATCHES "\\.(a|lib)$")
    if(UNIX AND NOT APPLE)
      list(APPEND _ulua_luajit_link_deps m dl)
    endif()
  endif()

  add_library(LuaJIT::LuaJIT UNKNOWN IMPORTED)
  set_target_properties(LuaJIT::LuaJIT PROPERTIES
    IMPORTED_LOCATION "${LuaJIT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LuaJIT_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${_ulua_luajit_link_deps}"
  )
endif()
