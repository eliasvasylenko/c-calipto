add_executable(test-calipto test.c)
 
target_link_libraries(test-calipto src unity)
 
add_test(test_1 test-calipto)
