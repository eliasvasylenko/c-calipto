file(GLOB SOURCES src/*.c)
add_library(runtime SHARED ${SOURCES})
set_property(TARGET runtime PROPERTY C_STANDARD 11)

target_link_libraries(runtime data io)

target_include_directories(runtime PUBLIC include)

if(TARGET_GROUP STREQUAL test)
#  add_subdirectory(test)
endif()

