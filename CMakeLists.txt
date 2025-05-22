# 设置最低CMake版本要求
cmake_minimum_required(VERSION 3.8)

# 生成compile_commands.json用于代码分析工具
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MIPS交叉编译选项
option(USE_MIPS_TOOLCHAIN "Use MIPS cross-compilation toolchain" OFF)
if(USE_MIPS_TOOLCHAIN)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/mips-linux-gnu.cmake" CACHE FILEPATH "MIPS toolchain file" FORCE)
endif()

# 设置项目名称和版本
project(CrolinKit VERSION 1.0.0 LANGUAGES C)

# 复制compile_commands.json到项目根目录便于代码分析工具使用
add_custom_target(copy-compile-commands
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
  VERBATIM
)


set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_CROSSCOMPILING)
    add_compile_options(-fPIC)
    add_compile_definitions(CROSS_COMPILE)
endif()

include(GNUInstallDirs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
enable_testing()

add_subdirectory(src)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
    add_subdirectory(tools)
endif()

option(BUILD_EXAMPLES "Build the examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 设置默认安装路径为build目录下的install目录
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation path" FORCE)
message(STATUS "Installation prefix: ${CMAKE_INSTALL_PREFIX}")

# 生成版本头文件
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/version.h.in"
  "${CMAKE_BINARY_DIR}/include/version.h"
  @ONLY
)

# 安装版本头文件
install(FILES "${CMAKE_BINARY_DIR}/include/version.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 安装导出目标，使用CrolinKit::命名空间
install(EXPORT CrolinKitTargets
    FILE CrolinKitTargets.cmake
    NAMESPACE CrolinKit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CrolinKit
)
