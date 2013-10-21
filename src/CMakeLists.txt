if ( CONFUSE_FOUND AND LibXML2_FOUND AND LIBUSB_FOUND AND LIBFTDI_FOUND )
  message(STATUS "Configuring ftdi-flash-tool *${VERSION_STRING}*")

  #include_directories ( ${CONFUSE_INCLUDE_DIRS} )
  #include_directories ( ${LIBFTDI_INCLUDE_DIRS} )
  #include_directories ( ${LIBUSB_INCLUDE_DIRS} )
	#include_directories ( ${LibXML2_INCLUDE_DIRS} )

  list ( APPEND libs ${LIBFTDI_LIBRARIES} )
  list ( APPEND libs ${CONFUSE_LIBRARIES} )
  list ( APPEND libs ${LIBUSB_LIBRARIES} )
  list ( APPEND libs ${LibXML2_LIBRARIES} )

  include_directories ( BEFORE ${CMAKE_SOURCE_DIR}/src )
  include_directories ( BEFORE ${CMAKE_CURRENT_BINARY_DIR} )

  # Version defines
	add_definitions( -DEEPROM_VERSION_STRING="${VERSION_STRING}" )

  add_executable ( ftdi-flash-tool main.c )
  target_link_libraries ( ftdi-flash-tool ${LIBFTDI_LIBRARIES} )
  target_link_libraries ( ftdi-flash-tool ${LIBUSB_LIBRARIES} )
  target_link_libraries ( ftdi-flash-tool ${CONFUSE_LIBRARIES} )

  add_executable ( ftdi-xform-config ftdi_config_reader.c )
  target_link_libraries ( ftdi-xform-config ${LibXML2_LIBRARIES} )

  install ( TARGETS ftdi-flash-tool DESTINATION bin )
else ()
  message ( STATUS "libConfuse or libusb1 or libxml2 or libftdi not found, won't build ftdi-flash-tool" )
endif ()

