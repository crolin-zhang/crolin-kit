/**
 * @file thread_cancel_example.c
 * @brief 线程池任务取消功能示例程序
 *
 * 此示例演示如何使用CrolinKit线程池的任务取消和任务存在性检查功能。
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <thread.h>

// 全局变量，用于优雅退出
volatile sig_atomic_t g_shutdown_requested = 0;

// 信号处理函数
static void signal_handler(int signum) {
    (void)signum; // 标记参数为已使用
    g_shutdown_requested = 1;
}

// 任务函数 - 长时间运行的任务
void long_task(void *arg) {
    int task_num = *((int *)arg);
    printf("长时间任务 %d 开始执行\n", task_num);
    
    // 模拟长时间运行的任务
    for (int i = 0; i < 5; i++) {
        if (g_shutdown_requested) {
            break;
        }
        printf("长时间任务 %d 正在执行: %d/5\n", task_num, i+1);
        sleep(1);
    }
    
    printf("长时间任务 %d 完成\n", task_num);
}

// 任务函数 - 短时间运行的任务
void short_task(void *arg) {
    int task_num = *((int *)arg);
    printf("短时间任务 %d 开始执行\n", task_num);
    usleep(500000); // 休眠0.5秒
    printf("短时间任务 %d 完成\n", task_num);
}

// 取消回调函数
void cancel_callback(void *arg, task_id_t task_id) {
    int task_num = *((int *)arg);
    printf("任务 %d (ID: %lu) 已被取消\n", task_num, (unsigned long)task_id);
}

int main(void) {
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    // 创建线程池，4个线程
    thread_pool_t pool = thread_pool_create(4);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    
    // 设置线程池的最小和最大线程数量
    thread_pool_set_limits(pool, 2, 8);
    
    printf("线程池创建成功，4个线程\n");
    
    // 为任务参数分配内存
    int *task_nums = malloc(10 * sizeof(int));
    if (task_nums == NULL) {
        fprintf(stderr, "内存分配失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    
    // 添加多个任务到线程池
    task_id_t task_ids[10];
    for (int i = 0; i < 10; i++) {
        task_nums[i] = i + 1;
        
        // 偶数任务是长时间任务，奇数任务是短时间任务
        if (i % 2 == 0) {
            task_ids[i] = thread_pool_add_task(pool, long_task, &task_nums[i], "长时间任务", 0);
        } else {
            task_ids[i] = thread_pool_add_task(pool, short_task, &task_nums[i], "短时间任务", 0);
        }
        
        printf("添加任务 %d，任务ID: %lu\n", i+1, (unsigned long)task_ids[i]);
    }
    
    // 等待一段时间，让一些任务开始执行
    sleep(1);
    
    // 检查每个任务的状态并尝试取消一些任务
    for (int i = 0; i < 10; i++) {
        int is_running = 0;
        int exists = thread_pool_task_exists(pool, task_ids[i], &is_running);
        
        if (exists == 1) {
            printf("任务 %d (ID: %lu) 存在，%s\n", 
                   i+1, (unsigned long)task_ids[i], 
                   is_running ? "正在运行" : "在队列中等待");
            
            // 尝试取消队列中的任务（非运行中的任务）
            if (!is_running) {
                printf("尝试取消任务 %d (ID: %lu)...\n", i+1, (unsigned long)task_ids[i]);
                int result = thread_pool_cancel_task(pool, task_ids[i], cancel_callback);
                
                if (result == 0) {
                    printf("成功取消任务 %d\n", i+1);
                } else if (result == -1) {
                    printf("无法取消任务 %d，任务不存在或正在运行\n", i+1);
                } else {
                    printf("取消任务 %d 失败，参数无效\n", i+1);
                }
            }
        } else if (exists == 0) {
            printf("任务 %d (ID: %lu) 不存在（可能已完成或已被取消）\n", 
                   i+1, (unsigned long)task_ids[i]);
        } else {
            printf("检查任务 %d (ID: %lu) 存在性失败，参数无效\n", 
                   i+1, (unsigned long)task_ids[i]);
        }
    }
    
    // 等待所有任务完成
    printf("\n等待剩余任务完成...\n");
    
    // 等待任务完成（线程池没有内置thread_pool_wait函数，所以我们使用简单的轮询）
    thread_pool_stats_t stats;
    do {
        sleep(1); // 每秒检查一次
        thread_pool_get_stats(pool, &stats);
    } while (stats.task_queue_size > 0 || stats.idle_threads < stats.thread_count);
    
    // 清理资源
    thread_pool_destroy(pool);
    free(task_nums);
    
    printf("程序正常退出\n");
    return 0;
}
