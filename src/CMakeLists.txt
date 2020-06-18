file(GLOB SOURCES ./*.c)

add_executable(calipto ${SOURCES})

set_property(TARGET calipto PROPERTY C_STANDARD 11)

target_include_directories(calipto PUBLIC ../include)
target_include_directories(calipto PUBLIC "${PROJECT_BINARY_DIR}")

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
target_link_libraries(calipto ICU::uc ICU::data ICU::io)

