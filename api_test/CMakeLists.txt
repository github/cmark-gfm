add_executable(api_test
  cplusplus.cpp
  harness.c
  harness.h
  main.c
)
include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}/src
  ${PROJECT_BINARY_DIR}/extensions
)
if(CMARK_SHARED)
  target_link_libraries(api_test libcmark-gfm-extensions libcmark-gfm)
else()
  target_link_libraries(api_test libcmark-gfm-extensions_static libcmark-gfm_static)
endif()

add_test(NAME api_test COMMAND api_test)
if(WIN32)
  if(CMARK_SHARED)
    set_tests_properties(api_test PROPERTIES
      ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$<TARGET_FILE_DIR:libcmark-gfm>;$ENV{PATH}")
  endif()
endif()
