add_subdirectory(thread)

add_executable(thread_pool_example thread_pool_example.c)
target_link_libraries(thread_pool_example PRIVATE thread)
target_include_directories(thread_pool_example PRIVATE 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/thread/include
)
