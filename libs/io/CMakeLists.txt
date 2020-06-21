file(GLOB SOURCES src/*.c)
add_library(io SHARED ${SOURCES})
set_property(TARGET io PROPERTY C_STANDARD 11)

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)
target_link_libraries(io ICU::uc ICU::data ICU::io)

target_include_directories(io PUBLIC include)

if(TARGET_GROUP STREQUAL test)
#  add_subdirectory(test)
endif()

