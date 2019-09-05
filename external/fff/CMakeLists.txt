if(NOT TARGET meekrosoft::fff)
  add_library(meekrosoft::fff INTERFACE IMPORTED GLOBAL)

  # TODO switch to better, commented syntax when we are able to move beyond
  # CMake 3.10.
  # target_include_directories(meekrosoft::fff INTERFACE ${CMAKE_CURRENT_LIST_DIR})
  set_target_properties(meekrosoft::fff PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR})

  # FFF generates a lot of unreachable code warnings in Visual C++
  if(MSVC)
    target_compile_options(meekrosoft::fff INTERFACE /wd4702)
  endif()
endif()
