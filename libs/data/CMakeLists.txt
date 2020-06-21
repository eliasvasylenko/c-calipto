file(GLOB SOURCES src/*.c)
add_library(data SHARED ${SOURCES})
set_property(TARGET data PROPERTY C_STANDARD 11)

find_package(ICU 61.0 COMPONENTS uc data io REQUIRED)

target_link_libraries(data io ICU::uc ICU::data ICU::io)

target_include_directories(data PRIVATE "${libpopcnt_SOURCE_DIR}")
target_include_directories(data PUBLIC include)

if(TARGET_GROUP STREQUAL test)
  add_subdirectory(test)
endif()

