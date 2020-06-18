cmake_minimum_required(VERSION 3.1...3.15)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# set the project name
project(c-ohvu VERSION 0.1
	DESCRIPTION "A simple interpreter for the Ohvu programming language, implemented in C"
	LANGUAGES C)

configure_file(src/c-ohvu.h.in c-ohvu.h)

SET(TARGET_GROUP release CACHE STRING "The type of build, accepts 'release' or 'test'.")

add_subdirectory(src)

if(TARGET_GROUP STREQUAL release)
  add_subdirectory(src)
  # release only
elseif(TARGET_GROUP STREQUAL test)
  include(CTest)
  add_subdirectory(lib/unity)
else() 
  message(FATAL_ERROR "I don't know the TARGET_GROUP you gave me!")
endif()

