# 创建日志模块静态库
add_library(log STATIC src/log.c)

# 设置头文件包含路径
target_include_directories(log PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE src)

# 查找并链接系统线程库
find_package(Threads REQUIRED)
target_link_libraries(log PUBLIC Threads::Threads)

# 安装库文件和头文件
install(TARGETS log EXPORT CrolinKitTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/log.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 复制头文件到构建目录
configure_file(include/log.h ${CMAKE_BINARY_DIR}/include/log.h COPYONLY)
