/**
 * @file thread_cancel_test.c
 * @brief 线程池任务取消功能的单元测试
 *
 * 此测试程序验证线程池的任务取消和任务存在性检查功能是否正常工作。
 * 测试包括：
 * 1. 取消队列中的任务
 * 2. 尝试取消正在运行的任务（应该失败）
 * 3. 检查任务存在性（运行中、队列中、不存在的任务）
 * 4. 取消回调函数的调用
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <thread.h>

// 测试状态
typedef struct {
    int tasks_created;
    int tasks_started;
    int tasks_completed;
    int tasks_cancelled;
    pthread_mutex_t lock;
} test_stats_t;

// 全局测试状态
test_stats_t g_stats = {0};

// 初始化测试状态
void init_test_stats(void) {
    memset(&g_stats, 0, sizeof(test_stats_t));
    pthread_mutex_init(&g_stats.lock, NULL);
}

// 清理测试状态
void cleanup_test_stats(void) {
    pthread_mutex_destroy(&g_stats.lock);
}

// 短时间任务函数
void short_task(void *arg) {
    int task_num = *((int *)arg);
    
    // 更新任务开始计数
    pthread_mutex_lock(&g_stats.lock);
    g_stats.tasks_started++;
    pthread_mutex_unlock(&g_stats.lock);
    
    printf("短时间任务 %d 开始执行\n", task_num);
    usleep(100000); // 休眠0.1秒
    printf("短时间任务 %d 完成\n", task_num);
    
    // 更新任务完成计数
    pthread_mutex_lock(&g_stats.lock);
    g_stats.tasks_completed++;
    pthread_mutex_unlock(&g_stats.lock);
}

// 长时间任务函数
void long_task(void *arg) {
    int task_num = *((int *)arg);
    
    // 更新任务开始计数
    pthread_mutex_lock(&g_stats.lock);
    g_stats.tasks_started++;
    pthread_mutex_unlock(&g_stats.lock);
    
    printf("长时间任务 %d 开始执行\n", task_num);
    for (int i = 0; i < 3; i++) {
        printf("长时间任务 %d 正在执行: %d/3\n", task_num, i+1);
        usleep(300000); // 每步休眠0.3秒
    }
    printf("长时间任务 %d 完成\n", task_num);
    
    // 更新任务完成计数
    pthread_mutex_lock(&g_stats.lock);
    g_stats.tasks_completed++;
    pthread_mutex_unlock(&g_stats.lock);
}

// 取消回调函数
void cancel_callback(void *arg, task_id_t task_id) {
    int task_num = *((int *)arg);
    printf("任务 %d (ID: %lu) 已被取消\n", task_num, (unsigned long)task_id);
    
    // 更新任务取消计数
    pthread_mutex_lock(&g_stats.lock);
    g_stats.tasks_cancelled++;
    pthread_mutex_unlock(&g_stats.lock);
}

// 测试1：取消队列中的任务
void test_cancel_queued_tasks(void) {
    printf("\n=== 测试1：取消队列中的任务 ===\n");
    
    // 重置测试状态
    init_test_stats();
    
    // 创建线程池，2个线程
    thread_pool_t pool = thread_pool_create(2);
    assert(pool != NULL);
    
    // 为任务参数分配内存
    int *task_nums = malloc(10 * sizeof(int));
    assert(task_nums != NULL);
    
    // 添加10个任务到线程池（前2个会立即开始执行，其余8个会在队列中等待）
    task_id_t task_ids[10];
    for (int i = 0; i < 10; i++) {
        task_nums[i] = i + 1;
        task_ids[i] = thread_pool_add_task(pool, short_task, &task_nums[i], "测试任务", 0);
        assert(task_ids[i] != 0);
        
        // 更新任务创建计数
        pthread_mutex_lock(&g_stats.lock);
        g_stats.tasks_created++;
        pthread_mutex_unlock(&g_stats.lock);
        
        printf("添加任务 %d，任务ID: %lu\n", i+1, (unsigned long)task_ids[i]);
    }
    
    // 等待一段时间，让前面的任务开始执行
    usleep(50000); // 0.05秒
    
    // 尝试取消所有任务
    int cancelled_count = 0;
    for (int i = 0; i < 10; i++) {
        int is_running = 0;
        int exists = thread_pool_task_exists(pool, task_ids[i], &is_running);
        
        if (exists == 1 && !is_running) {
            printf("尝试取消任务 %d (ID: %lu)...\n", i+1, (unsigned long)task_ids[i]);
            int result = thread_pool_cancel_task(pool, task_ids[i], cancel_callback);
            if (result == 0) {
                cancelled_count++;
            }
        }
    }
    
    // 等待所有剩余任务完成
    thread_pool_stats_t stats;
    do {
        usleep(100000); // 每0.1秒检查一次
        thread_pool_get_stats(pool, &stats);
    } while (stats.task_queue_size > 0 || stats.idle_threads < stats.thread_count);
    
    // 验证结果
    printf("任务创建数: %d\n", g_stats.tasks_created);
    printf("任务开始数: %d\n", g_stats.tasks_started);
    printf("任务完成数: %d\n", g_stats.tasks_completed);
    printf("任务取消数: %d\n", g_stats.tasks_cancelled);
    
    assert(g_stats.tasks_created == 10);
    assert(g_stats.tasks_started + g_stats.tasks_cancelled == 10);
    assert(g_stats.tasks_cancelled == cancelled_count);
    
    // 清理资源
    thread_pool_destroy(pool);
    free(task_nums);
    cleanup_test_stats();
    
    printf("测试1通过！\n");
}

// 测试2：尝试取消正在运行的任务（应该失败）
void test_cancel_running_tasks(void) {
    printf("\n=== 测试2：尝试取消正在运行的任务 ===\n");
    
    // 重置测试状态
    init_test_stats();
    
    // 创建线程池，2个线程
    thread_pool_t pool = thread_pool_create(2);
    assert(pool != NULL);
    
    // 为任务参数分配内存
    int *task_nums = malloc(2 * sizeof(int));
    assert(task_nums != NULL);
    
    // 添加2个长时间运行的任务
    task_id_t task_ids[2];
    for (int i = 0; i < 2; i++) {
        task_nums[i] = i + 1;
        task_ids[i] = thread_pool_add_task(pool, long_task, &task_nums[i], "长时间任务", 0);
        assert(task_ids[i] != 0);
        
        // 更新任务创建计数
        pthread_mutex_lock(&g_stats.lock);
        g_stats.tasks_created++;
        pthread_mutex_unlock(&g_stats.lock);
        
        printf("添加长时间任务 %d，任务ID: %lu\n", i+1, (unsigned long)task_ids[i]);
    }
    
    // 等待任务开始执行
    usleep(100000); // 0.1秒
    
    // 尝试取消正在运行的任务（应该失败）
    int cancel_failures = 0;
    for (int i = 0; i < 2; i++) {
        int is_running = 0;
        int exists = thread_pool_task_exists(pool, task_ids[i], &is_running);
        
        if (exists == 1 && is_running) {
            printf("尝试取消正在运行的任务 %d (ID: %lu)...\n", i+1, (unsigned long)task_ids[i]);
            int result = thread_pool_cancel_task(pool, task_ids[i], cancel_callback);
            if (result == -1) {
                cancel_failures++;
                printf("预期的失败：无法取消正在运行的任务 %d\n", i+1);
            }
        }
    }
    
    // 等待所有任务完成
    thread_pool_stats_t stats;
    do {
        usleep(100000); // 每0.1秒检查一次
        thread_pool_get_stats(pool, &stats);
    } while (stats.task_queue_size > 0 || stats.idle_threads < stats.thread_count);
    
    // 验证结果
    printf("任务创建数: %d\n", g_stats.tasks_created);
    printf("任务开始数: %d\n", g_stats.tasks_started);
    printf("任务完成数: %d\n", g_stats.tasks_completed);
    printf("任务取消数: %d\n", g_stats.tasks_cancelled);
    printf("取消失败数: %d\n", cancel_failures);
    
    assert(g_stats.tasks_created == 2);
    assert(g_stats.tasks_started == 2);
    assert(g_stats.tasks_completed == 2);
    assert(g_stats.tasks_cancelled == 0);
    assert(cancel_failures == 2);
    
    // 清理资源
    thread_pool_destroy(pool);
    free(task_nums);
    cleanup_test_stats();
    
    printf("测试2通过！\n");
}

// 测试3：检查任务存在性
void test_task_existence(void) {
    printf("\n=== 测试3：检查任务存在性 ===\n");
    
    // 重置测试状态
    init_test_stats();
    
    // 创建线程池，1个线程
    thread_pool_t pool = thread_pool_create(1);
    assert(pool != NULL);
    
    // 为任务参数分配内存
    int *task_nums = malloc(3 * sizeof(int));
    assert(task_nums != NULL);
    
    // 添加3个任务：1个长任务和2个短任务
    task_id_t task_ids[3];
    
    // 添加长任务（会立即开始执行）
    task_nums[0] = 1;
    task_ids[0] = thread_pool_add_task(pool, long_task, &task_nums[0], "长时间任务", 0);
    assert(task_ids[0] != 0);
    g_stats.tasks_created++;
    
    // 添加两个短任务（会在队列中等待）
    for (int i = 1; i < 3; i++) {
        task_nums[i] = i + 1;
        task_ids[i] = thread_pool_add_task(pool, short_task, &task_nums[i], "短时间任务", 0);
        assert(task_ids[i] != 0);
        g_stats.tasks_created++;
    }
    
    // 等待长任务开始执行
    usleep(100000); // 0.1秒
    
    // 检查任务存在性
    int running_count = 0;
    int queued_count = 0;
    
    for (int i = 0; i < 3; i++) {
        int is_running = 0;
        int exists = thread_pool_task_exists(pool, task_ids[i], &is_running);
        
        assert(exists == 1); // 所有任务都应该存在
        
        if (is_running) {
            running_count++;
            printf("任务 %d (ID: %lu) 正在运行\n", i+1, (unsigned long)task_ids[i]);
        } else {
            queued_count++;
            printf("任务 %d (ID: %lu) 在队列中等待\n", i+1, (unsigned long)task_ids[i]);
        }
    }
    
    // 验证结果
    printf("运行中的任务数: %d\n", running_count);
    printf("队列中的任务数: %d\n", queued_count);
    
    assert(running_count == 1); // 应该有1个任务正在运行
    assert(queued_count == 2);  // 应该有2个任务在队列中
    
    // 取消一个队列中的任务
    for (int i = 0; i < 3; i++) {
        int is_running = 0;
        thread_pool_task_exists(pool, task_ids[i], &is_running);
        
        if (!is_running) {
            printf("取消任务 %d (ID: %lu)\n", i+1, (unsigned long)task_ids[i]);
            int result = thread_pool_cancel_task(pool, task_ids[i], cancel_callback);
            assert(result == 0);
            break; // 只取消第一个找到的队列中的任务
        }
    }
    
    // 等待一段时间
    usleep(200000); // 0.2秒
    
    // 检查被取消任务的存在性
    int non_existent_count = 0;
    for (int i = 0; i < 3; i++) {
        int exists = thread_pool_task_exists(pool, task_ids[i], NULL);
        if (exists == 0) {
            non_existent_count++;
            printf("任务 %d (ID: %lu) 不存在（已被取消）\n", i+1, (unsigned long)task_ids[i]);
        }
    }
    
    assert(non_existent_count == 1); // 应该有1个任务不存在（被取消）
    
    // 等待所有任务完成
    thread_pool_stats_t stats;
    do {
        usleep(100000); // 每0.1秒检查一次
        thread_pool_get_stats(pool, &stats);
    } while (stats.task_queue_size > 0 || stats.idle_threads < stats.thread_count);
    
    // 再次检查所有任务的存在性
    non_existent_count = 0;
    for (int i = 0; i < 3; i++) {
        int exists = thread_pool_task_exists(pool, task_ids[i], NULL);
        if (exists == 0) {
            non_existent_count++;
        }
    }
    
    printf("所有任务完成后，不存在的任务数: %d\n", non_existent_count);
    assert(non_existent_count == 3); // 所有任务都应该不存在（完成或取消）
    
    // 清理资源
    thread_pool_destroy(pool);
    free(task_nums);
    cleanup_test_stats();
    
    printf("测试3通过！\n");
}

// 测试4：无效参数处理
void test_invalid_parameters(void) {
    printf("\n=== 测试4：无效参数处理 ===\n");
    
    // 创建线程池
    thread_pool_t pool = thread_pool_create(1);
    assert(pool != NULL);
    
    // 测试取消函数的无效参数
    int result = thread_pool_cancel_task(NULL, 1, NULL);
    assert(result == -2);
    printf("thread_pool_cancel_task(NULL, 1, NULL) 返回 %d（预期 -2）\n", result);
    
    result = thread_pool_cancel_task(pool, 0, NULL);
    assert(result == -2);
    printf("thread_pool_cancel_task(pool, 0, NULL) 返回 %d（预期 -2）\n", result);
    
    // 测试存在性检查函数的无效参数
    int exists = thread_pool_task_exists(NULL, 1, NULL);
    assert(exists == -1);
    printf("thread_pool_task_exists(NULL, 1, NULL) 返回 %d（预期 -1）\n", exists);
    
    exists = thread_pool_task_exists(pool, 0, NULL);
    assert(exists == -1);
    printf("thread_pool_task_exists(pool, 0, NULL) 返回 %d（预期 -1）\n", exists);
    
    // 测试不存在的任务ID
    exists = thread_pool_task_exists(pool, 999999, NULL);
    assert(exists == 0);
    printf("thread_pool_task_exists(pool, 999999, NULL) 返回 %d（预期 0）\n", exists);
    
    result = thread_pool_cancel_task(pool, 999999, NULL);
    assert(result == -1);
    printf("thread_pool_cancel_task(pool, 999999, NULL) 返回 %d（预期 -1）\n", result);
    
    // 清理资源
    thread_pool_destroy(pool);
    
    printf("测试4通过！\n");
}

int main(void) {
    printf("=== 线程池任务取消功能测试 ===\n");
    
    // 运行所有测试
    test_cancel_queued_tasks();
    test_cancel_running_tasks();
    test_task_existence();
    test_invalid_parameters();
    
    printf("\n所有测试通过！\n");
    return 0;
}
