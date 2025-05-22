add_executable(thread_example thread_example.c)
target_link_libraries(thread_example PRIVATE thread)
target_include_directories(thread_example PRIVATE ${CMAKE_BINARY_DIR}/include)
