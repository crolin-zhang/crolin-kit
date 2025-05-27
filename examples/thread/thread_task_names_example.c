/**
 * @file thread_task_names_example.c
 * @brief 演示如何获取和使用线程池中正在运行的任务名称
 */

#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/random.h>

// 全局变量用于跟踪完成的任务数量
static int g_tasks_completed = 0;

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
        // 使用更复杂的种子机制提高随机性
        struct timespec time_spec;
        clock_gettime(CLOCK_REALTIME, &time_spec);
        // 组合多个熵源增强随机性
        // 直接使用时间数据生成随机数，无需中间seed变量
        
        // 使用更复杂的方式生成随机数
        rand_val = (unsigned int)time_spec.tv_nsec ^ (unsigned int)time_spec.tv_sec;
    }
    
    // 使用安全的转换方式，避免有符号整数的转换问题
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

/**
 * @brief 模拟长时间运行的任务
 * 
 * 这个任务会睡眠一段随机时间，模拟实际工作负载
 * 
 * @param arg 任务参数，包含任务ID
 */
static void long_running_task(void *arg)
{
    int task_id = *(int *)arg;
    free(arg); // 释放传入的参数内存
    
    // 模拟工作负载 (2-5秒)
    int work_time = get_random_int(2, 5);
    
    printf("任务 #%d 开始执行 (预计耗时 %d 秒)\n", task_id, work_time);
    
    // 模拟工作过程
    for (int i = 0; i < work_time; i++) {
        printf("任务 #%d: 工作进度 %d/%d\n", task_id, i + 1, work_time);
        sleep(1);
    }
    
    printf("任务 #%d 完成\n", task_id);
    __sync_add_and_fetch(&g_tasks_completed, 1);
}

/**
 * @brief 打印线程池中正在运行的任务名称
 * 
 * @param pool 线程池实例
 */
static void print_running_tasks(thread_pool_t pool)
{
    // 获取线程池统计信息
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) != 0) {
        printf("获取线程池统计信息失败\n");
        return;
    }
    
    // 获取正在运行的任务名称
    char **running_tasks = thread_pool_get_running_task_names(pool);
    if (running_tasks == NULL) {
        printf("获取运行中的任务名称失败\n");
        return;
    }
    
    printf("\n=== 线程池状态 ===\n");
    printf("总线程数: %d\n", stats.thread_count);
    printf("空闲线程数: %d\n", stats.idle_threads);
    printf("任务队列长度: %d\n", stats.task_queue_size);
    printf("\n=== 正在运行的任务 ===\n");
    
    // 打印每个线程正在执行的任务
    for (int i = 0; i < stats.thread_count; i++) {
        printf("线程 #%d: 任务名称: %s\n", i, running_tasks[i]);
    }
    printf("========================\n\n");
    
    // 释放任务名称数组
    free_running_task_names(running_tasks, stats.thread_count);
}

int main(void)
{
    printf("=== 线程池任务名称查询示例 ===\n\n");
    
    // 使用get_random_int函数生成随机数，无需初始化
    
    // 创建线程池 (4个线程)
    thread_pool_t pool = thread_pool_create(4);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    printf("线程池创建成功，4个工作线程\n");
    
    // 添加10个长时间运行的任务
    const int num_tasks = 10;
    for (int i = 0; i < num_tasks; i++) {
        int *task_id = malloc(sizeof(int));
        if (task_id == NULL) {
            fprintf(stderr, "内存分配失败\n");
            continue;
        }
        
        *task_id = i + 1;
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "我的名字是 长时间任务_%d", i + 1);
        
        if (thread_pool_add_task_default(pool, long_running_task, task_id, task_name) != 0) {
            fprintf(stderr, "添加任务 #%d 失败\n", i + 1);
            free(task_id);
        } else {
            printf("任务 #%d 已添加到队列\n", i + 1);
        }
    }
    
    // 等待任务开始执行
    printf("\n等待任务开始执行...\n");
    sleep(1);
    
    // 每秒查询一次正在运行的任务名称，直到所有任务完成
    while (g_tasks_completed < num_tasks) {
        print_running_tasks(pool);
        sleep(1);
    }
    
    printf("\n所有任务已完成\n");
    
    // 最后一次查询，应该显示所有线程都是空闲状态
    printf("\n最终线程池状态：\n");
    print_running_tasks(pool);
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    printf("线程池已销毁，示例程序结束\n");
    return 0;
}
