# CrolinKit - 嵌入式系统开发工具包

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![AI-Assisted](https://img.shields.io/badge/AI--Assisted-100%25-purple.svg)](https://github.com/crolin-zhang/crolin-kit.git)

CrolinKit 是一个为嵌入式系统设计的多功能开发工具包，提供了一系列模块化的组件，包括线程池、日志系统等。该工具包采用纯C语言开发，基于POSIX标准，支持交叉编译，可以在各种嵌入式平台上运行。目前已完成线程模块与日志模块的开发和集成，正在进行内存管理模块的设计与实现。

> **注意**：这是一个完全借助AI（人工智能）进行开发的项目，从设计、编码到测试和文档编写，全过程由AI辅助完成。项目展示了AI辅助开发的能力和潜力。

## 功能特点

### 核心特性
- **简单易用的API** - 提供直观的函数接口，易于集成到现有项目
- **模块化设计** - 符合IPC SDK的目录结构，支持按需集成各个功能模块
- **交叉编译支持** - 支持MIPS等多种嵌入式平台，适用于各类嵌入式设备
- **完整测试套件** - 包含单元测试和集成测试，确保功能正确性
- **详细文档** - 提供全面的API文档、使用指南和架构说明

### 线程模块
- **高效的任务分配** - 使用线程池避免频繁创建和销毁线程的开销
- **任务状态监控** - 支持查询当前正在执行的任务名称和状态
- **线程安全** - 使用互斥锁和条件变量确保线程安全
- **优雅关闭** - 支持等待所有任务完成后再关闭线程池

### 日志模块
- **多级别日志** - 支持ERROR、WARNING、INFO、DEBUG、TRACE多级别日志
- **多输出目标** - 支持同时输出到控制台和文件
- **日志文件轮转** - 基于大小和时间的日志文件轮转功能
- **上下文信息** - 记录文件名、函数名、行号等上下文信息
- **格式化输出** - 支持丰富的格式化选项

### 开发中功能
- **内存管理** - 内存池、内存泄漏检测和内存使用统计（开发中）
- **IPC通信** - 进程间通信支持，包括共享内存、消息队列等（计划中）
- **网络通信** - 套接字通信、HTTP客户端/服务器支持（计划中）

## 项目结构

```
/
├─ CMakeLists.txt        # 顶层CMake构建文件
├─ .gitignore            # Git忽略文件
├─ cmake/                # CMake相关文件
│   ├─ templates/         # 模板文件
│   │   └─ version.h.in    # 版本头文件模板
│   └─ toolchains/        # 交叉编译工具链
│       └─ mips-linux-gnu.cmake # MIPS交叉编译工具链
├─ src/                  # 源代码目录
│   ├─ CMakeLists.txt    # 源代码构建文件
│   └─ core/              # 核心模块目录
│       ├─ CMakeLists.txt  # 核心模块构建文件
│       └─ thread/          # 线程模块
│           ├─ CMakeLists.txt # 线程模块构建文件
│           ├─ include/       # 公共API头文件目录
│           │   └─ thread.h    # 线程池API头文件
│           └─ src/           # 源代码目录
│               ├─ thread.c    # 线程池实现
│               └─ thread_internal.h # 内部结构和函数声明
├─ tools/                # 工具目录
│   └─ CMakeLists.txt    # 工具构建文件
├─ tests/                # 测试目录
│   ├─ CMakeLists.txt    # 测试构建文件
│   ├─ test_thread_pool.c # 测试程序
│   └─ modules/           # 模块测试目录
│       ├─ CMakeLists.txt  # 模块测试构建文件
│       └─ thread/          # 线程模块测试
│           ├─ CMakeLists.txt # 线程模块测试构建文件
│           └─ src/           # 线程模块测试源代码
│               ├─ CMakeLists.txt # 测试源代码构建文件
│               └─ thread_unit_test.c # 线程模块单元测试
├─ examples/             # 示例目录
│   ├─ CMakeLists.txt    # 示例构建文件
│   ├─ thread_pool_example.c # 示例程序
│   └─ thread/            # 线程模块示例
│       ├─ CMakeLists.txt  # 线程模块示例构建文件
│       └─ thread_example.c # 线程模块示例程序
├─ tools/                # 工具目录
│   ├─ CMakeLists.txt    # 工具构建文件
│   └─ build/             # 构建工具
│       ├─ CMakeLists.txt  # 构建工具构建文件
│       └─ version_template.h.in # 版本信息模板
├─ cmake/                # CMake模块目录
│   └─ toolchains/         # 交叉编译工具链目录
│       └─ mips-linux-gnu.cmake # MIPS交叉编译工具链配置
└─ docs/                 # 文档目录
    ├─ README.md          # 文档目录概述
    ├─ project_overview.md # 项目概述
    ├─ architecture.md     # 架构设计
    ├─ api_reference.md    # API参考
    ├─ user_guide.md       # 用户指南
    └─ test_report.md      # 测试报告
```

## 构建和安装

### 前提条件

- CMake 3.8 或更高版本
- C 编译器 (GCC 或 Clang)
- POSIX 线程支持 (pthread)

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/crolin-zhang/crolin-kit.git
cd crolin-kit

# 创建构建目录
mkdir build
cd build

# 配置和构建
cmake ..
make

# 运行测试
make test

# 安装 (默认安装到build/install目录)
make install
```

默认情况下，库将被安装到 `build/install` 目录下。安装后的目录结构如下：

```
build/install/
├─ include/           # 头文件目录
│   ├─ thread.h       # 线程池API头文件
│   └─ version.h      # 版本信息头文件
└─ lib/               # 库文件目录
    ├─ cmake/           # CMake配置文件
    │   └─ CrolinKit/    # 项目配置
    │       ├─ CrolinKitTargets.cmake
    │       └─ CrolinKitTargets-noconfig.cmake
    └─ libthread.a     # 线程池库
```

如果你想安装到系统目录，可以使用：

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make install
```

## 快速开始

### 编译与运行

使用CMake构建系统：

#### 本地编译

```bash
# 在项目根目录下创建构建目录
mkdir -p build && cd build

# 仅构建线程池库
cmake ..
make

# 或者，构建库、测试和示例
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make

# 运行测试程序
./tests/test_thread_pool

# 运行模块测试
./tests/modules/thread/src/thread_unit_test

# 运行示例程序
./examples/thread_pool_example
./examples/thread/thread_example
```

#### 交叉编译（以MIPS为例）

```bash
# 在项目根目录下创建构建目录
mkdir -p build-mips && cd build-mips

# 使用MIPS交叉编译工具链配置
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mips-linux-gnu.cmake

# 构建库
make

# 或者，指定编译器路径
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mips-linux-gnu.cmake \
         -DCMAKE_C_COMPILER=/path/to/mips-linux-gnu-gcc \
         -DCMAKE_CXX_COMPILER=/path/to/mips-linux-gnu-g++
make
```

### 安装

如果你想将线程池库安装到系统中：

```bash
# 在build目录中执行
sudo make install
```

### 与其他项目集成

如果你想将CrolinKit作为子目录集成到其他CMake项目中，可以在主项目的CMakeLists.txt中添加：

```cmake
# 添加CrolinKit目录
add_subdirectory(path/to/crolin-kit)

# 链接线程模块
target_link_libraries(your_target PRIVATE thread)

# 包含头文件目录
target_include_directories(your_target PRIVATE 
    ${path/to/crolin-kit}/src/core/thread/include
)
```

或者，如果你已经安装了CrolinKit，可以使用find_package：

```cmake
# 查找CrolinKit包
find_package(CrolinKit REQUIRED)

# 链接线程模块
target_link_libraries(your_target PRIVATE CrolinKit::thread)
```

### 基本用法

```c
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// 定义任务函数
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    free(arg); // 释放参数
}

int main() {
    // 创建线程池，包含4个工作线程
    thread_pool_t pool = thread_pool_create(4);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 添加任务到线程池
    for (int i = 0; i < 10; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i + 1;
        char task_name[64];
        sprintf(task_name, "任务-%d", i + 1);
        
        if (thread_pool_add_task(pool, my_task, arg, task_name) != 0) {
            fprintf(stderr, "添加任务失败\n");
            free(arg);
        }
    }
    
    // 获取当前正在执行的任务名称
    char **tasks = thread_pool_get_running_task_names(pool);
    if (tasks != NULL) {
        for (int i = 0; i < 4; i++) {
            printf("线程 %d: %s\n", i, tasks[i]);
        }
        free_running_task_names(tasks, 4);
    }
    
    // 等待任务完成（简化示例）
    sleep(5);
    
    // 销毁线程池
    thread_pool_destroy(pool);
    
    return 0;
}
```

## API参考

### 创建线程池

```c
thread_pool_t thread_pool_create(int num_threads);
```

创建一个包含指定数量工作线程的线程池。

### 添加任务

```c
int thread_pool_add_task(thread_pool_t pool, void (*function)(void *), void *arg, const char *task_name);
```

向线程池添加一个新任务。

### 获取任务状态

```c
char **thread_pool_get_running_task_names(thread_pool_t pool);
```

获取当前正在执行的任务名称列表。

### 释放任务名称

```c
void free_running_task_names(char **task_names, int count);
```

释放由`thread_pool_get_running_task_names`返回的资源。

### 销毁线程池

```c
int thread_pool_destroy(thread_pool_t pool);
```

销毁线程池，释放所有资源。

## 开发计划

### 里程碑进展

#### 里程碑 1: 基础模块完成 ✔️
- [x] 实现线程池模块的基本功能
- [x] 实现日志模块的基本功能
- [x] 线程模块与日志模块集成
- [x] 编写基本测试和示例应用

#### 里程碑 2: 资源管理模块 (进行中)
- [ ] 实现内存管理模块
- [ ] 增强日志模块功能
- [ ] 添加性能监控和统计
- [ ] 完善单元测试和文档

#### 里程碑 3: 进程间通信和高级线程功能 (计划中)
- [ ] 实现IPC模块
- [ ] 增强线程模块（优先级、动态调整、取消机制）
- [ ] 集成内存管理和日志模块到其他模块

#### 里程碑 4: 网络通信和生产就绪 (计划中)
- [ ] 实现网络通信模块
- [ ] 支持高级任务功能（依赖关系、结果返回）
- [ ] 全面的性能优化
- [ ] 完整的文档和测试套件

### 已完成功能

#### 线程模块
- [x] 线程池创建与销毁
- [x] 任务队列管理
- [x] 线程同步机制
- [x] 任务状态监控
- [x] 与日志模块集成

#### 日志模块
- [x] 多级别日志记录
- [x] 日志格式化输出
- [x] 日志文件输出与轮转
- [x] 日志回调功能

#### 项目基础设施
- [x] 模块化目录结构
- [x] CMake构建系统
- [x] 交叉编译支持
- [x] 单元测试框架

### 进行中的工作

- [ ] **内存管理模块设计与实现**
  - [ ] 内存池功能
  - [ ] 内存泄漏检测
  - [ ] 内存使用统计

- [ ] **日志模块功能增强**
  - [ ] 日志过滤功能
  - [ ] 可配置的格式化选项
  - [ ] 性能优化

- [ ] **测试与性能**
  - [ ] 完善单元测试覆盖率
  - [ ] 性能基准测试
  - [ ] 内存泄漏测试

### 计划中的功能

- [ ] **线程模块增强**
  - [ ] 任务优先级支持
  - [ ] 任务完成通知机制
  - [ ] 动态调整线程池大小
  - [ ] 任务超时处理

- [ ] **高级模块**
  - [ ] IPC（进程间通信）模块
  - [ ] 网络通信模块
  - [ ] 异步IO支持

## 贡献

欢迎提交问题报告和功能请求。如果您想贡献代码，请先创建一个issue讨论您的想法。

## 作者

- [@crolin-zhang](https://github.com/crolin-zhang)

## 许可证

本项目采用MIT许可证 - 详见LICENSE文件
