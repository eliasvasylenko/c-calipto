cmake_minimum_required(VERSION 3.14...3.15)

# set the project name
project(c-ohvu VERSION 0.1
	DESCRIPTION "A simple interpreter for the Ohvu programming language, implemented in C"
	LANGUAGES C)

include(FetchContent)
FetchContent_Declare(libpopcnt GIT_REPOSITORY https://github.com/kimwalisch/libpopcnt GIT_TAG e3e3233ddd16eb2e609ab46deb75754f58034c9a)
FetchContent_Declare(Unity GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity/ GIT_TAG 0126e4804cf5edf87412044f841d7d7023a7c0c3)

SET(TARGET_GROUP release CACHE STRING "The type of build, accepts 'release' or 'test'.")

FetchContent_MakeAvailable(libpopcnt)
add_subdirectory(libs/data)
add_subdirectory(libs/runtime)
add_subdirectory(libs/io)

if(TARGET_GROUP STREQUAL release)
  file(GLOB SOURCES src/*.c)
  add_executable(ohvu ${SOURCES})
  set_property(TARGET ohvu PROPERTY C_STANDARD 11)

  configure_file(src/c-ohvu.h.in c-ohvu.h)
  target_include_directories(ohvu PUBLIC include)
  target_include_directories(ohvu PUBLIC "${PROJECT_BINARY_DIR}")

  find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
  target_link_libraries(ohvu ICU::uc ICU::data ICU::io)

elseif(TARGET_GROUP STREQUAL test)
  include(CTest)
  FetchContent_MakeAvailable(Unity)

else() 
  message(FATAL_ERROR "I don't know the TARGET_GROUP you gave me!")
endif()

