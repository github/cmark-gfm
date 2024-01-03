add_library(libcmark-gfm-extensions
  autolink.c
  core-extensions.c
  ext_scanners.c
  ext_scanners.h
  ext_scanners.re
  strikethrough.c
  table.c
  tagfilter.c
  tasklist.c)
set_target_properties(libcmark-gfm-extensions PROPERTIES
  DEFINE_SYMBOL cmark-gfm
  MACOSX_RPATH TRUE
  OUTPUT_NAME cmark-gfm-extensions
  PDB_NAME libcmark-gfm-extensions
  SOVERSION ${PROJECT_VERSION}
  VERSION ${PROJECT_VERSION})
target_link_libraries(libcmark-gfm-extensions PRIVATE
  libcmark-gfm)


install(TARGETS libcmark-gfm-extensions
  EXPORT cmark-gfm-extensions
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES cmark-gfm-core-extensions.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cmark-gfm-extensions
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake-gfm-extensions)
