/**
 * @file thread_unnamed_tasks_example.c
 * @brief 演示未命名任务自动生成唯一名称功能
 */

#include "thread.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/random.h>

// 简单任务函数
void simple_task(void *arg) {
    int task_num = *((int *)arg);
    int duration = 2 + (task_num % 3); // 2-4秒的执行时间
    
    printf("未命名任务 #%d 开始执行 (耗时 %d 秒)\n", task_num, duration);
    
    for (int i = 1; i <= duration; i++) {
        printf("未命名任务 #%d: 进度 %d/%d\n", task_num, i, duration);
        sleep(1);
    }
    
    printf("未命名任务 #%d 执行完成\n", task_num);
    free(arg); // 释放在main中分配的参数内存
}

// 打印线程池状态
void print_pool_status(thread_pool_t pool) {
    thread_pool_stats_t stats;
    thread_pool_get_stats(pool, &stats);
    
    printf("\n=== 线程池状态 ===\n");
    printf("总线程数: %d\n", stats.thread_count);
    printf("空闲线程数: %d\n", stats.idle_threads);
    printf("任务队列长度: %d\n", stats.task_queue_size);
    printf("已启动线程数: %d\n", stats.started);
    printf("最小线程数: %d\n", stats.min_threads);
    printf("最大线程数: %d\n", stats.max_threads);
    printf("========================\n\n");
}

int main(void) {
    // 初始化日志系统
    log_init(NULL, LOG_LEVEL_INFO);
    log_set_module_level(LOG_MODULE_THREAD, LOG_LEVEL_INFO);
    
    printf("=== 未命名任务自动生成唯一名称测试 ===\n\n");
    
    // 创建线程池，2个工作线程
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        printf("线程池创建失败\n");
        return 1;
    }
    printf("线程池创建成功，2个工作线程\n");
    
    // 添加多个未命名任务
    task_id_t task_ids[10];
    int num_tasks = 10;
    
    for (int i = 0; i < num_tasks; i++) {
        int *task_num = malloc(sizeof(int));
        if (task_num == NULL) {
            printf("内存分配失败\n");
            continue;
        }
        *task_num = i + 1;
        
        // 添加未命名任务（不提供任务名称）
        task_ids[i] = thread_pool_add_task(pool, simple_task, task_num, NULL, TASK_PRIORITY_NORMAL);
        
        if (task_ids[i] == 0) {
            printf("任务 #%d 添加失败\n", i + 1);
            free(task_num);
        } else {
            printf("未命名任务 #%d 已添加到队列，任务ID: %lu\n", i + 1, (unsigned long)task_ids[i]);
        }
    }
    
    // 打印线程池状态
    print_pool_status(pool);
    
    // 查找任务名称
    printf("=== 查找自动生成的任务名称 ===\n");
    for (int i = 0; i < num_tasks; i += 2) {
        task_id_t found_id = thread_pool_find_task_by_name(pool, NULL, NULL);
        if (found_id == 0) {
            printf("使用NULL查找任务失败，符合预期\n");
        }
        
        char expected_name[64];
        snprintf(expected_name, sizeof(expected_name), "unnamed_task_%lu", (unsigned long)task_ids[i]);
        
        int is_running = 0;
        found_id = thread_pool_find_task_by_name(pool, expected_name, &is_running);
        if (found_id != 0) {
            printf("找到自动生成的任务名称 '%s'，任务ID: %lu，%s\n", 
                   expected_name, (unsigned long)found_id, 
                   is_running ? "正在执行" : "在队列中等待");
        } else {
            printf("未找到任务名称 '%s'\n", expected_name);
        }
    }
    
    // 测试取消一些任务
    printf("\n=== 测试取消自动生成名称的任务 ===\n");
    for (int i = 1; i < num_tasks; i += 2) {
        char expected_name[64];
        snprintf(expected_name, sizeof(expected_name), "unnamed_task_%lu", (unsigned long)task_ids[i]);
        
        printf("尝试取消任务 '%s'...\n", expected_name);
        int result = thread_pool_cancel_task_by_name(pool, expected_name, NULL);
        
        if (result == 0) {
            printf("成功取消任务 '%s'\n", expected_name);
        } else {
            printf("无法取消任务 '%s'，错误码: %d\n", expected_name, result);
        }
    }
    
    // 再次打印线程池状态
    print_pool_status(pool);
    
    // 等待剩余任务完成
    printf("等待剩余任务完成...\n");
    sleep(10);
    
    // 最终状态
    print_pool_status(pool);
    
    // 销毁线程池
    thread_pool_destroy(pool);
    printf("线程池已销毁\n");
    
    return 0;
}
