CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(iopp C)

#
# Files to build.
#

add_executable(iopp iopp.c)

#      
# Set compiler flags.
#

SET_SOURCE_FILES_PROPERTIES(iopp.c  COMPILE_FLAGS "-g -Wall")
