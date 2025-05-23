#!/bin/bash

# CrolinKit 代码格式化脚本
# 使用clang-format格式化项目中的所有C/C++文件

# 项目根目录
PROJECT_ROOT="$(pwd)"

# 检查clang-format是否安装
if ! command -v clang-format &> /dev/null; then
    echo "错误: clang-format未安装，请先安装clang-format"
    echo "可以使用: sudo apt-get install clang-format"
    exit 1
fi

# 格式化单个文件的函数
format_file() {
    local file="$1"
    echo "正在格式化: $file"
    clang-format -i -style=file "$file"
    if [ $? -ne 0 ]; then
        echo "警告: 格式化 $file 失败"
        return 1
    fi
    return 0
}

# 如果提供了文件参数，只格式化该文件
if [ $# -eq 1 ]; then
    if [ ! -f "$1" ]; then
        echo "错误: 文件 '$1' 不存在"
        exit 1
    fi
    format_file "$1"
    if [ $? -eq 0 ]; then
        echo "成功格式化文件: $1"
    else
        echo "格式化文件失败: $1"
        exit 1
    fi
    exit 0
fi

# 格式化所有C/C++文件
echo "开始格式化CrolinKit项目中的所有C/C++文件..."

# 计数器
total_files=0
success_files=0
failed_files=0

# 查找并格式化所有.c和.h文件
find "$PROJECT_ROOT" \( -path "*/build/*" -o -path "*/\.*" \) -prune -o \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) -type f -print | while read -r file; do
    total_files=$((total_files + 1))
    format_file "$file"
    if [ $? -eq 0 ]; then
        success_files=$((success_files + 1))
    else
        failed_files=$((failed_files + 1))
    fi
done

# 输出统计信息
echo "格式化完成!"
echo "总文件数: $total_files"
echo "成功格式化: $success_files"
echo "格式化失败: $failed_files"

if [ $failed_files -gt 0 ]; then
    echo "警告: 有 $failed_files 个文件格式化失败"
    exit 1
fi

echo "所有文件已成功格式化!"
exit 0
