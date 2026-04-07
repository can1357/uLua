function(ulua_add_example target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE ulua::ulua)
endfunction()

ulua_add_example(ulua-basic basic.cpp)
ulua_add_example(ulua-hello hello.cpp)
ulua_add_example(ulua-userdata userdata.cpp)
