add_executable(bdtrie-test bdtrie_test.c)
 
target_link_libraries(bdtrie-test data unity)
 
add_test(bdtrie-test bdtrie-test)
