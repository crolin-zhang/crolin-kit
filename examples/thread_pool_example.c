/**
 * @file thread_pool_example.c
 * @brief 线程池库的示例程序
 *
 * 此示例程序展示了如何使用线程池库创建线程池、添加任务、
 * 获取运行中的任务名称以及销毁线程池。
 */

// 在包含 thread.h 之前定义 DEBUG_THREAD_POOL 以启用日志
// 这通常通过编译器标志来完成，例如 -DDEBUG_THREAD_POOL
#define DEBUG_THREAD_POOL 

#include <stdio.h>      // 用于标准输入输出 (printf, fprintf)
#include <stdlib.h>     // 用于标准库函数 (malloc, free, srand, exit)
#include <unistd.h>     // 用于 POSIX 操作系统 API (sleep)
#include <time.h>       // 用于时间相关函数 (time, 用于 srand 初始化)
#include "thread.h"     // 线程池库的头文件

#define NUM_THREADS 4   // 定义工作线程的数量
#define NUM_TASKS 10    // 定义要添加到池中的任务数量

// 示例任务函数
void my_task_function(void *arg) {
    if (arg == NULL) {
        TPOOL_ERROR("任务 (ID: %p): 收到 NULL 参数。", arg);
        return;
    }
    int task_id = *(int *)arg; // 将参数转换为整数ID
    // 通过随机睡眠一段时间来模拟工作
    int sleep_time = (rand() % 3) + 1; // 睡眠 1-3 秒
    
    TPOOL_LOG("任务 %d (参数指针: %p): 开始，将睡眠 %d 秒。", task_id, arg, sleep_time);
    sleep(sleep_time); // 执行睡眠
    TPOOL_LOG("任务 %d (参数指针: %p): 完成。", task_id, arg);
    
    free(arg); // 释放动态分配的参数
}

int main() {
    // 初始化随机数种子
    srand(time(NULL));

    TPOOL_LOG("Main: 开始线程池演示。");

    // 创建线程池
    TPOOL_LOG("Main: 正在创建包含 %d 个线程的线程池。", NUM_THREADS);
    thread_pool_t pool = thread_pool_create(NUM_THREADS); // 调用线程池创建函数

    if (pool == NULL) {
        TPOOL_ERROR("Main: 创建线程池失败。正在退出。");
        return EXIT_FAILURE; // 创建失败则退出
    }
    TPOOL_LOG("Main: 线程池创建成功: %p", (void*)pool);

    // 向池中添加任务
    TPOOL_LOG("Main: 正在向池中添加 %d 个任务。", NUM_TASKS);
    for (int i = 0; i < NUM_TASKS; i++) {
        int *task_arg = (int *)malloc(sizeof(int)); // 为任务参数动态分配内存
        if (!task_arg) {
            TPOOL_ERROR("Main: 未能为任务 %d 分配参数。跳过此任务。", i);
            continue; // 如果分配失败，则跳过此任务
        }
        *task_arg = i + 1; // 任务 ID 从 1 到 NUM_TASKS

        char task_name_buf[MAX_TASK_NAME_LEN]; // 用于存储任务名称的缓冲区
        snprintf(task_name_buf, MAX_TASK_NAME_LEN, "示例任务-%d", i + 1); // 创建任务名称

        TPOOL_LOG("Main: 正在添加任务 %s (参数指针: %p, 值: %d)", task_name_buf, (void*)task_arg, *task_arg);
        if (thread_pool_add_task(pool, my_task_function, task_arg, task_name_buf) != 0) {
            TPOOL_ERROR("Main: 添加任务 %s 失败。正在释放参数。", task_name_buf);
            free(task_arg); // 如果任务添加失败，则释放参数
        }
    }

    // 演示检查正在运行的任务
    TPOOL_LOG("Main: 睡眠 2 秒后检查正在运行的任务...");
    sleep(2); 

    TPOOL_LOG("Main: 正在检查运行中的任务...");
    // 我们使用 NUM_THREADS 作为计数，如提示中所讨论。
    char **running_tasks = thread_pool_get_running_task_names(pool); // 获取当前运行任务的名称列表
    if (running_tasks) {
        TPOOL_LOG("Main: 当前正在运行的任务 (或 [idle]):");
        for (int i = 0; i < NUM_THREADS; i++) {
            // 线程可能处于空闲状态，或者无法检索到名称
            if (running_tasks[i]) {
                TPOOL_LOG("Main: 线程 %d 正在运行: %s", i, running_tasks[i]);
            } else {
                // 如果 thread_pool_get_running_task_names 健壮，则理想情况下不应发生此情况
                TPOOL_LOG("Main: 线程 %d 的任务名称为 NULL (应为 [idle] 或任务名称)。", i);
            }
        }
        // 使用 free_running_task_names 函数释放内存
        free_running_task_names(running_tasks, NUM_THREADS);
        TPOOL_LOG("Main: 已释放复制的正在运行的任务名称数组。");
    } else {
        TPOOL_LOG("Main: 无法获取正在运行的任务名称 (thread_pool_get_running_task_names 返回 NULL)。");
    }

    // 等待任务可能完成 (这是一个粗略的估计)
    // 更健壮的解决方案将涉及检查任务队列是否为空
    // 并且所有线程都处于空闲状态，或使用专用的同步原语。
    int wait_time_estimate = (NUM_TASKS / NUM_THREADS + 1) * 3; // 每个任务的平均睡眠时间
    TPOOL_LOG("Main: 等待任务完成 (预计 %d 秒)...", wait_time_estimate);
    sleep(wait_time_estimate); 
    // 对于更确定的等待，如果需要，可以实现一种查询 task_queue_size 
    // 或等待直到所有初始任务都已处理的方法。

    TPOOL_LOG("Main: 等待后再次检查正在运行的任务...");
    running_tasks = thread_pool_get_running_task_names(pool);
    if (running_tasks) {
        TPOOL_LOG("Main: 长时间等待后当前正在运行的任务 (或 [idle]):");
        for (int i = 0; i < NUM_THREADS; i++) {
            if (running_tasks[i]) {
                TPOOL_LOG("Main: 线程 %d 正在运行: %s", i, running_tasks[i]);
            } else {
                TPOOL_LOG("Main: 线程 %d 的任务名称为 NULL。", i);
            }
        }
        free_running_task_names(running_tasks, NUM_THREADS);
        TPOOL_LOG("Main: 已释放复制的正在运行的任务名称数组。");
    } else {
        TPOOL_LOG("Main: 第二次检查无法获取正在运行的任务名称。");
    }

    // 销毁线程池
    TPOOL_LOG("Main: 正在销毁线程池: %p", (void*)pool);
    if (thread_pool_destroy(pool) == 0) {
        TPOOL_LOG("Main: 线程池销毁成功。");
    } else {
        TPOOL_ERROR("Main: 销毁线程池时出错。");
    }
    
    TPOOL_LOG("Main: 线程池演示完成。正在退出。");
    return EXIT_SUCCESS; // 程序成功退出
}
