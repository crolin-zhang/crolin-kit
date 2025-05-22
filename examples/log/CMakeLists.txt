add_executable(log_basic_example log_basic_example.c)
target_link_libraries(log_basic_example PRIVATE log)
target_include_directories(log_basic_example PRIVATE 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/log/include
)

add_executable(log_advanced_example log_advanced_example.c)
target_link_libraries(log_advanced_example PRIVATE log)
target_include_directories(log_advanced_example PRIVATE 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/log/include
)
