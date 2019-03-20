if(NOT TARGET meekrosoft::fff)
  add_library(meekrosoft::fff INTERFACE IMPORTED GLOBAL)
  target_include_directories(meekrosoft::fff INTERFACE ${CMAKE_CURRENT_LIST_DIR})

  # FFF generates a lot of unreachable code warnings in Visual C++
  if(MSVC)
    target_compile_options(meekrosoft::fff INTERFACE /wd4702)
  endif()
endif()
