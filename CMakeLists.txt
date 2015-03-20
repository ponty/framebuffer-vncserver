PROJECT(framebuffer-vncserver)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)


FILE(GLOB SOURCES src/*.c)
ADD_EXECUTABLE(framebuffer-vncserver ${SOURCES})
INSTALL(TARGETS framebuffer-vncserver RUNTIME DESTINATION bin)

# LIBVNC
find_library(LIBVNC NAMES libvncserver vncserver)
target_link_libraries(framebuffer-vncserver ${LIBVNC})


MESSAGE( STATUS "LIBVNC:         " ${LIBVNC} )
