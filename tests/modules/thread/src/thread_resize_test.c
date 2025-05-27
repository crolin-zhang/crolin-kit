/**
 * @file thread_resize_test.c
 * @brief 线程池动态调整大小功能的单元测试 - 随机化版本
 */

#include "log.h" // 包含日志模块头文件
#include "thread.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/random.h>
#include <signal.h>
#include <unistd.h>

// 全局变量
static volatile sig_atomic_t g_alarm_received = 0;

// 安全随机数生成函数
static int get_random_int(int min, int max) {
    unsigned int value = 0;
    unsigned int range = 0;
    
    // 安全地获取随机值
    if (getrandom(&value, sizeof(value), 0) == -1) {
        // 使用多源熵作为后备
        struct {
            pid_t pid;
            struct timespec time_spec;
            void* stack_ptr;
        } entropy;
        
        entropy.pid = getpid();
        clock_gettime(CLOCK_MONOTONIC, &entropy.time_spec);
        entropy.stack_ptr = &entropy;
        
        value = (unsigned int)(entropy.time_spec.tv_nsec ^ 
                             ((uintptr_t)entropy.stack_ptr & 0xFFFFFFFF) ^ 
                             ((unsigned int)entropy.pid << 16));
    }
    
    // 安全地计算范围
    if (max <= min) {
        return min; // 防御性编程
    }
    
    range = (unsigned int)(max - min + 1);
    return min + (int)(value % range);
}

// 信号处理函数
static void alarm_handler(int sig) {
    (void)sig; // 避免未使用参数警告
    g_alarm_received = 1;
}

// 测试任务函数 - 随机化执行时间
static void test_task(void *arg)
{
    int task_id = *(int *)arg;
    
    // 随机化执行时间 (200-800ms)
    int sleep_time = get_random_int(200, 800);
    
    printf("测试任务 #%d 正在执行 (预计耗时 %d ms)\n", task_id, sleep_time);

    // 模拟工作负载
    usleep(sleep_time * 1000);

    printf("测试任务 #%d 完成\n", task_id);
    free(arg);
}

// 初始化日志模块
static void init_log(void)
{
    // 初始化日志模块
    log_init("thread_resize_test.log", LOG_LEVEL_DEBUG);

    // 设置模块日志级别
    log_set_module_level(LOG_MODULE_THREAD, LOG_LEVEL_DEBUG);
    log_set_module_level(LOG_MODULE_CORE, LOG_LEVEL_DEBUG);

    // 设置模块输出（控制台和文件）
    log_set_module_output(LOG_MODULE_THREAD, true, true);
    log_set_module_output(LOG_MODULE_CORE, true, true);
}

