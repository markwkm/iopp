CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
PROJECT(IOPP C)

#
# Files to build.
#

add_executable(iopp iopp.c)

#      
# Set compiler flags.
#

SET_SOURCE_FILES_PROPERTIES(iopp.c  COMPILE_FLAGS "-g -Wall")

#
# Install rules.
#

INSTALL(PROGRAMS iopp DESTINATION "/bin")
