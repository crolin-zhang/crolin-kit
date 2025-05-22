# C线程池库 (C Thread Pool Library)

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![AI-Assisted](https://img.shields.io/badge/AI--Assisted-100%25-purple.svg)](https://github.com/crolin-zhang/crolin-zhang)

一个高性能、可靠的C语言线程池库，提供简洁的API接口，使开发者能够轻松地在应用程序中实现并发任务处理。该库已重构为符合IPC SDK结构的模块化设计。

> **注意**：这是一个完全借助AI（人工智能）进行开发的项目，从设计、编码到测试和文档编写，全过程由AI辅助完成。项目展示了AI辅助开发的能力和潜力。

## 功能特点

- **简单易用的API** - 提供直观的函数接口，易于集成
- **高效的任务分配** - 使用线程池避免频繁创建和销毁线程的开销
- **任务状态监控** - 支持查询当前正在执行的任务
- **线程安全** - 使用互斥锁和条件变量确保线程安全
- **详细的错误处理** - 提供全面的错误检查和日志记录
- **内存管理** - 细致的内存分配与释放，防止内存泄漏
- **模块化设计** - 符合IPC SDK的目录结构，易于集成
- **完整测试套件** - 包含验证功能正确性的测试程序

## 项目结构

```
/
├── CMakeLists.txt        # 顶层CMake构建文件
├── thread/               # 线程池库核心实现
│   ├── CMakeLists.txt    # 线程池库构建文件
│   ├── include/
│   │   └── thread.h         # 公共API头文件
│   └── src/
│       ├── thread.c         # 线程池实现
│       └── thread_internal.h # 内部结构和函数声明
├── tests/                # 测试目录
│   ├── CMakeLists.txt    # 测试构建文件
│   └── test_thread_pool.c # 测试程序
├── examples/             # 示例目录
│   ├── CMakeLists.txt    # 示例构建文件
│   └── thread_pool_example.c # 示例程序
└── docs/                 # 文档目录
    ├── README.md          # 文档目录概述
    ├── project_overview.md # 项目概述
    ├── architecture.md     # 架构设计
    ├── api_reference.md    # API参考
    ├── user_guide.md       # 用户指南
    └── test_report.md      # 测试报告
```

## 快速开始

### 编译与运行

使用CMake构建系统：

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

# 运行示例程序
./examples/thread_pool_example
```

### 安装

如果你想将线程池库安装到系统中：

```bash
# 在build目录中执行
sudo make install
```

### 与其他项目集成

如果你想将线程池库作为子目录集成到其他CMake项目中，可以在主项目的CMakeLists.txt中添加：

```cmake
add_subdirectory(path/to/thread_pool_library)
target_link_libraries(your_target PRIVATE thread)
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

- [x] 基础线程池功能实现
- [x] 任务队列管理
- [x] 线程同步机制
- [x] 任务状态监控
- [x] 重构为符合IPC SDK的目录结构
- [x] 添加测试套件
- [ ] 任务优先级支持
- [ ] 任务完成通知机制
- [ ] 动态调整线程池大小
- [ ] 性能监控与统计

## 贡献

欢迎提交问题报告和功能请求。如果您想贡献代码，请先创建一个issue讨论您的想法。

## 作者

- [@crolin-zhang](https://github.com/crolin-zhang)

## 许可证

本项目采用MIT许可证 - 详见LICENSE文件