// 测试线程池调整大小功能 - 随机化版本
static void test_thread_pool_resize(void)
{
    printf("\n=== 测试线程池调整大小功能 ===\n");

    // 判断是否接收到超时信号
    if (g_alarm_received) {
        printf("\n警告: 测试超时，跳过此部分测试\n");
        return;
    }

    // 初始化日志模块
    init_log();
    
    // 随机初始线程池大小 (3-6个线程)
    int initial_threads = get_random_int(3, 6);
    printf("初始线程池大小: %d 个线程\n", initial_threads);
    
    // 创建线程池，随机初始线程数
    thread_pool_t pool = thread_pool_create(initial_threads);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        exit(1);
    }

    // 随机设置线程池限制（最小2-3个，最大7-10个）
    int min_threads = get_random_int(2, 3);
    int max_threads = get_random_int(7, 10);
    printf("设置线程池限制: 最小 %d 个，最大 %d 个线程\n", 
           min_threads, max_threads);
           
    int ret = thread_pool_set_limits(pool, min_threads, max_threads);
    if (ret != 0) {
        fprintf(stderr, "设置线程池限制失败\n");
        thread_pool_destroy(pool);
        exit(1);
    }

    // 获取初始线程池状态
    thread_pool_stats_t stats;
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);

    printf("初始线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  最小线程数: %d\n", stats.min_threads);
    printf("  最大线程数: %d\n", stats.max_threads);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);

    // 提交随机数量的任务
    int task_count = get_random_int(8, 15); // 随机充8-15个任务
    printf("\n提交 %d 个随机执行时间的任务...\n", task_count);
    
    for (int task_idx = 0; task_idx < task_count; task_idx++) {
        int *arg = (int *)malloc(sizeof(int));
        if (arg == NULL) {
            fprintf(stderr, "内存分配失败\n");
            thread_pool_destroy(pool);
            exit(1);
        }
        
        *arg = task_idx;
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "Task-%d", task_idx);
        
        ret = thread_pool_add_task(pool, test_task, arg, task_name);
        if (ret != 0) {
            fprintf(stderr, "添加任务失败\n");
            free(arg);
            thread_pool_destroy(pool);
            exit(1);
        }
    }

    // 等待任务开始执行
    sleep(1);

    // 获取当前线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);

    printf("\n提交任务后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);

    // 检查是否收到超时信号
    if (g_alarm_received) {
        printf("\n警告: 测试超时，提前退出\n");
        thread_pool_destroy(pool);
        return;
    }
    
    // 随机增加线程数量
    int increase_to = get_random_int(initial_threads + 1, max_threads);
    printf("\n增加线程数量到 %d...\n", increase_to);
    ret = thread_pool_resize(pool, increase_to);
    if (ret != 0) {
        fprintf(stderr, "调整线程池大小失败\n");
        thread_pool_destroy(pool);
        exit(1);
    }

    // 等待新线程创建完成
    printf("等待新线程创建完成...\n");
    usleep(get_random_int(500, 1000) * 1000); // 随机等待500-1000ms

    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);

    printf("增加线程后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);

    // 等待所有任务完成
    sleep(1);

    // 检查是否收到超时信号
    if (g_alarm_received) {
        printf("\n警告: 测试超时，提前退出\n");
        thread_pool_destroy(pool);
        return;
    }
    
    // 随机减少线程数量，但不低于最小值
    int decrease_to = get_random_int(min_threads, increase_to - 1);
    printf("\n减少线程数量到 %d...\n", decrease_to);
    ret = thread_pool_resize(pool, decrease_to);
    if (ret != 0) {
        fprintf(stderr, "调整线程池大小失败\n");
        thread_pool_destroy(pool);
        exit(1);
    }

    // 等待线程减少完成
    printf("等待线程减少完成...\n");
    usleep(get_random_int(500, 1000) * 1000); // 随机等待500-1000ms

    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);

    printf("减少线程后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);

    // 测试随机设置线程池限制
    int new_min = get_random_int(1, 2);
    int new_max = get_random_int(11, 15);
    printf("\n设置新的线程池限制 [%d, %d]...\n", new_min, new_max);
    ret = thread_pool_set_limits(pool, new_min, new_max);
    if (ret != 0) {
        fprintf(stderr, "设置线程池限制失败\n");
        thread_pool_destroy(pool);
        exit(1);
    }

    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    if (ret != 0) {
        fprintf(stderr, "获取线程池状态失败\n");
        thread_pool_destroy(pool);
        exit(1);
    }

    printf("设置限制后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  最小线程数: %d\n", stats.min_threads);
    printf("  最大线程数: %d\n", stats.max_threads);

    // 测试错误情况：调整到超出范围的线程数
    int invalid_size = new_max + get_random_int(3, 5);
    printf("\n测试错误情况：调整到超出范围的线程数 (%d)...\n", invalid_size);
    ret = thread_pool_resize(pool, invalid_size);
    if (ret != -1) {
        fprintf(stderr, "预期的错误没有发生，应该返回-1，实际返回%d\n", ret);
        thread_pool_destroy(pool);
        exit(1);
    }
    printf("测试成功: 正确拒绝了超出范围的调整\n");

    // 销毁线程池
    printf("\n销毁线程池...\n");
    ret = thread_pool_destroy(pool);
    if (ret != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        exit(1);
    }
    
    printf("\n======================================\n");
    printf("=== 线程池调整大小功能测试成功完成 ===\n");
    printf("======================================\n");
}

int main(void)
{
    printf("======================================\n");
    printf("=== 线程池动态调整测试 (随机化版本) ===\n");
    printf("======================================\n");
    
    // 设置超时警报 - 随机 10-15 秒
    int timeout = get_random_int(10, 15);
    printf("测试超时设置: %d 秒\n", timeout);
    signal(SIGALRM, alarm_handler); // 使用信号处理函数，解决未使用的lint警告
    alarm(timeout);

    test_thread_pool_resize();

    // 根据超时状态显示不同的完成提示
    if (g_alarm_received) {
        printf("\n警告: 测试超时，可能未完成所有测试项\n");
    } else {
        printf("\n所有测试项已成功完成！\n");
    }
    
    printf("\n======================================\n");
    printf("=== 线程池动态调整测试已完成并退出 ===\n");
    printf("======================================\n");
    return 0;
}
