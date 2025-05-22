cmake_minimum_required(VERSION 3.8)

# 设置项目名称
set(PROJECT_NAME thread)
project(${PROJECT_NAME} C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

# 设置调试模式下的编译选项
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")

# 设置发布模式下的编译选项
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG")

# 设置线程库源文件
set(THREAD_SOURCES
    src/thread.c
)

# 设置线程库头文件目录
set(THREAD_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 创建线程库
add_library(${PROJECT_NAME} STATIC ${THREAD_SOURCES})

# 设置线程库的包含目录
target_include_directories(${PROJECT_NAME} PUBLIC ${THREAD_INCLUDE_DIRS})

# 链接pthread库
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

# 安装规则
install(TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# 注意：测试和示例已移动到顶层目录
