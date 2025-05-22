#include "../../src/core/thread/include/thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

// 日志宏定义
#define LOG_PREFIX "[THREAD_EXAMPLE]"
#define LOG(fmt, ...) printf("%s " fmt "\n", LOG_PREFIX, ##__VA_ARGS__)

// 任务函数
void example_task(void *arg) {
    int task_id = *(int*)arg;
    int sleep_time = (task_id % 3 + 1) * 1000; // 1-3秒
    
    LOG("任务 %d 开始执行，将休眠 %d 毫秒", task_id, sleep_time);
    usleep(sleep_time * 1000);
    LOG("任务 %d 执行完成", task_id);
    
    free(arg);
}

int main() {
    LOG("线程池示例程序开始运行");
    
    // 创建线程池，包含4个工作线程
    LOG("创建包含4个工作线程的线程池");
    thread_pool_t pool = thread_pool_create(4);
    if (pool == NULL) {
        LOG("创建线程池失败");
        return 1;
    }
    
    // 添加任务到线程池
    const int NUM_TASKS = 10;
    LOG("向线程池添加 %d 个任务", NUM_TASKS);
    
    for (int i = 0; i < NUM_TASKS; i++) {
        int *task_id = malloc(sizeof(int));
        *task_id = i + 1;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "示例任务-%d", i + 1);
        
        if (thread_pool_add_task(pool, example_task, task_id, task_name) != 0) {
            LOG("添加任务 %d 失败", i + 1);
            free(task_id);
        } else {
            LOG("已添加任务 %d: %s", i + 1, task_name);
        }
    }
    
    // 等待一段时间，让任务开始执行
    LOG("等待2秒后查看任务执行状态");
    sleep(2);
    
    // 获取当前正在执行的任务名称
    LOG("当前正在执行的任务:");
    char **tasks = thread_pool_get_running_task_names(pool);
    if (tasks != NULL) {
        for (int i = 0; i < 4; i++) {
            LOG("线程 %d: %s", i, tasks[i]);
        }
        free_running_task_names(tasks, 4);
    }
    
    // 等待所有任务完成
    LOG("等待所有任务完成 (最多10秒)");
    for (int i = 0; i < 10; i++) {
        sleep(1);
        
        // 再次获取当前正在执行的任务名称
        tasks = thread_pool_get_running_task_names(pool);
        if (tasks != NULL) {
            int all_idle = 1;
            for (int j = 0; j < 4; j++) {
                if (strcmp(tasks[j], "[idle]") != 0) {
                    all_idle = 0;
                    break;
                }
            }
            free_running_task_names(tasks, 4);
            
            if (all_idle) {
                LOG("所有任务已完成");
                break;
            }
        }
    }
    
    // 销毁线程池
    LOG("销毁线程池");
    if (thread_pool_destroy(pool) != 0) {
        LOG("销毁线程池失败");
        return 1;
    }
    
    LOG("线程池示例程序运行完成");
    return 0;
}
