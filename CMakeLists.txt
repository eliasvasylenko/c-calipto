cmake_minimum_required(VERSION 3.1...3.15)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# set the project name
project(c-calipto VERSION 0.1
	DESCRIPTION "A simple interpreter for the Calipto language, implemented in C"
	LANGUAGES C)

configure_file(src/c-calipto.h.in c-calipto.h)

# add the executable
add_executable(calipto repl.c src/reader.c src/scanner.c src/stream.c src/sexpr.c src/interpreter.c)

set_property(TARGET calipto PROPERTY C_STANDARD 11)

target_include_directories(calipto PUBLIC include)
target_include_directories(calipto PUBLIC "${PROJECT_BINARY_DIR}")

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
target_link_libraries(calipto ICU::uc ICU::data ICU::io)
