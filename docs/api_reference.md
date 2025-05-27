# 线程池库API参考

本文档详细介绍了线程池库的所有公共API函数、数据类型和常量。

## 数据类型

### thread_pool_t

```c
typedef struct thread_pool_s* thread_pool_t;
```

线程池实例的不透明句柄。此类型用于与线程池交互，隐藏了实际的结构定义。

### task_t

```c
typedef struct {
    void (*function)(void *arg); // 指向要执行的函数的指针
    void *arg;                   // 要传递给函数的参数
    char task_name[MAX_TASK_NAME_LEN]; // 任务的名称，用于日志记录/监控
} task_t;
```

表示将由线程池执行的任务。包含函数指针、参数和任务名称。

## 常量

### MAX_TASK_NAME_LEN

```c
#define MAX_TASK_NAME_LEN 64
```

任务名称的最大长度，包括空终止符。此宏定义用于确定`task_t`结构中`task_name`缓冲区的大小，以及库中其他地方存储任务名称的缓冲区的大小。

### DEBUG_THREAD_POOL

```c
// 条件编译宏
#define DEBUG_THREAD_POOL
```

当定义此宏时，将启用日志输出功能。可以在编译时通过`-DDEBUG_THREAD_POOL`标志启用，或在包含头文件之前定义。

## 函数

### thread_pool_create

```c
thread_pool_t thread_pool_create(int num_threads);
```

创建一个新的线程池。

**参数**:
- `num_threads`: 要在池中创建的工作线程数。必须为正数。

**返回值**:
- 成功时返回指向新创建的`thread_pool_t`实例的指针。
- 错误时返回`NULL`（例如，内存分配失败，无效参数）。

**示例**:
```c
// 创建一个包含4个工作线程的线程池
thread_pool_t pool = thread_pool_create(4);
if (pool == NULL) {
    fprintf(stderr, "创建线程池失败\n");
    return 1;
}
```

### thread_pool_add_task

```c
int thread_pool_add_task(thread_pool_t pool, void (*function)(void *), void *arg, const char *task_name);
```

向线程池的队列中添加一个新任务。该任务将被一个可用的工作线程拾取以执行。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。
- `function`: 指向定义任务的函数的指针。不能为空。
- `arg`: 要传递给任务函数的参数。如果函数期望，可以为`NULL`。
- `task_name`: 任务的描述性名称。如果为`NULL`，将使用"unnamed_task"。该名称被复制到任务结构中。

**返回值**:
- 成功时返回0。
- 错误时返回-1（例如，`pool`为`NULL`，`function`为`NULL`，池正在关闭，任务节点的内存分配失败）。

**示例**:
```c
// 定义任务函数
void my_task(void *arg) {
    int task_id = *(int*)arg;
    printf("执行任务 %d\n", task_id);
    free(arg); // 释放参数
}

// 添加任务到线程池
int *arg = malloc(sizeof(int));
*arg = 1;
if (thread_pool_add_task(pool, my_task, arg, "Task-1") != 0) {
    fprintf(stderr, "添加任务失败\n");
    free(arg);
}
```

### thread_pool_get_running_task_names

```c
char **thread_pool_get_running_task_names(thread_pool_t pool);
```

检索由工作线程当前执行的任务名称的副本。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。

**返回值**:
- 一个动态分配的字符串数组（`char **`）。此数组的大小等于池中的线程数。每个字符串是相应线程正在执行的任务名称的副本，或者如果线程当前未执行任务，则为"[idle]"。
- 错误时返回`NULL`（例如，`pool`为`NULL`，内存分配失败）。

**注意**:
- 调用者负责使用`free_running_task_names()`释放返回的数组及其中的每个字符串。

**示例**:
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

### free_running_task_names

```c
void free_running_task_names(char **task_names, int count);
```

释放由`thread_pool_get_running_task_names`返回的任务名称数组。

**参数**:
- `task_names`: 要释放的字符串数组（`char **`）。
- `count`: 数组中的字符串数量（应与调用`thread_pool_get_running_task_names`时池的`thread_count`相匹配）。

**返回值**:
- 无。

**示例**:
```c
// 释放任务名称数组
free_running_task_names(running_tasks, 4);
```

### thread_pool_resize

```c
int thread_pool_resize(thread_pool_t pool, int new_thread_count);
```

动态调整线程池的大小。如果新的线程数量大于当前数量，将创建新的线程。如果新的线程数量小于当前数量，将优雅地关闭多余的线程。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。
- `new_thread_count`: 新的线程数量，必须为正数。

**返回值**:
- 成功时返回0。
- 错误时返回-1（例如，`pool`为`NULL`，`new_thread_count`小于等于0，或内存分配失败）。

**示例**:
```c
// 将线程池大小调整为8个线程
if (thread_pool_resize(pool, 8) != 0) {
    fprintf(stderr, "调整线程池大小失败\n");
}
```

### thread_pool_enable_auto_adjust

```c
int thread_pool_enable_auto_adjust(thread_pool_t pool, int min_threads, int max_threads, int check_interval_ms);
```

