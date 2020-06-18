add_executable(test-intrie test.c)
 
target_link_libraries(test-intrie intrie unity)
 
add_test(test_1 test-intrie)
