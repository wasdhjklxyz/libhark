cmake_minimum_required(VERSION 3.20)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HARK_VERSION)
string(STRIP "${HARK_VERSION}" HARK_VERSION)

project(hark
  VERSION ${HARK_VERSION}
  LANGUAGES C
  DESCRIPTION "Event-driven fd reactor with timers and reconnect"
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(HARK_BUILD_EXAMPLES "Build examples"            OFF)
option(HARK_BUILD_SHARED   "Build shared library"      OFF)
option(HARK_ASAN           "Enable AddressSanitizer"   OFF)

add_subdirectory(src)

if(HARK_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

include(cmake/CompilerWarnings.cmake)

include(GNUInstallDirs)
install(
  DIRECTORY include/hark/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hark
)

configure_file(
  include/hark/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/hark/version.h
  @ONLY
)
