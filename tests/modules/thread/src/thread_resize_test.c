/**
 * @file thread_resize_test.c
 * @brief 线程池动态调整大小功能的单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include "thread.h"
#include "log.h"      // 包含日志模块头文件

// 测试任务函数
static void test_task(void *arg) {
    int task_id = *(int *)arg;  // 使用更有意义的变量名
    printf("测试任务 #%d 正在执行\n", task_id);
    
    // 模拟工作负载
    usleep(500000);  // 500ms
    
    printf("测试任务 #%d 完成\n", task_id);
    free(arg);
    // void函数不需要返回值
}

// 初始化日志模块
static void init_log(void) {
    // 初始化日志模块
    log_init("thread_resize_test.log", LOG_LEVEL_DEBUG);
    
    // 设置模块日志级别
    log_set_module_level(LOG_MODULE_THREAD, LOG_LEVEL_DEBUG);
    log_set_module_level(LOG_MODULE_CORE, LOG_LEVEL_DEBUG);
    
    // 设置模块输出（控制台和文件）
    log_set_module_output(LOG_MODULE_THREAD, true, true);
    log_set_module_output(LOG_MODULE_CORE, true, true);
}

// 测试线程池调整大小功能
static void test_thread_pool_resize(void) {
    printf("\n=== 测试线程池调整大小功能 ===\n");
    
    // 初始化日志模块
    init_log();
    
    // 创建线程池，初始4个线程
    thread_pool_t pool = thread_pool_create(4);
    assert(pool != NULL);
    
    // 设置线程池限制（最小2个，最大8个）
    int ret = thread_pool_set_limits(pool, 2, 8);
    assert(ret == 0);
    
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
    
    // 提交一些任务
    printf("\n提交10个任务...\n");
    for (int i = 0; i < 10; i++) {
        int *arg = (int *)malloc(sizeof(int));
        assert(arg != NULL); // 在测试中，内存分配失败应该导致测试失败
        *arg = i;
        ret = thread_pool_add_task(pool, test_task, arg, "test_task");
        assert(ret == 0);
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
    
    // 增加线程数量
    printf("\n增加线程数量到6...\n");
    ret = thread_pool_resize(pool, 6);
    assert(ret == 0);
    
    // 等待新线程创建完成
    sleep(1);
    
    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);
    
    printf("增加线程后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);
    
    // 等待所有任务完成
    sleep(1);
    
    // 减少线程数量
    printf("\n减少线程数量到3...\n");
    ret = thread_pool_resize(pool, 3);
    assert(ret == 0);
    
    // 等待线程减少完成
    sleep(1);
    
    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);
    
    printf("减少线程后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  空闲线程数: %d\n", stats.idle_threads);
    printf("  任务队列大小: %d\n", stats.task_queue_size);
    
    // 测试设置线程池限制
    printf("\n设置线程池限制 [1, 10]...\n");
    ret = thread_pool_set_limits(pool, 1, 10);
    assert(ret == 0);
    
    // 获取调整后的线程池状态
    ret = thread_pool_get_stats(pool, &stats);
    assert(ret == 0);
    
    printf("设置限制后线程池状态：\n");
    printf("  线程数量: %d\n", stats.thread_count);
    printf("  最小线程数: %d\n", stats.min_threads);
    printf("  最大线程数: %d\n", stats.max_threads);
    
    // 测试错误情况：调整到超出范围的线程数
    printf("\n测试错误情况：调整到超出范围的线程数 (12)...\n");
    ret = thread_pool_resize(pool, 12);
    assert(ret == -1);
    
    // 销毁线程池
    printf("\n销毁线程池...\n");
    ret = thread_pool_destroy(pool);
    assert(ret == 0);
    
    printf("=== 线程池调整大小功能测试完成 ===\n");
}

int main(void) {
    printf("开始线程池动态调整大小功能测试\n");
    
    test_thread_pool_resize();
    
    printf("所有测试通过！\n");
    return 0;
}
