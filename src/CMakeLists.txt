if(HARK_BUILD_SHARED)
  add_library(hark SHARED)
else()
  add_library(hark STATIC)
endif()

add_library(hark::hark ALIAS hark)

target_sources(hark PRIVATE
  connector.c
  error.c
  reactor.c
  signal.c
  timer.c
)

target_include_directories(hark
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(hark PROPERTIES
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE ON
  OUTPUT_NAME hark
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  include(CompilerWarnings)
  target_set_warnings(hark)
  include(Hardening)
  target_set_hardening(hark)
  include(Sanitizers)
  target_set_sanitizers(hark)
endif()

include(GNUInstallDirs)
install(TARGETS hark
  EXPORT harkTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(EXPORT harkTargets
  FILE harkTargets.cmake
  NAMESPACE hark::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hark
)
