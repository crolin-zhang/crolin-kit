add_executable(log_unit_test log_unit_test.c)
target_link_libraries(log_unit_test PRIVATE log)
target_include_directories(log_unit_test PRIVATE 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/log/include
)

add_test(NAME log_unit_test COMMAND log_unit_test)
