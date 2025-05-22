# 示例目录的CMakeLists.txt
cmake_minimum_required(VERSION 3.8)

# 设置示例可执行文件
set(EXAMPLE_SOURCES
    thread_pool_example.c
)

# 创建示例可执行文件
add_executable(thread_pool_example ${EXAMPLE_SOURCES})

# 链接线程池库
target_link_libraries(thread_pool_example PRIVATE thread)

# 包含线程池头文件目录
target_include_directories(thread_pool_example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../thread/include
)
