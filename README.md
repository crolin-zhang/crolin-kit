# CrolinKit - 嵌入式系统开发工具包

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![AI-Assisted](https://img.shields.io/badge/AI--Assisted-100%25-purple.svg)](https://github.com/crolin-zhang/crolin-kit.git)

CrolinKit 是一个为嵌入式系统设计的多功能开发工具包，采用纯C语言开发，支持交叉编译。当前已实现的核心模块包括线程池和日志系统。

> **注意**：这是一个完全借助AI（人工智能）进行开发的项目，从设计、编码到测试和文档编写，全过程由AI辅助完成。项目展示了AI辅助开发的能力和潜力。

## 功能特点

- **模块化设计** - 支持按需集成各个功能模块
- **交叉编译支持** - 适用于各类嵌入式平台
- **完整测试套件** - 确保功能正确性

### 已实现模块

- **线程模块** - 高效线程池实现，支持任务优先级、动态调整和状态监控
- **日志模块** - 多级别日志记录(INFO/DEBUG/TRACE)，支持控制台和文件输出

### 计划中的模块

- **内存管理** - 内存池和泄漏检测
- **IPC通信** - 进程间通信支持
- **网络通信** - 套接字和HTTP支持

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
│   └─ modules/           # 模块测试目录
│       ├─ CMakeLists.txt  # 模块测试构建文件
│       └─ thread/          # 线程模块测试
│           ├─ CMakeLists.txt # 线程模块测试构建文件
│           └─ src/           # 线程模块测试源代码
│               ├─ CMakeLists.txt # 测试源代码构建文件
│               ├─ thread_unit_test.c # 基本功能测试
│               ├─ thread_resize_test.c # 线程池大小调整测试
│               ├─ thread_pool_test.c # 线程池综合测试
│               ├─ thread_debug_test.c # 线程池调试测试
│               └─ thread_auto_adjust.c # 线程池自动调整测试
├─ examples/             # 示例目录
│   ├─ CMakeLists.txt    # 示例构建文件
│   ├─ thread_pool_example.c # 示例程序
│   └─ thread/            # 线程模块示例
│       ├─ CMakeLists.txt  # 线程模块示例构建文件
│       ├─ thread_example.c # 基本功能示例程序
│       ├─ thread_resize_example.c # 线程池大小调整示例
│       ├─ thread_auto_adjust_example.c # 线程池自动调整示例
│       ├─ thread_task_names_example.c # 线程池任务名称查询示例
│       └─ thread_pool_example.c # 线程池综合示例
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

### 开发环境

项目已配置好开发环境支持，包括对VSCode和Clangd的集成。详细的IDE配置和使用指南请参考[IDE集成文档](docs/ide_integration.md)。

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

# 运行模块测试
./tests/modules/thread/src/thread_unit_test    # 基本功能测试
./tests/modules/thread/src/thread_resize_test  # 线程池大小调整测试
./tests/modules/thread/src/thread_pool_test    # 线程池综合测试
./tests/modules/thread/src/thread_debug_test   # 线程池调试测试
./tests/modules/thread/src/thread_auto_adjust  # 线程池自动调整测试

# 运行示例程序
./examples/thread_pool_example
./examples/thread/thread_example
./examples/thread/thread_resize_example
./examples/thread/thread_auto_adjust_example
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

## 模块优化计划

### 线程池模块优化

当前线程池模块已经实现了以下功能：

- 基础线程池功能（创建、销毁、添加任务）
- 任务优先级支持（高、中、低、后台四个优先级级别）
- 动态线程数调整（手动或自动）
- 线程池状态监控与统计
- 运行任务名称查询（实时获取每个线程正在执行的任务名称）
- 线程池限制设置（最小/最大线程数）
- 优化的日志级别结构（INFO/DEBUG/TRACE），可通过环境变量控制

每个功能都提供了相应的示例程序，例如：
- `thread_example.c` - 基础线程池功能示例
- `thread_resize_example.c` - 动态调整线程池大小示例
- `thread_auto_adjust_example.c` - 自动调整线程池大小示例
- `thread_task_names_example.c` - 获取运行中任务名称示例
- `thread_priority_example.c` - 任务优先级功能示例
- `thread_pool_example.c` - 线程池综合示例，展示多种功能的组合使用

#### 近期功能增强和修复内容

- **任务优先级实现**：
  - 添加四级任务优先级（高、中、低、后台）
  - 优化任务队列，确保高优先级任务优先执行
  - 提供简洁的API接口，便于设置任务优先级
  - 添加任务优先级示例程序，展示不同优先级任务的执行顺序

- **日志级别优化**：
  - 重构线程池日志级别结构，减少默认输出量
  - 将详细操作日志从INFO级别移至DEBUG级别
  - 保留重要状态变化（如线程池创建、销毁）在INFO级别
  - 支持通过LOG_LEVEL环境变量控制日志输出详细程度
  - 添加TRACE级别日志，提供最详细的内部状态和流程信息

- **代码质量改进**：
  - 修复内存管理问题：解决`thread_pool_resize`中`realloc`失败可能导致的内存泄漏
  - 增强线程创建失败时的资源回收和状态恢复
  - 改进`thread_pool_get_running_task_names`函数中的安全性检查
  - 添加完善的边界检查，防止缓冲区溢出和空指针访问
  - 修复结构体命名问题，确保命名一致性

- **测试优化**：
  - 优化线程池测试程序，增强随机化和稳定性
  - 随机化线程池初始大小（2-5个线程）
  - 随机化任务执行时间（短任务10-50ms，长任务200-800ms）
  - 随机化等待时间和超时设置
  - 改进错误处理，从简单assert改为详细的错误检查和报告
  - 实现自动退出机制，确保测试不会挂起
  - 修复线程池自动调整测试的稳定性问题

#### 计划中的功能增强

1. **任务取消机制**：支持取消尚未开始执行的任务
2. **任务超时控制**：为任务设置最大执行时间，超时自动中断
3. **性能监控**：添加线程池性能统计功能，如平均任务等待时间、执行时间等
4. **线程亲和性**：支持设置线程CPU亲和性，提高缓存命中率
5. **内存优化**：减少任务队列的内存分配次数，使用内存池技术
6. **线程池健康追踪**：提供详细的运行时状态信息和活动日志
7. **故障自恢复**：自动检测和处理线程崩溃或卡死情况
8. **垃圾回收机制**：处理长时间未完成或遗弃的任务
9. **安全取消点**：支持在任务执行期间添加取消点，允许安全终止

### 日志模块优化

日志模块可以在以下方面进行改进：

1. **异步日志**：实现异步日志写入，减少对主线程的阻塞
2. **日志压缩**：支持自动压缩和归档历史日志文件
3. **结构化日志**：支持JSON等结构化日志格式，便于日志分析
4. **日志过滤**：实现更灵活的日志过滤机制，支持正则表达式过滤
5. **远程日志**：支持将日志发送到远程服务器或日志聚合系统
6. **上下文跟踪**：实现日志上下文跟踪，便于跟踪多线程环境中的调用链
7. **性能优化**：减少日志格式化的开销，优化文件I/O操作

详细API文档请参考[API参考文档](docs/api_reference.md)。

## 开发计划

- **已完成**: 线程池和日志模块
- **进行中**: 内存管理模块
- **计划中**: IPC通信和网络模块

详细路线图请参考[project_overview.md](docs/project_overview.md)

## 贡献

欢迎提交问题报告和功能请求。如果您想贡献代码，请先创建一个issue讨论您的想法。

## 作者

- [@crolin-zhang](https://github.com/crolin-zhang)

## 许可证

本项目采用MIT许可证 - 详见LICENSE文件
