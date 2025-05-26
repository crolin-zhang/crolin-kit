/**
 * @file thread_resize_example.c
 * @brief 线程池动态调整大小功能的示例程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/random.h>  // 用于 getrandom()
#include "thread.h"
#include "log.h"      // 包含日志模块头文件

// 全局线程池句柄，用于信号处理
thread_pool_t g_pool = NULL;

// 任务函数
static void task_function(void *arg) {
    int task_id = *(int *)arg;
    printf("任务 #%d 开始执行\n", task_id);
    
    // 模拟工作负载 - 随机工作时间
    unsigned int random_value = 500000;  // 默认值，防止变量未初始化的警告
    if (getrandom(&random_value, sizeof(random_value), 0) == -1) {
        // 如果 getrandom 失败，使用默认值
        random_value = 500000;
    }
    int work_time = 100000 + (int)(random_value % 900000); // 100-1000ms
    usleep(work_time);
    
    printf("任务 #%d 完成执行 (工作时间: %d ms)\n", task_id, work_time / 1000);
    free(arg);
    // void函数不需要返回值
}

// 打印线程池状态
static void print_pool_stats(thread_pool_t pool) {
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
static void init_log(void) {
    // 初始化日志模块
    log_init("thread_resize_example.log", LOG_LEVEL_DEBUG);
    
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
static void signal_handler(int signum) {
    if (signum == SIGINT) {
        // 只设置标志，不进行其他操作
        g_shutdown_requested = 1;
    }
}

int main(void) {

    // 初始化日志模块
    init_log();
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    printf("线程池动态调整大小功能示例\n");
    printf("按 Ctrl+C 退出\n\n");
    
    // 创建线程池，初始4个线程
    g_pool = thread_pool_create(4);
    
    // 设置线程池限制（最小2个，最大8个）
    thread_pool_set_limits(g_pool, 2, 8);
    if (g_pool == NULL) {
        printf("创建线程池失败\n");
        return 1;
    }
    
    printf("线程池创建成功\n");
    print_pool_stats(g_pool);
    
    // 提交一批任务
    printf("\n提交10个任务...\n");
    for (int i = 0; i < 10; i++) {
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
    
    // 等待任务开始执行
    sleep(1);
    printf("\n任务执行中...\n");
    print_pool_stats(g_pool);
    
    // 增加线程数量
    printf("\n增加线程数量到6...\n");
    if (thread_pool_resize(g_pool, 6) == 0) {
        printf("线程数量调整成功\n");
    } else {
        printf("线程数量调整失败\n");
    }
    
    // 等待新线程创建完成
    sleep(1);
    print_pool_stats(g_pool);
    
    // 提交更多任务
    printf("\n提交5个额外任务...\n");
    for (int i = 10; i < 15; i++) {
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
    
    // 等待一段时间
    sleep(3);
    
    // 减少线程数量
    printf("\n减少线程数量到3...\n");
    if (thread_pool_resize(g_pool, 3) == 0) {
        printf("线程数量调整成功\n");
    } else {
        printf("线程数量调整失败\n");
    }
    
    // 等待线程减少完成
    sleep(1);
    print_pool_stats(g_pool);
    
    // 修改线程池限制
    printf("\n设置线程池限制 [1, 10]...\n");
    if (thread_pool_set_limits(g_pool, 1, 10) == 0) {
        printf("线程池限制设置成功\n");
    } else {
        printf("线程池限制设置失败\n");
    }
    
    print_pool_stats(g_pool);
    
    // 尝试超出范围的调整
    printf("\n尝试调整到超出范围的线程数 (12)...\n");
    if (thread_pool_resize(g_pool, 12) == 0) {
        printf("线程数量调整成功（不应该发生）\n");
    } else {
        printf("线程数量调整失败（预期行为）\n");
    }
    
    // 等待用户中断
    printf("\n示例程序正在运行中...\n");
    printf("按 Ctrl+C 退出\n");
    
    // 主循环
    while (!g_shutdown_requested) {
        sleep(5);
        print_pool_stats(g_pool);
    }
    
    // 优雅地关闭
    printf("\n接收到中断信号，正在优雅地关闭线程池...\n");
    if (g_pool != NULL) {
        thread_pool_destroy(g_pool);
        g_pool = NULL;
    }
    
    return 0;
}
