# 线程池库使用指南

本指南将帮助您从安装到高级用法，全面了解如何使用线程池库。

## 目录

- [安装与编译](#安装与编译)
- [基本用法](#基本用法)
- [线程池动态调整](#线程池动态调整)
- [线程池自动调整](#线程池自动调整)
- [高级用法](#高级用法)
- [最佳实践](#最佳实践)
- [常见问题](#常见问题)
- [故障排除](#故障排除)

## 安装与编译

### 前提条件

使用线程池库需要以下依赖：

- C编译器（如GCC或Clang）
- CMake 3.8或更高版本
- POSIX线程库（pthread）

### 编译库

#### 方法1：使用CMake构建系统

```bash
# 在项目根目录下创建构建目录
mkdir -p build && cd build

# 仅构建线程池库
cmake ..
make

# 或者，构建库、测试和示例
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make
```

#### 方法2：手动编译

如果您不想使用CMake，也可以手动编译库：

```bash
# 编译库
gcc -c -Wall -pthread -o thread.o thread/src/thread.c -Ithread/include
ar rcs libthread.a thread.o

# 编译示例程序
gcc -Wall -pthread -o thread_pool_example examples/thread_pool_example.c libthread.a -Ithread/include
```

### 安装库

如果您想将线程池库安装到系统中：

```bash
# 在build目录中执行
sudo make install
```

这将安装库文件和头文件到系统目录，通常是`/usr/local/lib`和`/usr/local/include`。

### 与其他项目集成

#### 作为子目录集成

如果您想将线程池库作为子目录集成到其他CMake项目中，可以在主项目的CMakeLists.txt中添加：

```cmake
add_subdirectory(path/to/thread)
target_link_libraries(your_target PRIVATE thread)
```

#### 作为外部库使用

如果您已经安装了线程池库，可以在其他项目中使用：

```cmake
find_package(Thread REQUIRED)
target_link_libraries(your_target PRIVATE Thread::Thread)
```

## 基本用法

### 包含头文件

```c
#include <thread.h>
```

如果您想启用调试日志，可以在包含头文件之前定义`DEBUG_THREAD_POOL`宏：

```c
#define DEBUG_THREAD_POOL
#include <thread.h>
```

### 创建线程池

```c
// 创建一个包含4个工作线程的线程池
thread_pool_t pool = thread_pool_create(4);
if (pool == NULL) {
    fprintf(stderr, "创建线程池失败\n");
    return 1;
}
```

### 定义任务函数

任务函数必须符合以下签名：

```c
void task_function(void *arg);
```

例如：

```c
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    // 执行任务逻辑
    free(arg); // 释放参数
}
```

### 添加任务到线程池

```c
// 分配参数内存
int *arg = malloc(sizeof(int));
*arg = 1;

// 添加任务到线程池
if (thread_pool_add_task(pool, my_task, arg, "Task-1") != 0) {
    fprintf(stderr, "添加任务失败\n");
    free(arg);
}
```

### 获取任务状态

```c
// 获取当前正在运行的任务名称
char **running_tasks = thread_pool_get_running_task_names(pool);
if (running_tasks) {
    for (int i = 0; i < 4; i++) { // 假设线程池有4个线程
        printf("线程 %d: %s\n", i, running_tasks[i]);
    }
    // 释放资源
    free_running_task_names(running_tasks, 4);
}
```

### 销毁线程池

```c
// 销毁线程池
if (thread_pool_destroy(pool) != 0) {
    fprintf(stderr, "销毁线程池失败\n");
    return 1;
}
```

## 线程池动态调整

线程池库支持在运行时动态调整线程数量，以适应不同的负载情况。

### 调整线程池大小

```c
// 将线程池大小从4增加到8
// 当任务负载增加时，可以增加线程数量
// 注意：新线程数量必须大于0

if (thread_pool_resize(pool, 8) != 0) {
    fprintf(stderr, "调整线程池大小失败\n");
}

// 当负载减少时，可以减少线程数量以节省资源
if (thread_pool_resize(pool, 2) != 0) {
    fprintf(stderr, "调整线程池大小失败\n");
}
```

### 获取线程池统计信息

```c
// 获取线程池的当前统计信息
thread_pool_stats_t stats;
if (thread_pool_get_stats(pool, &stats) == 0) {
    printf("线程数量: %d\n", stats.thread_count);
    printf("活跃线程: %d\n", stats.active_threads);
    printf("等待任务: %d\n", stats.queued_tasks);
    printf("已完成任务: %d\n", stats.completed_tasks);
    printf("自动调整: %s\n", stats.auto_adjust_enabled ? "已启用" : "已禁用");
}
```

### 动态调整线程池大小的完整示例

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread.h>

// 定义任务函数
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    sleep(1); // 模拟工作
    free(arg); // 释放参数
}

int main(void) {
    // 创建线程池，初始化为2个工作线程
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 获取初始统计信息
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("初始线程数量: %d\n", stats.thread_count);
    }
    
    // 添加一些任务
    for (int i = 0; i < 5; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i + 1;
        char task_name[64];
        sprintf(task_name, "任务-%d", i + 1);
        
        if (thread_pool_add_task(pool, my_task, arg, task_name) != 0) {
            fprintf(stderr, "添加任务失败\n");
            free(arg);
        }
    }
    
    // 等待一些任务开始执行
    sleep(1);
    
    // 增加线程数量到4
    printf("\n增加线程数量到4...\n");
    if (thread_pool_resize(pool, 4) != 0) {
        fprintf(stderr, "调整线程池大小失败\n");
    }
    
    // 获取调整后的统计信息
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("调整后线程数量: %d\n", stats.thread_count);
    }
    
    // 添加更多任务
    for (int i = 5; i < 10; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i + 1;
        char task_name[64];
        sprintf(task_name, "任务-%d", i + 1);
        
        if (thread_pool_add_task(pool, my_task, arg, task_name) != 0) {
            fprintf(stderr, "添加任务失败\n");
            free(arg);
        }
    }
    
    // 等待任务完成
    sleep(3);
    
    // 减少线程数量回到2
    printf("\n减少线程数量回到2...\n");
    if (thread_pool_resize(pool, 2) != 0) {
        fprintf(stderr, "调整线程池大小失败\n");
    }
    
    // 获取最终统计信息
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("最终线程数量: %d\n", stats.thread_count);
        printf("已完成任务: %d\n", stats.completed_tasks);
    }
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    return 0;
}
```

## 线程池自动调整

线程池库支持自动调整功能，可以根据负载自动调整线程数量，提高资源利用率。

### 启用自动调整

```c
// 启用自动调整功能
// 参数说明：
// 1. 线程池句柄
// 2. 最小线程数（必须大于0）
// 3. 最大线程数（必须大于等于最小线程数）
// 4. 检查间隔（毫秒）

if (thread_pool_enable_auto_adjust(pool, 2, 8, 1000) != 0) {
    fprintf(stderr, "启用线程池自动调整失败\n");
}
```

### 禁用自动调整

```c
// 禁用自动调整功能
if (thread_pool_disable_auto_adjust(pool) != 0) {
    fprintf(stderr, "禁用线程池自动调整失败\n");
}
```

### 自动调整的工作原理

线程池自动调整功能基于以下原理进行调整：

1. 当任务队列中的任务数量超过当前线程数量时，会增加线程数量，但不超过指定的最大线程数。
2. 当任务队列中的任务数量很少，并且活跃线程数量低于当前线程数量的一半时，会减少线程数量，但不低于指定的最小线程数。
3. 调整过程是渐进的，每次只增加或减少一定比例的线程，以避免突然的大幅调整。

### 自动调整的完整示例

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <thread.h>

// 全局变量用于优雅退出
volatile sig_atomic_t g_shutdown_requested = 0;

// 信号处理函数
void signal_handler(int signum) {
    g_shutdown_requested = 1;
}

// 定义任务函数
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    sleep(1); // 模拟工作
    free(arg); // 释放参数
}

int main(void) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    // 创建线程池，初始化为2个工作线程
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 启用自动调整功能，线程数量在2到8之间，每500毫秒检查一次
    if (thread_pool_enable_auto_adjust(pool, 2, 8, 500) != 0) {
        fprintf(stderr, "启用线程池自动调整失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    
    printf("线程池初始化完成，开始添加任务...\n");
    
    // 获取初始统计信息
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("初始线程数量: %d\n", stats.thread_count);
        printf("自动调整: %s\n", stats.auto_adjust_enabled ? "已启用" : "已禁用");
        printf("最小线程数: %d\n", stats.min_threads);
        printf("最大线程数: %d\n", stats.max_threads);
    }
    
    // 循环添加任务，直到收到退出信号
    int task_counter = 0;
    int batch_size = 1; // 初始批量大小
    
    while (!g_shutdown_requested) {
        printf("\n添加%d个任务...\n", batch_size);
        
        // 添加任务到线程池
        for (int i = 0; i < batch_size; i++) {
            int *arg = malloc(sizeof(int));
            if (arg == NULL) {
                fprintf(stderr, "内存分配失败\n");
                continue;
            }
            *arg = ++task_counter;
            char task_name[64];
            sprintf(task_name, "任务-%d", task_counter);
            
            if (thread_pool_add_task(pool, my_task, arg, task_name) != 0) {
                fprintf(stderr, "添加任务失败\n");
                free(arg);
            }
        }
        
        // 获取当前线程池统计信息
        if (thread_pool_get_stats(pool, &stats) == 0) {
            printf("\n当前线程池状态:\n");
            printf("线程数量: %d\n", stats.thread_count);
            printf("活跃线程: %d\n", stats.active_threads);
            printf("等待任务: %d\n", stats.queued_tasks);
            printf("已完成任务: %d\n", stats.completed_tasks);
        }
        
        // 每隔一段时间增加批量大小，模拟负载变化
        if (task_counter % 20 == 0) {
            batch_size = (batch_size % 10) + 1; // 循环变化批量大小，1-10之间
        }
        
        // 暂停一段时间
        printf("按Ctrl+C退出...\n");
        sleep(2);
    }
    
    printf("\n收到退出信号，开始关闭线程池...\n");
    
    // 禁用自动调整
    if (thread_pool_disable_auto_adjust(pool) != 0) {
        fprintf(stderr, "禁用线程池自动调整失败\n");
    }
    
    // 获取最终统计信息
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("\n最终线程池状态:\n");
        printf("线程数量: %d\n", stats.thread_count);
        printf("活跃线程: %d\n", stats.active_threads);
        printf("等待任务: %d\n", stats.queued_tasks);
        printf("已完成任务: %d\n", stats.completed_tasks);
        printf("自动调整: %s\n", stats.auto_adjust_enabled ? "已启用" : "已禁用");
    }
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    printf("线程池已成功关闭\n");
    return 0;
}
```

### 完整示例

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread.h>

// 定义任务函数
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    sleep(1); // 模拟工作
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
    
    // 等待任务完成
    sleep(5);
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    return 0;
}
```

## 高级用法

### 任务命名和识别

线程池库支持任务命名，这使得可以跟踪、查找和取消特定任务。任务命名的主要功能包括：

#### 1. 命名任务

在添加任务时，可以为任务指定一个描述性名称：

```c
thread_pool_add_task(pool, my_task, arg, "data_processing_task", TASK_PRIORITY_NORMAL);
```

#### 2. 未命名任务的自动命名

从版本1.2.0开始，当添加未命名任务（传递NULL作为任务名称）时，线程池将自动生成一个唯一的名称：

```c
// 添加未命名任务，将自动生成格式为"unnamed_task_{task_id}"的唯一名称
thread_pool_add_task(pool, my_task, arg, NULL, TASK_PRIORITY_NORMAL);
```

自动生成的名称格式为`"unnamed_task_{task_id}"`，其中`{task_id}`是任务的唯一标识符。这个功能在以下场景特别有用：

- 当你不需要显式命名每个任务，但仍需要在日志或状态报告中识别它们
- 当你需要在不知道确切名称的情况下取消任务
- 当你需要跟踪大量自动生成的任务

#### 3. 通过名称查找任务

可以使用`thread_pool_find_task_by_name`函数根据名称查找任务：

```c
int is_queued;
int task_found = thread_pool_find_task_by_name(pool, "data_processing_task", &is_queued);
if (task_found) {
    printf("找到任务，%s\n", is_queued ? "任务在队列中" : "任务正在运行");
}

// 也可以查找自动生成名称的任务
int unnamed_task_found = thread_pool_find_task_by_name(pool, "unnamed_task_42", &is_queued);
```

#### 4. 通过名称取消任务

可以使用`thread_pool_cancel_task`函数通过名称取消任务：

```c
int cancelled = thread_pool_cancel_task(pool, "data_processing_task", cancel_callback);
printf("取消任务%s\n", cancelled ? "成功" : "失败");

// 也可以取消自动生成名称的任务
int unnamed_cancelled = thread_pool_cancel_task(pool, "unnamed_task_42", cancel_callback);
```

### 任务参数管理

在线程池中，任务参数的内存管理非常重要。有几种常见的参数传递方式：

#### 1. 动态分配参数

```c
void task_with_dynamic_arg(void *arg) {
    int *value = (int*)arg;
    // 使用参数
    printf("值: %d\n", *value);
    // 任务完成后释放参数
    free(arg);
}

// 调用时
int *arg = malloc(sizeof(int));
*arg = 42;
thread_pool_add_task(pool, task_with_dynamic_arg, arg, "动态参数任务");
```

#### 2. 使用结构体传递多个参数

```c
typedef struct {
    int id;
    char *name;
    double value;
} task_params_t;

void task_with_struct_arg(void *arg) {
    task_params_t *params = (task_params_t*)arg;
    // 使用参数
    printf("ID: %d, 名称: %s, 值: %f\n", params->id, params->name, params->value);
    // 如果需要，释放结构体内的动态分配内存
    free(params->name);
    // 释放结构体本身
    free(params);
}

// 调用时
task_params_t *params = malloc(sizeof(task_params_t));
params->id = 1;
params->name = strdup("测试任务");
params->value = 3.14;
thread_pool_add_task(pool, task_with_struct_arg, params, "结构体参数任务");
```

#### 3. 使用静态或全局数据

```c
// 全局或静态数据
static int global_data[10];
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

void task_with_shared_data(void *arg) {
    int index = *(int*)arg;
    
    // 安全访问共享数据
    pthread_mutex_lock(&data_mutex);
    global_data[index]++;
    pthread_mutex_unlock(&data_mutex);
    
    free(arg);
}

// 调用时
int *index = malloc(sizeof(int));
*index = 5;
thread_pool_add_task(pool, task_with_shared_data, index, "共享数据任务");
```

### 任务依赖管理

虽然线程池库本身不直接支持任务依赖，但您可以通过以下方式实现：

#### 1. 使用回调链

```c
typedef struct {
    thread_pool_t pool;
    void *next_arg;
    char next_task_name[MAX_TASK_NAME_LEN];
} callback_data_t;

void first_task(void *arg) {
    callback_data_t *data = (callback_data_t*)arg;
    
    // 执行第一个任务的逻辑
    printf("执行第一个任务\n");
    
    // 添加下一个任务到线程池
    thread_pool_add_task(data->pool, second_task, data->next_arg, data->next_task_name);
    
    // 释放当前任务的数据
    free(data);
}

void second_task(void *arg) {
    // 执行第二个任务的逻辑
    printf("执行第二个任务\n");
    free(arg);
}

// 调用时
callback_data_t *data = malloc(sizeof(callback_data_t));
data->pool = pool;
data->next_arg = malloc(sizeof(int)); // 为第二个任务准备参数
*(int*)(data->next_arg) = 42;
strncpy(data->next_task_name, "第二个任务", MAX_TASK_NAME_LEN - 1);

thread_pool_add_task(pool, first_task, data, "第一个任务");
```

#### 2. 使用计数器和条件变量

```c
typedef struct {
    int *counter;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
} sync_data_t;

void prerequisite_task(void *arg) {
    sync_data_t *data = (sync_data_t*)arg;
    
    // 执行前置任务的逻辑
    printf("执行前置任务\n");
    
    // 更新计数器并通知
    pthread_mutex_lock(data->mutex);
    (*data->counter)--;
    if (*data->counter == 0) {
        pthread_cond_signal(data->cond);
    }
    pthread_mutex_unlock(data->mutex);
    
    free(data);
}

void dependent_task(void *arg) {
    // 执行依赖任务的逻辑
    printf("执行依赖任务\n");
    free(arg);
}

// 调用时
int counter = 3; // 3个前置任务
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// 添加前置任务
for (int i = 0; i < 3; i++) {
    sync_data_t *data = malloc(sizeof(sync_data_t));
    data->counter = &counter;
    data->mutex = &mutex;
    data->cond = &cond;
    thread_pool_add_task(pool, prerequisite_task, data, "前置任务");
}

// 在另一个线程中等待前置任务完成，然后添加依赖任务
pthread_t waiter_thread;
pthread_create(&waiter_thread, NULL, waiter_function, &pool);

// waiter_function的实现
void *waiter_function(void *arg) {
    thread_pool_t *pool = (thread_pool_t*)arg;
    
    pthread_mutex_lock(&mutex);
    while (counter > 0) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    
    // 所有前置任务完成，添加依赖任务
    int *task_arg = malloc(sizeof(int));
    *task_arg = 42;
    thread_pool_add_task(*pool, dependent_task, task_arg, "依赖任务");
    
    return NULL;
}
```

## 最佳实践

### 确定最佳线程数

线程池的最佳线程数取决于多种因素，包括：

1. **CPU核心数**：通常，线程数应该与CPU核心数相匹配或略高。
2. **任务类型**：I/O密集型任务可以使用更多线程，而CPU密集型任务应限制线程数。
3. **系统负载**：在高负载系统上，应减少线程数以避免上下文切换开销。

一种常见的经验法则是：

- 对于CPU密集型任务：线程数 = CPU核心数
- 对于I/O密集型任务：线程数 = CPU核心数 * (1 + I/O等待时间/CPU时间)

示例代码：

```c
#include <unistd.h>

int get_optimal_thread_count() {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    // 对于CPU密集型任务
    return num_cores;
    
    // 对于I/O密集型任务（假设I/O等待时间是CPU时间的2倍）
    // return num_cores * 3;
}

// 使用
thread_pool_t pool = thread_pool_create(get_optimal_thread_count());
```

### 动态调整线程池大小

动态调整线程池大小可以显著提高资源利用率，但需要注意以下几点：

1. **避免频繁调整**：频繁调整线程数量会导致系统开销增加。应该在负载模式发生显著变化时才调整线程数量。

2. **渐进调整**：每次调整线程数量时，应该采用渐进的方式，而不是突然大幅增加或减少线程数。

3. **设置合理的上下限**：在调用`thread_pool_resize`时，确保新的线程数量在合理的范围内，不要设置过高或过低的值。

4. **监控负载**：在调整前使用`thread_pool_get_stats`获取线程池的当前状态，根据队列中的任务数量和活跃线程数量来决定是否需要调整。

```c
// 基于负载动态调整线程池大小的示例
void adjust_thread_pool_based_on_load(thread_pool_t pool) {
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) != 0) {
        return;
    }
    
    // 如果队列中的任务数量超过当前线程数量的两倍，增加线程数
    if (stats.queued_tasks > stats.thread_count * 2) {
        int new_thread_count = stats.thread_count + 2; // 每次增加2个线程
        thread_pool_resize(pool, new_thread_count);
    }
    // 如果队列中没有任务，并且活跃线程很少，减少线程数
    else if (stats.queued_tasks == 0 && stats.active_threads < stats.thread_count / 2) {
        int new_thread_count = stats.thread_count - 1; // 每次减少1个线程
        if (new_thread_count >= 2) { // 确保至少保甸2个线程
            thread_pool_resize(pool, new_thread_count);
        }
    }
}
```

### 使用自动调整功能

自动调整功能可以自动管理线程池大小，但也需要注意以下几点：

1. **适当设置参数**：
   - `min_threads`：设置为能处理基本负载的最小线程数，通常为2或2个。
   - `max_threads`：设置为系统可承受的最大线程数，通常不超过CPU核心数的两倍。
   - `check_interval_ms`：设置为合理的检查间隔，太短会增加系统开销，太长会导致响应不及时。通常在500-2000毫秒之间。

2. **适用场景**：
   - 自动调整功能适用于负载变化大的场景，如Web服务器、数据处理系统等。
   - 对于负载相对稳定的场景，可以使用固定线程数量。

3. **监控和调整**：
   - 定期检查线程池统计信息，确保自动调整功能正常工作。
   - 如果发现线程数量持续在最大值，可能需要调整`max_threads`参数。

4. **优雅关闭**：
   - 在关闭线程池前，先禁用自动调整功能，然后再调用`thread_pool_destroy`。

```c
// 使用自动调整功能的示例
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread.h>

int main(void) {
    // 创建线程池，初始化为2个线程
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 获取CPU核心数，设置最大线程数
    int max_threads = sysconf(_SC_NPROCESSORS_ONLN) * 2;
    if (max_threads <= 0) max_threads = 8; // 默认值
    
    // 启用自动调整，最小2个线程，最大为CPU核心数的2倍，每1秒检查一次
    if (thread_pool_enable_auto_adjust(pool, 2, max_threads, 1000) != 0) {
        fprintf(stderr, "启用线程池自动调整失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    
    printf("自动调整已启用，最小线程数: 2, 最大线程数: %d\n", max_threads);
    
    // ... 添加任务到线程池 ...
    
    // 定期监控线程池状态
    for (int i = 0; i < 10; i++) {
        sleep(5); // 每5秒检查一次
        
        thread_pool_stats_t stats;
        if (thread_pool_get_stats(pool, &stats) == 0) {
            printf("当前线程数: %d, 活跃线程: %d, 队列任务: %d\n", 
                   stats.thread_count, stats.active_threads, stats.queued_tasks);
        }
    }
    
    // 关闭前先禁用自动调整
    thread_pool_disable_auto_adjust(pool);
    
    // 销毁线程池
    thread_pool_destroy(pool);
    
    return 0;
}
```

### 任务粒度

任务粒度是指每个任务执行的工作量。粒度过细会导致线程池管理开销过大，粒度过粗会导致负载不均衡。

**推荐的任务粒度**：

- 任务执行时间应该比线程池管理开销长得多（通常至少几毫秒）。
- 任务之间的执行时间应该相对均衡，以避免某些线程过载而其他线程空闲。
- 对于大型工作，考虑将其分解为多个较小的任务。

### 内存管理

在线程池中正确管理内存非常重要：

1. **明确所有权**：明确定义谁负责释放内存（通常是执行任务的线程）。
2. **避免内存泄漏**：确保所有动态分配的内存都被释放。
3. **避免竞态条件**：使用适当的同步机制保护共享数据。
4. **避免过早释放**：确保内存在使用完毕后才释放。

### 错误处理

线程池中的错误处理策略：

1. **检查返回值**：始终检查`thread_pool_create`、`thread_pool_add_task`和`thread_pool_destroy`的返回值。
2. **优雅处理失败**：如果添加任务失败，确保释放为任务分配的资源。
3. **任务内部错误处理**：任务函数应该捕获并处理内部错误，避免导致工作线程崩溃。

```c
void robust_task(void *arg) {
    if (arg == NULL) {
        fprintf(stderr, "任务收到NULL参数\n");
        return; // 早期返回，避免空指针解引用
    }
    
    // 使用try-catch风格的错误处理
    int result = do_something(arg);
    if (result != 0) {
        fprintf(stderr, "任务执行失败: %d\n", result);
        // 清理资源
        free(arg);
        return;
    }
    
    // 正常执行路径
    free(arg);
}
```

## 常见问题

### Q: 如何确定线程池已经处理完所有任务？

A: 线程池库没有直接提供查询任务队列是否为空的功能。有几种方法可以解决这个问题：

1. **使用睡眠等待**：根据任务的预期执行时间，使用`sleep`等待足够长的时间。
2. **使用外部同步机制**：维护一个计数器，在添加任务时增加，在任务完成时减少。
3. **检查所有线程是否空闲**：使用`thread_pool_get_running_task_names`检查所有线程是否处于"[idle]"状态。

```c
// 检查所有线程是否空闲
int all_threads_idle(thread_pool_t pool, int thread_count) {
    char **running_tasks = thread_pool_get_running_task_names(pool);
    if (!running_tasks) return 0;
    
    int idle = 1;
    for (int i = 0; i < thread_count; i++) {
        if (strcmp(running_tasks[i], "[idle]") != 0) {
            idle = 0;
            break;
        }
    }
    
    free_running_task_names(running_tasks, thread_count);
    return idle;
}

// 使用
while (!all_threads_idle(pool, 4)) {
    usleep(100000); // 睡眠100毫秒
}
```

### Q: 线程池是否线程安全？

A: 是的，线程池库的公共API函数是线程安全的。您可以从多个线程同时调用`thread_pool_add_task`和`thread_pool_get_running_task_names`。但是，`thread_pool_create`和`thread_pool_destroy`应该只由一个线程调用。

### Q: 如何处理长时间运行的任务？

A: 对于长时间运行的任务，有几种策略：

1. **分解任务**：将长任务分解为多个较小的任务。
2. **增加线程数**：使用`thread_pool_resize`增加线程数，为长任务提供更多资源。
3. **实现超时机制**：在任务中实现超时检查。

### Q: 动态调整和自动调整有什么区别？

A: 动态调整和自动调整的主要区别在于：

1. **动态调整**（`thread_pool_resize`）：
   - 由开发者手动调用，根据应用程序的需求和状态主动调整线程数量。
   - 需要开发者自行实现监控和调整逻辑。
   - 适用于开发者能够准确预测负载变化的场景。

2. **自动调整**（`thread_pool_enable_auto_adjust`）：
   - 由线程池内部自动管理，根据负载情况自动调整线程数量。
   - 开发者只需要设置参数（最小/最大线程数和检查间隔）。
   - 适用于负载模式不可预测或变化频繁的场景。

如果应用程序的负载模式变化频繁且不可预测，建议使用自动调整功能。如果需要精确控制线程数量，则使用动态调整功能。

### Q: 自动调整功能如何判断何时调整线程数量？

A: 自动调整功能基于以下因素决定何时调整线程数量：

1. **任务队列长度**：当任务队列中的任务数量超过当前线程数量时，会增加线程数。

2. **活跃线程比例**：当活跃线程数量低于当前线程数量的一定比例（通常是50%）时，会减少线程数。

3. **负载趋势**：系统会追踪负载的变化趋势，如果负载持续增加，会更积极地增加线程数。

4. **最小/最大线程数限制**：调整后的线程数量始终保持在用户设置的`min_threads`和`max_threads`范围内。

自动调整算法会在每个`check_interval_ms`间隔检查一次这些因素，并决定是否需要调整线程数量。

### Q: 如何处理长时间运行的任务？

```c
typedef struct {
    time_t start_time;
    int timeout_seconds;
    void *real_arg;
} timeout_arg_t;

void task_with_timeout(void *arg) {
    timeout_arg_t *timeout_data = (timeout_arg_t*)arg;
    
    // 检查是否超时
    time_t current_time = time(NULL);
    if (current_time - timeout_data->start_time > timeout_data->timeout_seconds) {
        printf("任务超时\n");
        free(timeout_data->real_arg);
        free(timeout_data);
        return;
    }
    
    // 执行实际任务
    do_real_work(timeout_data->real_arg);
    
    // 清理
    free(timeout_data->real_arg);
    free(timeout_data);
}

// 调用时
timeout_arg_t *arg = malloc(sizeof(timeout_arg_t));
arg->start_time = time(NULL);
arg->timeout_seconds = 60; // 60秒超时
arg->real_arg = malloc(sizeof(int));
*(int*)(arg->real_arg) = 42;

thread_pool_add_task(pool, task_with_timeout, arg, "带超时的任务");
```

### Q: 如何处理任务之间的依赖关系？

A: 请参见[高级用法](#高级用法)中的"任务依赖管理"部分。

## 故障排除

### 线程池创建失败

可能的原因：

1. **内存不足**：系统内存不足，无法分配线程池结构或线程数组。
2. **线程数过多**：请求的线程数超过系统限制。
3. **线程创建失败**：系统无法创建更多线程。

解决方案：

1. 减少请求的线程数。
2. 检查系统资源限制（使用`ulimit -a`）。
3. 确保有足够的内存可用。

### 添加任务失败

可能的原因：

1. **线程池为NULL**：传递了无效的线程池句柄。
2. **函数指针为NULL**：传递了无效的任务函数指针。
3. **线程池正在关闭**：尝试向正在关闭的线程池添加任务。
4. **内存分配失败**：无法为任务节点分配内存。

解决方案：

1. 检查线程池和函数指针的有效性。
2. 确保线程池未处于关闭状态。
3. 检查系统内存使用情况。

### 线程池销毁失败

可能的原因：

1. **线程池为NULL**：传递了无效的线程池句柄。
2. **线程连接失败**：无法连接工作线程。

解决方案：

1. 检查线程池句柄的有效性。
2. 确保所有工作线程都能正常退出。

### 任务执行时崩溃

可能的原因：

1. **空指针解引用**：任务函数尝试解引用NULL参数。
2. **内存错误**：访问无效内存、使用已释放内存或缓冲区溢出。
3. **并发访问错误**：多个线程未同步访问共享数据。

解决方案：

1. 在任务函数中检查参数的有效性。
2. 使用适当的同步机制保护共享数据。
3. 使用内存调试工具（如Valgrind）检查内存错误。
4. 在任务函数中实现错误处理。
