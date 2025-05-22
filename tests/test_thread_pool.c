/**
 * @file test_thread_pool.c
 * @brief 线程池库的基本测试程序
 *
 * 此测试程序验证线程池的基本功能，包括创建线程池、
 * 添加任务、获取运行中的任务名称以及销毁线程池。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include "thread.h"

// 定义测试任务数量
#define NUM_TASKS 20
#define NUM_THREADS 4

// 互斥锁用于保护共享数据
pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
// 已完成的任务计数
int completed_tasks = 0;

/**
 * @brief 简单的测试任务函数
 *
 * 此函数模拟一个工作负载，休眠一段随机时间，
 * 然后增加已完成任务的计数。
 *
 * @param arg 传递给任务的参数，包含任务ID
 */
void test_task(void *arg) {
    int task_id = *((int*)arg);
    
    // 模拟工作负载 (随机休眠0-100毫秒)
    int sleep_time = rand() % 100;
    usleep(sleep_time * 1000);
    
    // 更新已完成任务计数
    pthread_mutex_lock(&test_mutex);
    printf("任务 #%d 已完成 (休眠了 %d ms)\n", task_id, sleep_time);
    completed_tasks++;
    pthread_mutex_unlock(&test_mutex);
    
    free(arg); // 释放在主函数中分配的任务ID内存
}

/**
 * @brief 测试线程池的基本功能
 *
 * @return 成功时返回0，失败时返回非零值
 */
int test_basic_functionality() {
    printf("\n=== 测试线程池基本功能 ===\n");
    
    // 创建线程池
    thread_pool_t pool = thread_pool_create(NUM_THREADS);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    printf("成功创建包含 %d 个线程的线程池\n", NUM_THREADS);
    
    // 重置已完成任务计数
    completed_tasks = 0;
    
    // 添加任务到线程池
    for (int i = 0; i < NUM_TASKS; i++) {
        int *task_id = malloc(sizeof(int));
        if (task_id == NULL) {
            fprintf(stderr, "为任务ID分配内存失败\n");
            thread_pool_destroy(pool);
            return 1;
        }
        
        *task_id = i;
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "Task-%d", i);
        
        if (thread_pool_add_task(pool, test_task, task_id, task_name) != 0) {
            fprintf(stderr, "添加任务 #%d 失败\n", i);
            free(task_id);
            thread_pool_destroy(pool);
            return 1;
        }
        
        printf("已添加任务 #%d\n", i);
    }
    
    // 获取并显示正在运行的任务名称
    printf("\n=== 当前运行的任务 ===\n");
    char **running_tasks = thread_pool_get_running_task_names(pool);
    if (running_tasks != NULL) {
        for (int i = 0; i < NUM_THREADS; i++) {
            printf("线程 #%d: %s\n", i, running_tasks[i]);
        }
        free_running_task_names(running_tasks, NUM_THREADS);
    } else {
        fprintf(stderr, "获取运行中的任务名称失败\n");
    }
    
    // 等待所有任务完成
    while (1) {
        pthread_mutex_lock(&test_mutex);
        int current_completed = completed_tasks;
        pthread_mutex_unlock(&test_mutex);
        
        if (current_completed == NUM_TASKS) {
            break;
        }
        
        // 每秒检查一次并显示进度
        printf("进度: %d/%d 任务已完成\n", current_completed, NUM_TASKS);
        sleep(1);
        
        // 再次获取并显示正在运行的任务名称
        running_tasks = thread_pool_get_running_task_names(pool);
        if (running_tasks != NULL) {
            printf("\n=== 当前运行的任务 ===\n");
            for (int i = 0; i < NUM_THREADS; i++) {
                printf("线程 #%d: %s\n", i, running_tasks[i]);
            }
            free_running_task_names(running_tasks, NUM_THREADS);
        }
    }
    
    printf("\n所有 %d 个任务已完成\n", NUM_TASKS);
    
    // 销毁线程池
    if (thread_pool_destroy(pool) != 0) {
        fprintf(stderr, "销毁线程池失败\n");
        return 1;
    }
    printf("线程池已成功销毁\n");
    
    return 0;
}

/**
 * @brief 测试错误处理
 *
 * 测试线程池API在各种错误情况下的行为
 *
 * @return 成功时返回0，失败时返回非零值
 */
int test_error_handling() {
    printf("\n=== 测试错误处理 ===\n");
    
    // 测试无效参数
    thread_pool_t pool = thread_pool_create(0);
    assert(pool == NULL && "应该无法创建线程数为0的线程池");
    printf("测试通过: 无法创建线程数为0的线程池\n");
    
    // 创建有效的线程池用于后续测试
    pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 测试无效的任务函数
    int result = thread_pool_add_task(pool, NULL, NULL, "invalid-task");
    assert(result != 0 && "应该无法添加函数指针为NULL的任务");
    printf("测试通过: 无法添加函数指针为NULL的任务\n");
    
    // 测试无效的线程池指针
    result = thread_pool_add_task(NULL, test_task, NULL, "invalid-pool");
    assert(result != 0 && "应该无法向NULL线程池添加任务");
    printf("测试通过: 无法向NULL线程池添加任务\n");
    
    // 测试获取运行任务名称的错误处理
    char **tasks = thread_pool_get_running_task_names(NULL);
    assert(tasks == NULL && "从NULL线程池获取任务名称应该返回NULL");
    printf("测试通过: 从NULL线程池获取任务名称返回NULL\n");
    
    // 测试销毁NULL线程池
    result = thread_pool_destroy(NULL);
    assert(result != 0 && "销毁NULL线程池应该返回错误");
    printf("测试通过: 销毁NULL线程池返回错误\n");
    
    // 清理
    thread_pool_destroy(pool);
    printf("错误处理测试全部通过\n");
    
    return 0;
}

/**
 * @brief 主函数
 *
 * 运行所有测试用例
 *
 * @return 成功时返回0，失败时返回非零值
 */
int main() {
    // 初始化随机数生成器
    srand(time(NULL));
    
    printf("=== 线程池测试程序 ===\n");
    
    // 运行基本功能测试
    if (test_basic_functionality() != 0) {
        fprintf(stderr, "基本功能测试失败\n");
        return 1;
    }
    
    // 运行错误处理测试
    if (test_error_handling() != 0) {
        fprintf(stderr, "错误处理测试失败\n");
        return 1;
    }
    
    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
