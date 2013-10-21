# Project
project(ftdi-flash-tool)
set(PACKAGE ftdi-flash-tool)
set ( CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake )

# CMake
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
   set(CMAKE_BUILD_TYPE     RelWithDebInfo)
endif("${CMAKE_BUILD_TYPE}" STREQUAL "")
set(CMAKE_COLOR_MAKEFILE ON)
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)

add_definitions(-Wall)

# Debug build
message("-- Build type: ${CMAKE_BUILD_TYPE}")
if(${CMAKE_BUILD_TYPE} STREQUAL Debug)
   add_definitions(-DDEBUG)
endif(${CMAKE_BUILD_TYPE} STREQUAL Debug)

# find Confuse
find_package ( Confuse REQUIRED )
include_directories ( ${CONFUSE_INCLUDE_DIR} )

# find libusb
find_package ( USB1 REQUIRED )
include_directories ( ${LIBUSB_INCLUDE_DIR} )

# find libxml2
find_package ( LibXML2 REQUIRED )
include_directories ( ${LibXML2_INCLUDE_DIR} )

# find libftdi
find_package ( LibFTDI1 REQUIRED )
include_directories ( ${LIBFTDI_INCLUDE_DIR} )

# Set current version
execute_process( COMMAND git describe --tags HEAD
								 OUTPUT_VARIABLE VER_STRING 
								 OUTPUT_STRIP_TRAILING_WHITESPACE )
set(VERSION_STRING ${VER_STRING})

# Set components
add_subdirectory(src)

# Documentation
option ( DOCUMENTATION "Generate API documentation with Doxygen" OFF )


find_package ( Doxygen )
if ( DOCUMENTATION AND DOXYGEN_FOUND )

   # Find doxy config
   message(STATUS "Doxygen found.")

   # Copy doxy.config.in
   set(top_srcdir ${CMAKE_SOURCE_DIR})
   configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile )
   configure_file(${CMAKE_SOURCE_DIR}/doc/Doxyfile.xml.in ${CMAKE_BINARY_DIR}/Doxyfile.xml )

   # Run doxygen
   add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/doc/html/index.html
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
    DEPENDS ${c_headers};${c_sources};${cpp_sources};${cpp_headers}
   )

   add_custom_target(docs ALL DEPENDS ${CMAKE_BINARY_DIR}/doc/html/index.html)

   message(STATUS "Generating API documentation with Doxygen")
else(DOCUMENTATION AND DOXYGEN_FOUND)
   message(STATUS "Not generating API documentation")
endif(DOCUMENTATION AND DOXYGEN_FOUND)

# PkgConfig
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX}/bin)
set(includedir  ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME})

if(${UNIX})
   set(libdir      ${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX})
endif(${UNIX})
if(${WIN32})
   set(libdir      ${CMAKE_INSTALL_PREFIX}/bin)
endif(${WIN32})

