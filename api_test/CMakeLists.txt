add_executable(api_test
  cplusplus.cpp
  harness.c
  main.c)
target_link_libraries(api_test PRIVATE
  libcmark-gfm
  libcmark-gfm-extensions)

add_test(NAME api_test COMMAND api_test)
if(WIN32)
  if(CMARK_SHARED)
    set_tests_properties(api_test PROPERTIES
      ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$<TARGET_FILE_DIR:libcmark-gfm>;$ENV{PATH}")
  endif()
endif()
