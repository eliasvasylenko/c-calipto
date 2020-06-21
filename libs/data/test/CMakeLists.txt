add_executable(test-data test.c)
 
target_link_libraries(test-data data unity)
 
add_test(test_1 test-data)
