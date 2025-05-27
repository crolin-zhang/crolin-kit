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

#include "thread.h" // 线程池库的头文件
#include <stdio.h>  // 用于标准输入输出 (printf, fprintf)
#include <stdlib.h> // 用于标准库函数 (malloc, free, exit)
#include <time.h>   // 用于时间相关函数
#include <unistd.h> // 用于 POSIX 操作系统 API (sleep)
#include <sys/random.h> // 用于 getrandom() 函数
#include <signal.h>  // 用于信号处理

#define NUM_THREADS 4 // 定义工作线程的数量
#define NUM_TASKS 10  // 定义要添加到池中的任务数量

// 全局变量，用于信号处理和线程池访问
volatile sig_atomic_t g_shutdown_requested = 0;
thread_pool_t g_pool = NULL;
int g_tasks_completed = 0; // 已完成任务的计数器

/**
 * @brief 获取指定范围内的安全随机数
 * 
 * 使用getrandom()系统调用获取更安全的随机数，
 * 如果失败则回退到基于时间的随机数生成
 * 
 * @param min 最小值（包含）
 * @param max 最大值（包含）
 * @return 范围内的随机整数
 */
static int get_random_int(int min, int max)
{
    unsigned int rand_val = 0;
    
    // 使用getrandom获取随机数，如果失败则使用其他方法
    if (getrandom(&rand_val, sizeof(rand_val), 0) == -1) {
        // 使用更复杂的方式生成随机数
        struct timespec time_spec;
        clock_gettime(CLOCK_REALTIME, &time_spec);
        
        // 直接使用时间数据生成随机数
        rand_val = (unsigned int)time_spec.tv_nsec ^ (unsigned int)time_spec.tv_sec;
    }
    
    // 使用安全的转换方式，避免有符号整数的转换问题
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

// 信号处理函数
static void signal_handler(int signum)
{
    if (signum == SIGINT) {
        // 只设置标志，不进行其他操作
        g_shutdown_requested = 1;
        TPOOL_LOG("接收到中断信号，准备优雅退出...");
    }
}

// 示例任务函数
static void my_task_function(void *arg)
{
    if (arg == NULL) {
        TPOOL_ERROR("任务 (ID: %p): 收到 NULL 参数。", arg);
        return;
    }
    int task_id = *(int *)arg; // 将参数转换为整数ID
    // 通过随机睡眠一段时间来模拟工作
    int sleep_time = get_random_int(1, 3); // 睡眠 1-3 秒

    TPOOL_LOG("任务 %d (参数指针: %p): 开始，将睡眠 %d 秒。", task_id, arg, sleep_time);
    
    // 模拟工作过程，支持中断
    for (int i = 0; i < sleep_time && !g_shutdown_requested; i++) {
        sleep(1); // 每秒检查一次是否请求关闭
    }
    
    TPOOL_LOG("任务 %d (参数指针: %p): 完成。", task_id, arg);
    free(arg); // 释放动态分配的参数
    
    // 增加已完成任务计数
    __sync_add_and_fetch(&g_tasks_completed, 1);
}

int main(void)
{
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    // 使用get_random_int函数生成随机数，无需初始化随机种子

    TPOOL_LOG("Main: 开始线程池演示。");
    TPOOL_LOG("Main: 按 Ctrl+C 可随时优雅退出。");

    // 创建线程池
    TPOOL_LOG("Main: 正在创建包含 %d 个线程的线程池。", NUM_THREADS);
    g_pool = thread_pool_create(NUM_THREADS); // 调用线程池创建函数

    if (g_pool == NULL) {
        TPOOL_ERROR("Main: 创建线程池失败。正在退出。");
        return EXIT_FAILURE; // 创建失败则退出
    }
    TPOOL_LOG("Main: 线程池创建成功: %p", (void *)g_pool);

    // 向池中添加任务
    TPOOL_LOG("Main: 正在向池中添加 %d 个任务。", NUM_TASKS);
    int tasks_added = 0; // 实际添加的任务数量
    for (int i = 0; i < NUM_TASKS && !g_shutdown_requested; i++) {
        int *task_arg = (int *)malloc(sizeof(int)); // 为任务参数动态分配内存
        if (!task_arg) {
            TPOOL_ERROR("Main: 未能为任务 %d 分配参数。跳过此任务。", i);
            continue; // 如果分配失败，则跳过此任务
        }
        *task_arg = i + 1; // 任务 ID 从 1 到 NUM_TASKS

        char task_name_buf[MAX_TASK_NAME_LEN]; // 用于存储任务名称的缓冲区
        snprintf(task_name_buf, MAX_TASK_NAME_LEN, "示例任务-%d", i + 1); // 创建任务名称

        TPOOL_LOG("Main: 正在添加任务 %s (参数指针: %p, 值: %d)", task_name_buf, (void *)task_arg,
                  *task_arg);
        if (thread_pool_add_task_default(g_pool, my_task_function, task_arg, task_name_buf) != 0) {
            TPOOL_ERROR("Main: 添加任务 %s 失败。正在释放参数。", task_name_buf);
            free(task_arg); // 如果任务添加失败，则释放参数
        } else {
            tasks_added++;
        }
    }

    // 如果请求关闭，则跳过后续步骤
    if (g_shutdown_requested) {
        goto cleanup;
    }

    // 演示检查正在运行的任务
    TPOOL_LOG("Main: 睡眠 2 秒后检查正在运行的任务...");
    sleep(2);

    // 如果请求关闭，则跳过后续步骤
    if (g_shutdown_requested) {
        goto cleanup;
    }

    TPOOL_LOG("Main: 正在检查运行中的任务...");
    // 我们使用 NUM_THREADS 作为计数，如提示中所讨论。
    char **running_tasks = thread_pool_get_running_task_names(g_pool); // 获取当前运行任务的名称列表
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
        TPOOL_LOG(
            "Main: 无法获取正在运行的任务名称 (thread_pool_get_running_task_names 返回 NULL)。");
    }

    // 等待所有任务完成，使用精确的计数器而不是估计时间
    TPOOL_LOG("Main: 等待所有任务完成 (已添加 %d 个任务)...", tasks_added);
    
    // 最多等待30秒，防止无限等待
    int max_wait_seconds = 30;
    int waited_seconds = 0;
    
    while (g_tasks_completed < tasks_added && !g_shutdown_requested && waited_seconds < max_wait_seconds) {
        // 每秒检查一次任务完成情况和线程池状态
        sleep(1);
        waited_seconds++;
        
        // 每5秒打印一次状态
        if (waited_seconds % 5 == 0 || g_tasks_completed == tasks_added) {
            TPOOL_LOG("Main: 已完成 %d/%d 个任务，已等待 %d 秒", 
                     g_tasks_completed, tasks_added, waited_seconds);
            
            // 打印线程池状态
            thread_pool_stats_t stats;
            if (thread_pool_get_stats(g_pool, &stats) == 0) {
                TPOOL_LOG("Main: 线程池状态 - 线程数: %d, 空闲线程: %d, 任务队列: %d",
                          stats.thread_count, stats.idle_threads, stats.task_queue_size);
            }
        }
    }
    
    // 检查是否所有任务都已完成
    if (g_tasks_completed == tasks_added) {
        TPOOL_LOG("Main: 所有任务已完成！");
    } else if (g_shutdown_requested) {
        TPOOL_LOG("Main: 因用户请求而中断等待，已完成 %d/%d 个任务", 
                 g_tasks_completed, tasks_added);
    } else {
        TPOOL_LOG("Main: 等待超时，已完成 %d/%d 个任务", 
                 g_tasks_completed, tasks_added);
    }

    // 如果请求关闭，则跳过后续步骤
    if (g_shutdown_requested) {
        goto cleanup;
    }

    // 最后一次检查运行中的任务
    TPOOL_LOG("Main: 最后一次检查运行中的任务...");
    running_tasks = thread_pool_get_running_task_names(g_pool);
    if (running_tasks) {
        TPOOL_LOG("Main: 所有任务完成后当前正在运行的任务 (应全为 [idle]):");
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
        TPOOL_LOG("Main: 最后一次检查无法获取正在运行的任务名称。");
    }

cleanup:
    // 销毁线程池
    TPOOL_LOG("Main: 正在销毁线程池: %p", (void *)g_pool);
    if (thread_pool_destroy(g_pool) == 0) {
        TPOOL_LOG("Main: 线程池销毁成功。");
    } else {
        TPOOL_ERROR("Main: 销毁线程池时出错。");
    }
    g_pool = NULL;

    TPOOL_LOG("Main: 线程池演示完成。正在退出。");
    return EXIT_SUCCESS; // 程序成功退出
}
