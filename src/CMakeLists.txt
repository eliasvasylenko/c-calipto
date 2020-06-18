file(GLOB SOURCES ./*.c)

add_executable(ohvu ${SOURCES})

set_property(TARGET ohvu PROPERTY C_STANDARD 11)

target_include_directories(ohvu PUBLIC ../include)
target_include_directories(ohvu PUBLIC "${PROJECT_BINARY_DIR}")

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
target_link_libraries(ohvu ICU::uc ICU::data ICU::io)

