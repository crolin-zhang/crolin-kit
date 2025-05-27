/**
 * @file thread_auto_adjust_example.c
 * @brief 线程池自动动态调整功能的示例程序
 */

#include "log.h" // 包含日志模块头文件
#include "thread.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h> // 用于 getrandom()
#include <time.h>
#include <unistd.h>

// 全局线程池句柄，用于信号处理
thread_pool_t g_pool = NULL;

// 任务函数
static void task_function(void *arg)
{
    int task_id = *(int *)arg;
    printf("任务 #%d 开始执行\n", task_id);

    // 模拟工作负载 - 随机工作时间
    unsigned int random_value = 500000; // 默认值，防止变量未初始化的警告
    if (getrandom(&random_value, sizeof(random_value), 0) == -1) {
        // 如果 getrandom 失败，使用默认值
        random_value = 500000;
    }
    int work_time = 100000 + (int)(random_value % 900000); // 100-1000ms
    usleep(work_time);

    printf("任务 #%d 完成执行 (工作时间: %d ms)\n", task_id, work_time / 1000);
    free(arg);
}

// 打印线程池状态
static void print_pool_stats(thread_pool_t pool)
{
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("线程池状态:\n");
        printf("  线程数量: %d\n", stats.thread_count);
        printf("  最小线程数: %d\n", stats.min_threads);
        printf("  最大线程数: %d\n", stats.max_threads);
        printf("  空闲线程数: %d\n", stats.idle_threads);
        printf("  任务队列大小: %d\n", stats.task_queue_size);
        printf("  已启动线程数: %d\n", stats.started);
    } else {
        printf("无法获取线程池状态\n");
    }
}

// 初始化日志模块
static void init_log(void)
{
    // 初始化日志模块
    log_init("thread_auto_adjust_example.log", LOG_LEVEL_DEBUG);

    // 设置模块日志级别
    log_set_module_level(LOG_MODULE_THREAD, LOG_LEVEL_DEBUG);
    log_set_module_level(LOG_MODULE_CORE, LOG_LEVEL_DEBUG);

    // 设置模块输出（控制台和文件）
    log_set_module_output(LOG_MODULE_THREAD, true, true);
    log_set_module_output(LOG_MODULE_CORE, true, true);
}

// 全局变量，用于信号处理
volatile sig_atomic_t g_shutdown_requested = 0;

// 信号处理函数
static void signal_handler(int signum)
{
    if (signum == SIGINT) {
        // 只设置标志，不进行其他操作
        g_shutdown_requested = 1;
    }
}

int main(void)
{
    // 初始化日志模块
    init_log();

    // 设置信号处理
    signal(SIGINT, signal_handler);

    printf("线程池自动动态调整功能示例\n");
    printf("按 Ctrl+C 退出\n\n");

    // 创建线程池，初始4个线程
    g_pool = thread_pool_create(4);
    if (g_pool == NULL) {
        printf("创建线程池失败\n");
        return 1;
    }

    // 设置线程池限制（最小2个，最大8个）
    thread_pool_set_limits(g_pool, 2, 8);

    printf("线程池创建成功\n");
    print_pool_stats(g_pool);

    // 启用自动动态调整功能
    // 参数：线程池、任务队列高水位线、空闲线程高水位线、调整间隔（毫秒）
    thread_pool_enable_auto_adjust(g_pool, 5, 2, 3000);
    printf("\n已启用线程池自动动态调整功能\n");
    printf("  任务队列高水位线: 5 (当任务队列长度超过5时，增加线程)\n");
    printf("  空闲线程高水位线: 2 (当空闲线程数超过2时，减少线程)\n");
    printf("  调整间隔: 3000ms (每3秒最多调整一次)\n\n");

    // 模拟工作负载变化
    printf("模拟工作负载变化...\n\n");

    // 阶段1：低负载
    printf("阶段1：低负载 (提交3个任务)...\n");
    for (int i = 0; i < 3; i++) {
        int *arg = (int *)malloc(sizeof(int));
        if (arg == NULL) {
            printf("内存分配失败，无法添加任务 #%d\n", i);
            continue;
        }
        *arg = i;
        if (thread_pool_add_task(g_pool, task_function, arg, "task") != 0) {
            printf("添加任务 #%d 失败\n", i);
            free(arg);
        }
    }

    // 等待任务执行
    sleep(5);
    print_pool_stats(g_pool);

    // 阶段2：高负载
    printf("\n阶段2：高负载 (提交10个任务)...\n");
    for (int i = 0; i < 10; i++) {
        int *arg = (int *)malloc(sizeof(int));
        if (arg == NULL) {
            printf("内存分配失败，无法添加任务 #%d\n", i);
            continue;
        }
        *arg = i + 3;
        if (thread_pool_add_task(g_pool, task_function, arg, "task") != 0) {
            printf("添加任务 #%d 失败\n", i);
            free(arg);
        }
    }

    // 等待任务执行
    sleep(3);
    print_pool_stats(g_pool);

    // 再等待一段时间，观察线程池自动调整
    sleep(5);
    print_pool_stats(g_pool);

    // 阶段3：无负载
    printf("\n阶段3：无负载 (等待空闲线程减少)...\n");
    sleep(5);
    print_pool_stats(g_pool);

    // 禁用自动调整
    printf("\n禁用自动动态调整功能...\n");
    thread_pool_disable_auto_adjust(g_pool);

    // 阶段4：再次高负载，但不会自动调整
    printf("\n阶段4：高负载但禁用自动调整 (提交10个任务)...\n");
    for (int i = 0; i < 10; i++) {
        int *arg = (int *)malloc(sizeof(int));
        if (arg == NULL) {
            printf("内存分配失败，无法添加任务 #%d\n", i);
            continue;
        }
        *arg = i + 13;
        if (thread_pool_add_task(g_pool, task_function, arg, "task") != 0) {
            printf("添加任务 #%d 失败\n", i);
            free(arg);
        }
    }

    // 等待任务执行
    sleep(3);
    print_pool_stats(g_pool);

    // 再等待一段时间，观察线程池不会自动调整
    sleep(5);
    print_pool_stats(g_pool);

    // 等待用户中断
    printf("\n示例程序正在运行中...\n");
    printf("按 Ctrl+C 退出\n");

    // 主循环，最多运行30秒后自动退出
    int countdown = 6; // 6次循环 × 5秒 = 30秒
    while (!g_shutdown_requested && countdown > 0) {
        sleep(5);
        print_pool_stats(g_pool);
        countdown--;
        printf("\n剩余时间: %d 秒...\n", countdown * 5);
    }

    // 优雅地关闭
    printf("\n接收到中断信号，正在优雅地关闭线程池...\n");
    if (g_pool != NULL) {
        thread_pool_destroy(g_pool);
        g_pool = NULL;
    }

    return 0;
}
