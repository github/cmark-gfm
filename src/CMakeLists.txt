
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 29)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_GFM 13)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmark-gfm_version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmark-gfm_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libcmark-gfm.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/libcmark-gfm.pc @ONLY)

add_library(libcmark-gfm
  arena.c
  blocks.c
  buffer.c
  cmark.c
  cmark_ctype.c
  commonmark.c
  footnotes.c
  houdini_href_e.c
  houdini_html_e.c
  houdini_html_u.c
  html.c
  inlines.c
  iterator.c
  latex.c
  linked_list.c
  man.c
  map.c
  node.c
  plaintext.c
  plugin.c
  references.c
  registry.c
  render.c
  scanners.c
  scanners.re
  syntax_extension.c
  utf8.c
  xml.c)
target_include_directories(libcmark-gfm PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_sources(libcmark-gfm PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/cmark-gfm_version.h)
set_target_properties(libcmark-gfm PROPERTIES
  DEFINE_SYMBOL cmark-gfm
  MACOSX_RPATH TRUE
  OUTPUT_NAME cmark-gfm
  # Avoid name clash between PROGRAM and LIBRARY pdb files.
  PDB_NAME libcmark-gfm
  SOVERSION ${PROJECT_VERSION}
  VERSION ${PROJECT_VERSION})
generate_export_header(libcmark-gfm
  BASE_NAME ${PROJECT_NAME})

add_executable(cmark-gfm
  main.c)
target_link_libraries(cmark-gfm PRIVATE
  libcmark-gfm
  libcmark-gfm-extensions)
if(MSVC)
  target_link_options(cmark-gfm PRIVATE
    /INCREMENTAL:NO)
endif()


install(TARGETS cmark-gfm libcmark-gfm
  EXPORT cmark-gfm
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libcmark-gfm.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES
  cmark-gfm.h
  cmark-gfm-extension_api.h
  ${CMAKE_CURRENT_BINARY_DIR}/cmark-gfm_export.h
  ${CMAKE_CURRENT_BINARY_DIR}/cmark-gfm_version.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cmark-gfm
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
