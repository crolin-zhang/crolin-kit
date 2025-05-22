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

以下是线程池库的完整使用示例：

```c
#define DEBUG_THREAD_POOL // 启用日志输出
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
    
    // 获取当前正在运行的任务名称
    sleep(1); // 等待一些任务开始执行
    char **running_tasks = thread_pool_get_running_task_names(pool);
    if (running_tasks) {
        for (int i = 0; i < 4; i++) {
            printf("线程 %d: %s\n", i, running_tasks[i]);
        }
        free_running_task_names(running_tasks, 4);
    }
    
    // 等待所有任务完成
    sleep(3);
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    return 0;
}
```
