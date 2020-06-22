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

add_executable(ohvu-shell apps/shell.c)
set_property(TARGET ohvu-shell PROPERTY C_STANDARD 11)
target_include_directories(ohvu-shell PUBLIC include)
target_link_libraries(ohvu-shell data runtime io)

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
target_link_libraries(ohvu-shell ICU::uc ICU::data ICU::io)

if(BUILD_TESTING)
  include(CTest)
  FetchContent_MakeAvailable(Unity)
endif()

