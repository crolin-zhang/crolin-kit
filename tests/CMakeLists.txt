# 测试目录的CMakeLists.txt
cmake_minimum_required(VERSION 3.8)

# 设置测试可执行文件
set(TEST_SOURCES
    test_thread_pool.c
)

# 创建测试可执行文件
add_executable(test_thread_pool ${TEST_SOURCES})

# 链接线程池库
target_link_libraries(test_thread_pool PRIVATE thread)

# 包含线程池头文件目录
target_include_directories(test_thread_pool PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../thread/include
)