启用线程池自动调整功能。根据任务队列的负载自动调整线程数量，在指定的最小和最大线程数之间。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。
- `min_threads`: 线程池的最小线程数量，必须大于0。
- `max_threads`: 线程池的最大线程数量，必须大于等于`min_threads`。
- `check_interval_ms`: 检查负载并调整线程数量的时间间隔（毫秒）。

**返回值**:
- 成功时返回0。
- 错误时返回-1（例如，`pool`为`NULL`，`min_threads`小于等于0，`max_threads`小于`min_threads`，或内存分配失败）。

**示例**:
```c
// 启用自动调整，线程数量在2到8之间，每1000毫秒检查一次
if (thread_pool_enable_auto_adjust(pool, 2, 8, 1000) != 0) {
    fprintf(stderr, "启用线程池自动调整失败\n");
}
```

### thread_pool_disable_auto_adjust

```c
int thread_pool_disable_auto_adjust(thread_pool_t pool);
```

禁用线程池自动调整功能。线程池将保持当前的线程数量，不再自动调整。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。

**返回值**:
- 成功时返回0。
- 错误时返回-1（例如，`pool`为`NULL`）。

**示例**:
```c
// 禁用线程池自动调整
if (thread_pool_disable_auto_adjust(pool) != 0) {
    fprintf(stderr, "禁用线程池自动调整失败\n");
}
```

### thread_pool_get_stats

```c
typedef struct {
    int thread_count;      // 当前线程数量
    int active_threads;    // 当前活跃的线程数量
    int queued_tasks;      // 队列中等待的任务数量
    int completed_tasks;   // 已完成的任务数量
    int auto_adjust_enabled; // 是否启用了自动调整
    int min_threads;       // 自动调整的最小线程数
    int max_threads;       // 自动调整的最大线程数
} thread_pool_stats_t;

int thread_pool_get_stats(thread_pool_t pool, thread_pool_stats_t *stats);
```

获取线程池的当前统计信息。

**参数**:
- `pool`: 指向`thread_pool_t`实例的指针。
- `stats`: 指向`thread_pool_stats_t`结构的指针，用于存储统计信息。

**返回值**:
- 成功时返回0。
- 错误时返回-1（例如，`pool`为`NULL`或`stats`为`NULL`）。

**示例**:
```c
// 获取线程池统计信息
thread_pool_stats_t stats;
if (thread_pool_get_stats(pool, &stats) == 0) {
    printf("线程数量: %d\n", stats.thread_count);
    printf("活跃线程: %d\n", stats.active_threads);
    printf("等待任务: %d\n", stats.queued_tasks);
    printf("已完成任务: %d\n", stats.completed_tasks);
    printf("自动调整: %s\n", stats.auto_adjust_enabled ? "已启用" : "已禁用");
    if (stats.auto_adjust_enabled) {
        printf("最小线程数: %d\n", stats.min_threads);
        printf("最大线程数: %d\n", stats.max_threads);
    }
}
```

### thread_pool_destroy

```c
int thread_pool_destroy(thread_pool_t pool);
```

销毁线程池。通知所有工作线程关闭。如果当前有正在执行的任务，此函数将等待它们完成。队列中剩余的任务将被丢弃。所有相关资源都将被释放。

**参数**:
- `pool`: 指向要销毁的`thread_pool_t`实例的指针。

**返回值**:
- 成功时返回0。
- 如果池指针为`NULL`则返回-1。
- 如果池已在关闭或已销毁，则可能返回0作为无操作。

**示例**:
```c
// 销毁线程池
if (thread_pool_destroy(pool) != 0) {
    fprintf(stderr, "销毁线程池失败\n");
}
```

## 日志宏

当定义了`DEBUG_THREAD_POOL`宏时，以下日志宏可用：

### TPOOL_LOG

```c
#define TPOOL_LOG(fmt, ...) fprintf(stderr, "[THREAD_POOL_LOG] " fmt "\n", ##__VA_ARGS__)
```

用于一般日志消息的宏。仅当定义了`DEBUG_THREAD_POOL`时激活。打印到`stderr`。在消息前添加"[THREAD_POOL_LOG]"。

### TPOOL_ERROR

```c
#define TPOOL_ERROR(fmt, ...) fprintf(stderr, "[THREAD_POOL_ERROR] (%s:%d) " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
```

用于错误日志消息的宏。仅当定义了`DEBUG_THREAD_POOL`时激活。打印到`stderr`。在消息前添加"[THREAD_POOL_ERROR] (file:line)"。

## 完整使用示例

以下是线程池库的完整使用示例，包括基本功能和自动调整功能：

```c
#define DEBUG_THREAD_POOL // 启用日志输出
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
    while (!g_shutdown_requested) {
        // 添加任务到线程池
        for (int i = 0; i < 5; i++) {
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
        
        // 获取当前正在运行的任务名称
        char **running_tasks = thread_pool_get_running_task_names(pool);
        if (running_tasks) {
            printf("当前正在运行的任务:\n");
            for (int i = 0; i < stats.thread_count; i++) {
                printf("线程 %d: %s\n", i, running_tasks[i]);
            }
            free_running_task_names(running_tasks, stats.thread_count);
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
