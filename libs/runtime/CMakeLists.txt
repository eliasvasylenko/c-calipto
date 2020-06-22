file(GLOB SOURCES src/*.c)
add_library(runtime SHARED ${SOURCES})
set_property(TARGET runtime PROPERTY C_STANDARD 11)

target_link_libraries(runtime data io)

target_include_directories(runtime PUBLIC include)

configure_file(c-ohvu.h.in c-ohvu.h)
target_include_directories(runtime PUBLIC "${PROJECT_BINARY_DIR}")

if(BUILD_TESTING)
#  add_subdirectory(test)
endif()

