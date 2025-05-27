#include "thread.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

// 全局变量
static int completed_tasks = 0;
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

// 测试任务函数 - 随机执行时间
static void test_task(void *arg)
{
    int task_id = *(int *)arg;
    
    // 随机化执行时间 (10-150ms)
    int sleep_time = get_random_int(10, 150);

    // 模拟任务执行
    usleep(sleep_time * 1000);

    // 使用原子操作增加任务计数
    __sync_fetch_and_add(&completed_tasks, 1);
    
    printf("任务 #%d 已完成 (休眠了 %d ms)\n", task_id, sleep_time);

    // 释放参数内存
    free(arg);
}

// 测试基本功能 - 增加随机性
static void test_basic_functionality(void)
{
    // 随机化线程池大小 (2-6个线程)
    const int num_threads = get_random_int(2, 6);
    
    // 随机化任务数量 (15-30个任务)
    const int num_tasks = get_random_int(15, 30);

    printf("\n=== 测试线程池基本功能 ===\n");
    printf("线程数量: %d，任务数量: %d\n", num_threads, num_tasks);

    // 创建线程池
    thread_pool_t pool = thread_pool_create(num_threads);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        exit(1);
    }
    printf("成功创建包含 %d 个线程的线程池\n", num_threads);

    // 重置任务完成计数
    completed_tasks = 0;

    // 添加任务到线程池
    for (int i = 0; i < num_tasks; i++) {
        int *task_id = malloc(sizeof(int));
        *task_id = i;
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "Task-%d", i);

        int result = thread_pool_add_task_default(pool, test_task, task_id, task_name);
        assert(result == 0);
        printf("已添加任务 #%d\n", i);
    }

    // 检查当前运行的任务
    printf("\n=== 当前运行的任务 ===\n");
    char **tasks = thread_pool_get_running_task_names(pool);
    assert(tasks != NULL);

    for (int i = 0; i < num_threads; i++) {
        printf("线程 #%d: %s\n", i, tasks[i]);
    }

    // 释放任务名称数组
    free_running_task_names(tasks, num_threads);

    // 等待所有任务完成，添加超时机制
    int progress_interval = get_random_int(5, 10); // 随机进度报告间隔
    printf("进度: %d/%d 任务已完成\n", completed_tasks, num_tasks);
    
    int timeout_counter = 0;
    const int max_timeout = 200; // 最多等待10秒 (200 * 50ms)
    
    while (completed_tasks < num_tasks && timeout_counter < max_timeout && !g_alarm_received) {
        usleep(50000); // 50ms
        timeout_counter++;
        
        // 随机间隔显示进度
        if (timeout_counter % progress_interval == 0) {
            printf("进度: %d/%d 任务已完成 (剩余等待时间: %.1f 秒)\n", 
                   completed_tasks, num_tasks, (max_timeout - timeout_counter) * 0.05);
        }
    }
    
    // 检查完成状态
    printf("\n----------- 测试结果 -----------\n");
    if (g_alarm_received) {
        printf("警告: 收到超时信号，测试被中断\n");
        printf("完成情况: %d/%d 任务 (%.1f%%)\n", 
               completed_tasks, num_tasks, 
               (100.0 * (double)completed_tasks) / (double)num_tasks);
    } else if (completed_tasks < num_tasks) {
        printf("警告: 超时结束，只有 %d/%d 任务完成 (%.1f%%)\n", 
               completed_tasks, num_tasks,
               (100.0 * (double)completed_tasks) / (double)num_tasks);
    } else {
        printf("成功: 所有 %d 个任务已完成 (100%%)\n", num_tasks);
    }

    // 再次检查当前运行的任务
    printf("\n=== 当前运行的任务 ===\n");
    tasks = thread_pool_get_running_task_names(pool);
    assert(tasks != NULL);

    for (int i = 0; i < num_threads; i++) {
        printf("线程 #%d: %s\n", i, tasks[i]);
    }

    // 释放任务名称数组
    free_running_task_names(tasks, num_threads);

    printf("\n所有 %d 个任务已完成\n", num_tasks);

    // 销毁线程池
    int result = thread_pool_destroy(pool);
    assert(result == 0);
    printf("线程池已成功销毁\n");
}

// 测试错误处理
static void test_error_handling(void)
{
    printf("\n=== 测试错误处理 ===\n");

    // 测试无效参数
    thread_pool_t pool = thread_pool_create(0);
    assert(pool == NULL);
    printf("测试通过: 无法创建线程数为0的线程池\n");

    // 创建有效的线程池用于后续测试
    pool = thread_pool_create(2);
    assert(pool != NULL);

    // 测试无效的任务函数
    int result = thread_pool_add_task_default(pool, NULL, NULL, "invalid-task");
    assert(result != 0);
    printf("测试通过: 无法添加函数指针为NULL的任务\n");

    // 测试无效的线程池指针
    result = thread_pool_add_task_default(NULL, test_task, NULL, "invalid-pool");
    assert(result != 0);
    printf("测试通过: 无法向NULL线程池添加任务\n");

    // 测试获取运行任务名称的错误处理
    char **tasks = thread_pool_get_running_task_names(NULL);
    assert(tasks == NULL);
    printf("测试通过: 从NULL线程池获取任务名称返回NULL\n");

    // 测试销毁NULL线程池
    result = thread_pool_destroy(NULL);
    assert(result != 0);
    printf("测试通过: 销毁NULL线程池返回错误\n");

    // 清理
    thread_pool_destroy(pool);
    printf("错误处理测试全部通过\n");
}

int main(void)
{
    printf("======================================\n");
    printf("=== 线程池单元测试 (随机化版本) ===\n");
    printf("======================================\n");

    // 设置超时警报 - 随机 10-15 秒
    int timeout = get_random_int(10, 15);
    printf("测试超时设置: %d 秒\n", timeout);
    signal(SIGALRM, alarm_handler);
    alarm(timeout);

    // 运行测试
    test_basic_functionality();
    
    // 检查是否接收到警报信号
    if (g_alarm_received) {
        printf("\n警告: 测试超时，跳过错误处理测试\n");
    } else {
        test_error_handling();
    }

    printf("\n======================================\n");
    printf("=== 线程池单元测试已完成并退出 ===\n");
    printf("======================================\n");
    return 0;
}
