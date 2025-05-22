#include "../../../../src/core/thread/include/thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

// 全局计数器，用于跟踪完成的任务数量
static int completed_tasks = 0;

// 测试任务函数
void test_task(void *arg) {
    int task_id = *(int*)arg;
    int sleep_time = rand() % 100;
    
    // 模拟任务执行
    usleep(sleep_time * 1000);
    
    printf("任务 #%d 已完成 (休眠了 %d ms)\n", task_id, sleep_time);
    
    // 增加完成的任务计数
    __sync_fetch_and_add(&completed_tasks, 1);
    
    // 释放参数内存
    free(arg);
}

// 测试基本功能
void test_basic_functionality() {
    const int NUM_THREADS = 4;
    const int NUM_TASKS = 20;
    
    printf("\n=== 测试线程池基本功能 ===\n");
    
    // 创建线程池
    thread_pool_t pool = thread_pool_create(NUM_THREADS);
    assert(pool != NULL);
    printf("成功创建包含 %d 个线程的线程池\n", NUM_THREADS);
    
    // 重置任务完成计数
    completed_tasks = 0;
    
    // 添加任务到线程池
    for (int i = 0; i < NUM_TASKS; i++) {
        int *task_id = malloc(sizeof(int));
        *task_id = i;
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "Task-%d", i);
        
        int result = thread_pool_add_task(pool, test_task, task_id, task_name);
        assert(result == 0);
        printf("已添加任务 #%d\n", i);
    }
    
    // 检查当前运行的任务
    printf("\n=== 当前运行的任务 ===\n");
    char **tasks = thread_pool_get_running_task_names(pool);
    assert(tasks != NULL);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("线程 #%d: %s\n", i, tasks[i]);
    }
    
    // 释放任务名称数组
    free_running_task_names(tasks, NUM_THREADS);
    
    // 等待所有任务完成
    printf("进度: %d/%d 任务已完成\n", completed_tasks, NUM_TASKS);
    while (completed_tasks < NUM_TASKS) {
        usleep(10000); // 10ms
    }
    
    // 再次检查当前运行的任务
    printf("\n=== 当前运行的任务 ===\n");
    tasks = thread_pool_get_running_task_names(pool);
    assert(tasks != NULL);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("线程 #%d: %s\n", i, tasks[i]);
    }
    
    // 释放任务名称数组
    free_running_task_names(tasks, NUM_THREADS);
    
    printf("\n所有 %d 个任务已完成\n", NUM_TASKS);
    
    // 销毁线程池
    int result = thread_pool_destroy(pool);
    assert(result == 0);
    printf("线程池已成功销毁\n");
}

// 测试错误处理
void test_error_handling() {
    printf("\n=== 测试错误处理 ===\n");
    
    // 测试无效参数
    thread_pool_t pool = thread_pool_create(0);
    assert(pool == NULL);
    printf("测试通过: 无法创建线程数为0的线程池\n");
    
    // 创建有效的线程池用于后续测试
    pool = thread_pool_create(2);
    assert(pool != NULL);
    
    // 测试无效的任务函数
    int result = thread_pool_add_task(pool, NULL, NULL, "invalid-task");
    assert(result != 0);
    printf("测试通过: 无法添加函数指针为NULL的任务\n");
    
    // 测试无效的线程池指针
    result = thread_pool_add_task(NULL, test_task, NULL, "invalid-pool");
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

int main() {
    printf("=== 线程池单元测试 ===\n");
    
    // 初始化随机数生成器
    srand(time(NULL));
    
    // 运行测试
    test_basic_functionality();
    test_error_handling();
    
    printf("\n=== 所有测试通过 ===\n");
    return 0;
}
