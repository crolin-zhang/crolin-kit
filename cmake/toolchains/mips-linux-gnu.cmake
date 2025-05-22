# MIPS交叉编译工具链配置
# 使用方法: cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mips-linux-gnu.cmake ..

# 系统名称
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR mips)

# 指定交叉编译器路径
# 注意：根据实际安装路径进行调整
set(TOOLCHAIN_PREFIX mips-linux-gnu)

# 交叉编译器路径
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)

# 其他工具
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}-objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}-size CACHE INTERNAL "size tool")
set(CMAKE_STRIP ${TOOLCHAIN_PREFIX}-strip CACHE INTERNAL "strip tool")
set(CMAKE_AR ${TOOLCHAIN_PREFIX}-ar CACHE INTERNAL "ar tool")
set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}-ld CACHE INTERNAL "linker tool")
set(CMAKE_NM ${TOOLCHAIN_PREFIX}-nm CACHE INTERNAL "nm tool")
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}-objdump CACHE INTERNAL "objdump tool")
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}-ranlib CACHE INTERNAL "ranlib tool")

# 寻找程序时，只在目标环境中查找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# 寻找库和头文件时，只在目标环境中查找
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 编译器标志
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC" CACHE INTERNAL "c compiler flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC" CACHE INTERNAL "cxx compiler flags")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}" CACHE INTERNAL "asm compiler flags")

# 链接器标志
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections" CACHE INTERNAL "exe link flags")

# 定义MIPS特定的编译选项
add_compile_options(
    -march=mips32r2
    -mabi=32
    -mhard-float
    -mno-check-zero-division
)

# 设置交叉编译标志
add_definitions(-DCROSS_COMPILE)
add_definitions(-DMIPS_PLATFORM)

# 调试信息
message(STATUS "MIPS toolchain loaded")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "Linker flags: ${CMAKE_EXE_LINKER_FLAGS}")
