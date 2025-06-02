/**
 * @file thread_task_name_cancel_example.c
 * @brief 演示如何使用任务名称查找和取消线程池中的任务
 */

#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/random.h>

// 全局变量用于跟踪完成的任务数量
static int g_tasks_completed = 0;
static int g_tasks_cancelled = 0;

/**
 * @brief 获取随机整数
 * 
 * @param min 最小值（包含）
 * @param max 最大值（包含）
 * @return int 随机整数
 */
static int get_random_int(int min_val, int max_val)
{
    // 确保参数顺序正确
    if (min_val > max_val) {
        int temp = min_val;
        min_val = max_val;
        max_val = temp;
    }
    
    // 计算范围
    unsigned int range = (unsigned int)(max_val - min_val + 1);
    unsigned int random_value = 0;
    
    // 直接使用getrandom获取随机数
    if (getrandom(&random_value, sizeof(random_value), 0) == -1) {
        // 如果getrandom失败，使用时间作为备用
        struct timespec time_spec;
        clock_gettime(CLOCK_REALTIME, &time_spec);
        random_value = (unsigned int)(time_spec.tv_nsec ^ time_spec.tv_sec);
    }
    
    // 将随机值映射到指定范围
    return min_val + (int)(random_value % range);
}

/**
 * @brief 任务取消回调函数
 * 
 * 当任务被取消时调用此函数
 * 
 * @param arg 任务参数
 * @param task_id 任务ID
 */
static void task_cancel_callback(void *arg, task_id_t task_id)
{
    int task_num = *(int *)arg;
    printf("任务 #%d (任务ID: %lu) 已被取消\n", task_num, (unsigned long)task_id);
    __sync_add_and_fetch(&g_tasks_cancelled, 1);
    free(arg); // 释放参数内存
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
    
    // 模拟工作负载 (5-10秒)
    int work_time = get_random_int(5, 10);
    
    printf("任务 #%d 开始执行 (预计耗时 %d 秒)\n", task_id, work_time);
    
    // 模拟工作过程
    for (int i = 0; i < work_time; i++) {
        printf("任务 #%d: 工作进度 %d/%d\n", task_id, i + 1, work_time);
        sleep(1);
    }
    
    printf("任务 #%d 完成\n", task_id);
    __sync_add_and_fetch(&g_tasks_completed, 1);
    free(arg); // 释放参数内存
}

/**
 * @brief 打印线程池中的任务状态
 * 
 * @param pool 线程池实例
 */
static void print_pool_status(thread_pool_t pool)
{
    // 获取线程池统计信息
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) != 0) {
        printf("获取线程池统计信息失败\n");
        return;
    }
    
    printf("\n=== 线程池状态 ===\n");
    printf("总线程数: %d\n", stats.thread_count);
    printf("空闲线程数: %d\n", stats.idle_threads);
    printf("任务队列长度: %d\n", stats.task_queue_size);
    printf("已完成任务数: %d\n", g_tasks_completed);
    printf("已取消任务数: %d\n", g_tasks_cancelled);
    printf("========================\n\n");
}

int main(void)
{
    printf("=== 线程池任务名称查找和取消示例 ===\n\n");
    
    // 创建线程池 (2个线程)
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    printf("线程池创建成功，2个工作线程\n");
    
    // 添加10个长时间运行的任务
    const int num_tasks = 10;
    task_id_t task_ids[num_tasks];
    char task_names[num_tasks][64];
    
    for (int i = 0; i < num_tasks; i++) {
        int *task_id = malloc(sizeof(int));
        if (task_id == NULL) {
            fprintf(stderr, "内存分配失败\n");
            continue;
        }
        
        *task_id = i + 1;
        snprintf(task_names[i], sizeof(task_names[i]), "长时间任务_%d", i + 1);
        
        task_ids[i] = thread_pool_add_task(pool, long_running_task, task_id, task_names[i], TASK_PRIORITY_NORMAL);
        if (task_ids[i] == 0) {
            fprintf(stderr, "添加任务 #%d 失败\n", i + 1);
            free(task_id);
        } else {
            printf("任务 #%d 已添加到队列，任务ID: %lu, 任务名称: %s\n", 
                   i + 1, (unsigned long)task_ids[i], task_names[i]);
        }
    }
    
    // 打印初始状态
    print_pool_status(pool);
    
    // 测试任务名称查找功能
    printf("\n=== 测试通过任务名称查找任务 ===\n");
    for (int i = 0; i < 3; i++) {
        int index = get_random_int(0, num_tasks - 1);
        int is_running = 0;
        
        task_id_t found_id = thread_pool_find_task_by_name(pool, task_names[index], &is_running);
        if (found_id > 0) {
            printf("找到任务 '%s'，任务ID: %lu，状态: %s\n", 
                   task_names[index], (unsigned long)found_id, 
                   is_running ? "正在运行" : "在队列中等待");
        } else {
            printf("未找到任务 '%s'\n", task_names[index]);
        }
    }
    
    // 测试任务名称唯一性检查
    printf("\n=== 测试任务名称唯一性检查 ===\n");
    int *duplicate_task = malloc(sizeof(int));
    if (duplicate_task != NULL) {
        *duplicate_task = 999;
        // 尝试添加一个重名的任务
        int random_index = get_random_int(0, num_tasks - 1);
        task_id_t dup_id = thread_pool_add_task(pool, long_running_task, duplicate_task, task_names[random_index], TASK_PRIORITY_NORMAL);
        if (dup_id == 0) {
            printf("添加重名任务 '%s' 失败，符合预期（任务名称必须唯一）\n", task_names[random_index]);
            free(duplicate_task);
        } else {
            printf("错误：成功添加了重名任务 '%s'，任务ID: %lu\n", 
                   task_names[random_index], (unsigned long)dup_id);
        }
    }
    
    // 等待一些任务开始执行
    printf("\n等待任务开始执行...\n");
    sleep(2);
    print_pool_status(pool);
    
    // 测试通过任务名称取消任务
    printf("\n=== 测试通过任务名称取消任务 ===\n");
    
    // 取消一些任务
    const int num_to_cancel = 5;
    for (int i = 0; i < num_to_cancel; i++) {
        int index = get_random_int(0, num_tasks - 1);
        
        printf("尝试取消任务 '%s'...\n", task_names[index]);
        int result = thread_pool_cancel_task_by_name(pool, task_names[index], task_cancel_callback);
        
        if (result == 0) {
            printf("成功取消任务 '%s'\n", task_names[index]);
        } else if (result == -1) {
            printf("无法取消任务 '%s'，任务不存在或正在运行\n", task_names[index]);
        } else {
            printf("取消任务 '%s' 失败，参数无效\n", task_names[index]);
        }
    }
    
    // 等待剩余任务完成
    printf("\n等待剩余任务完成...\n");
    while (g_tasks_completed + g_tasks_cancelled < num_tasks) {
        print_pool_status(pool);
        sleep(1);
    }
    
    printf("\n所有任务已完成或取消\n");
    print_pool_status(pool);
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    
    printf("线程池已销毁，示例程序结束\n");
    return 0;
}
