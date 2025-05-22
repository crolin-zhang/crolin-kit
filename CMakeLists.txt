cmake_minimum_required(VERSION 3.8)

# 设置项目名称和版本
project(ThreadPoolLibrary VERSION 1.0.0 LANGUAGES C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加线程池子目录
add_subdirectory(thread)

# 添加测试目录
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 添加示例目录
option(BUILD_EXAMPLES "Build the examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
