# 创建线程模块静态库
add_library(thread STATIC src/thread.c)

# 设置头文件包含路径
target_include_directories(thread PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src
    ${CMAKE_SOURCE_DIR}/src/core/log/include)

# 查找并链接系统线程库
find_package(Threads REQUIRED)
target_link_libraries(thread PUBLIC Threads::Threads log)

# 安装库文件和头文件
install(TARGETS thread EXPORT CrolinKitTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/thread.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 复制头文件到构建目录
configure_file(include/thread.h ${CMAKE_BINARY_DIR}/include/thread.h COPYONLY)
