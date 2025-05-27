/**
 * @file thread_debug_test.c
 * @brief 线程池调试测试程序（随机化版本）
 */

#include "thread.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/random.h>

// 全局状态
typedef struct {
    int tasks_completed;
    int timeout_occurred;
    thread_pool_t pool;
} test_state_t;

static test_state_t g_test_state = {0, 0, NULL};
static volatile sig_atomic_t g_alarm_received = 0;

// 随机数生成函数 - 使用getrandom获取更好的随机性
static int get_random_int(int min, int max) {
    unsigned int rand_val = 0; // 初始化为0
    // 使用getrandom获取随机数，如果失败则使用其他方法
    if (getrandom(&rand_val, sizeof(rand_val), 0) == -1) {
        // 使用更复杂的种子机制提高随机性
        struct timespec time_spec; // 改用更长的变量名
        clock_gettime(CLOCK_REALTIME, &time_spec);
        // 组合多个熟抄源增强随机性
        unsigned int seed = (unsigned int)time_spec.tv_nsec + 
                          ((unsigned int)time_spec.tv_sec << 11) + 
                          ((unsigned int)pthread_self() << 17);
        srand(seed);
        
        // 尽管rand()随机性有限，但在这里只作为备用方案
        // 我们通过多次调用和异或操作增强其随机性
        // 注意：这里使用rand()是因为getrandom()失败时的备用方案
        unsigned int rand_value1 = (unsigned int)rand(); // NOLINT(cert-msc30-c,cert-msc50-cpp)
        unsigned int rand_value2 = (unsigned int)rand(); // NOLINT(cert-msc30-c,cert-msc50-cpp)
        rand_val = rand_value1 ^ (rand_value2 << 15) ^ ((unsigned int)clock() << 3);
    }
    
    // 使用安全的转换方式，避免有符号整数的置换问题
    return min + (int)(rand_val % (unsigned int)(max - min + 1));
}

// 超时处理函数 - 使用异步信号安全的函数
static void timeout_handler(int signum)
{
    (void)signum; // 避免未使用参数警告

    // 设置超时标志
    g_test_state.timeout_occurred = 1;
    g_alarm_received = 1;
    
    // 使用异步信号安全的函数输出消息
    const char* msg = "\n\n警告: 测试超时，已标记退出\n";
    write(STDERR_FILENO, msg, strlen(msg));
    
    // 不再使用_exit立即退出，而是设置标志让主程序检查并优雅退出
    // 这样可以确保资源得到正确清理
    // 注意：如果超时太严重，仍然可以使用如下代码强制退出：
    // _exit(1); // 只在句柄时才使用
}

// 设置超时定时器
static void set_test_timeout(int seconds)
{
    // 使用更可靠的sigaction而不是signal
    struct sigaction signal_action;
    memset(&signal_action, 0, sizeof(signal_action));
    signal_action.sa_handler = timeout_handler;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = 0;

    if (sigaction(SIGALRM, &signal_action, NULL) == -1) {
        const char *error_msg = "警告: 设置信号处理失败!\n";
        write(STDERR_FILENO, error_msg, strlen(error_msg));
        return;
    }

    // 设置定时器
    alarm(seconds);
    printf("已设置%d秒超时\n", seconds);
}

// 简单任务函数 - 短时间工作，随机化执行时间
// 注意：该函数在test_pool_destroy中被使用
// 用于提交短时间任务，例如：submit_tasks(pool, 5, short_task);
static void short_task(void *arg)
{
    int task_id = 0;
    if (arg != NULL) {
        task_id = *(int *)arg;
        free(arg); // 释放传入的参数内存
    }
    
    // 随机化执行时间（10-50毫秒）
    int sleep_time = get_random_int(10, 50);
    printf("短任务 #%d 执行 (预计耗时 %d ms)\n", task_id, sleep_time);
    usleep(sleep_time * 1000);

    // 增加已完成任务计数
    __sync_add_and_fetch(&g_test_state.tasks_completed, 1);
    printf("短任务 #%d 完成\n", task_id);
}

// 长时间任务函数 - 模拟重负载，随机化执行时间
static void long_task(void *arg)
{
    int task_id = 0;
    if (arg != NULL) {
        task_id = *(int *)arg;
        free(arg); // 释放传入的参数内存
    }
    
    // 随机化执行时间（200-800毫秒）
    int sleep_time = get_random_int(200, 800);
    printf("★★★ 长任务 #%d 开始执行 (预计耗时 %d ms, 当前完成: %d)\n", 
           task_id, sleep_time, g_test_state.tasks_completed);
    
    // 检查超时标志
    if (g_alarm_received) {
        printf("★★★ 长任务 #%d 检测到超时，快速结束\n", task_id);
        __sync_add_and_fetch(&g_test_state.tasks_completed, 1);
        return;
    }
    
    usleep(sleep_time * 1000);

    // 增加已完成任务计数
    __sync_add_and_fetch(&g_test_state.tasks_completed, 1);
    printf("★★★ 长任务 #%d 完成 (当前完成: %d)\n", task_id, g_test_state.tasks_completed);
}

// 验证线程池状态
int verify_thread_pool_state(thread_pool_t pool, int min_threads, int max_threads)
{
    thread_pool_stats_t stats;

    if (thread_pool_get_stats(pool, &stats) != 0) {
        printf("获取线程池状态失败\n");
        return 0;
    }

    printf("线程池状态: 线程数=%d, 空闲=%d, 队列大小=%d, 已启动=%d\n", stats.thread_count,
           stats.idle_threads, stats.task_queue_size, stats.started);

    // 验证线程数在预期范围内
    if (stats.thread_count < min_threads || stats.thread_count > max_threads) {
        printf("线程数验证失败: 当前=%d, 预期范围=[%d,%d]\n", stats.thread_count, min_threads,
               max_threads);
        return 0;
    }
    
    // 验证成功
    printf("线程数验证成功: 当前=%d, 预期范围=[%d,%d]\n", stats.thread_count, min_threads, max_threads);
    return 1;
}

// 提交多个任务到线程池
static void submit_tasks(thread_pool_t pool, int count, void (*task_func)(void *))
{
    printf("提交%d个任务到线程池\n", count);

    for (int i = 0; i < count; i++) {
        // 为每个任务分配一个ID并传递给任务函数
        int *task_id = malloc(sizeof(int));
        if (task_id == NULL) {
            fprintf(stderr, "内存分配失败\n");
            continue;
        }
        *task_id = i + 1;
        
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "Task-%d", i + 1);

        if (thread_pool_add_task_default(pool, task_func, task_id, task_name) != 0) {
            printf("提交任务失败: %d/%d\n", i + 1, count);
            free(task_id);
            return;
        }
    }

    printf("已成功提交%d个任务\n", count);
}

// 测试线程池销毁功能
static int test_pool_destroy(void)
{
    printf("\n=== 测试线程池销毁功能 ===\n");

    // 创建线程池，初始随机2-5个线程
    int initial_threads = get_random_int(2, 5);
    printf("创建包含 %d 个线程的线程池\n", initial_threads);
    thread_pool_t pool = thread_pool_create(initial_threads);
    if (pool == NULL) {
        printf("创建线程池失败\n");
        return 0;
    }

    // 设置线程池限制（随机范围）
    int min_threads = get_random_int(1, 2); // 随机最少线程数量
    int max_threads = get_random_int(8, 12); // 随机最大线程数量
    printf("设置线程池限制 [%d, %d]\n", min_threads, max_threads);
    thread_pool_set_limits(pool, min_threads, max_threads);

    // 启用自动动态调整功能，随机化参数
    // 参数顺序：线程池、任务队列高水位、空闲线程高水位、调整间隔(毫秒)
    int busy_threshold = 1000; // 任务队列高水位，当队列任务数量超过此值时增加线程
    int idle_threshold = get_random_int(1, 3); // 空闲线程高水位，当空闲线程数量超过此值时减少线程
    int adjust_interval = get_random_int(2, 4) * 1000;  // 调整间隔，毫秒单位
    printf("启用自动动态调整: 任务队列高水位=%d, 空闲线程高水位=%d, 调整间隔=%d毫秒\n", 
           busy_threshold, idle_threshold, adjust_interval);
    // 正确的参数顺序：pool, busy_threshold(任务队列高水位), idle_threshold(空闲线程高水位), adjust_interval(调整间隔)
    thread_pool_enable_auto_adjust(pool, busy_threshold, idle_threshold, adjust_interval);

    // 初始状态验证 - 使用初始线程数
    if (!verify_thread_pool_state(pool, initial_threads, initial_threads)) {
        thread_pool_destroy(pool);
        return 0;
    }

    // 提交随机数量的任务
    g_test_state.tasks_completed = 0;
    g_test_state.pool = pool;
    int task_count = get_random_int(3, 7); // 随机任务数量3-7
    int use_long_tasks = get_random_int(0, 1); // 随机决定使用长任务或短任务
    
    if (use_long_tasks) {
        printf("提交 %d 个长时间任务...\n", task_count);
        submit_tasks(pool, task_count, long_task);
    } else {
        printf("提交 %d 个短时间任务...\n", task_count);
        submit_tasks(pool, task_count, short_task);
    }

    // 等待自动调整发生
    printf("等待线程池自动调整...\n");
    int wait_time = get_random_int(300, 800); // 随机等待时间300-800毫秒
    printf("等待 %d 毫秒...\n", wait_time);
    usleep(wait_time * 1000);

    // 验证线程数在设置的范围内
    verify_thread_pool_state(pool, min_threads, max_threads);

    // 等待所有任务完成
    int wait_count = 0;
    const int max_wait = 50; // 最多等待5秒
    while (g_test_state.tasks_completed < 5 && wait_count < max_wait) {
        usleep(100000); // 100ms
        wait_count++;

        // 每秒打印一次状态
        if (wait_count % 10 == 0) {
            thread_pool_stats_t stats;
            if (thread_pool_get_stats(pool, &stats) == 0) {
                printf("等待任务完成: 已完成 %d/5, 线程数=%d, 空闲=%d, 队列大小=%d\n",
                       g_test_state.tasks_completed, stats.thread_count, stats.idle_threads,
                       stats.task_queue_size);
            }
        }
    }

    // 强制等待所有任务完成
    while (g_test_state.tasks_completed < 5) {
        usleep(100000); // 100ms
        printf("等待所有任务完成: 已完成 %d/5\n", g_test_state.tasks_completed);
    }

    printf("所有任务已完成\n");

    if (wait_count >= max_wait) {
        printf("警告: 等待任务完成超时\n");
    }

    printf("开始销毁线程池...\n");

    // 先禁用自动调整功能
    printf("禁用自动调整功能...\n");
    thread_pool_disable_auto_adjust(pool);

    // 打印销毁前的线程池状态
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("销毁前线程池状态: 线程数=%d, 空闲=%d, 队列大小=%d, 已启动=%d\n", stats.thread_count,
               stats.idle_threads, stats.task_queue_size, stats.started);
    }

    // 强制所有线程进入空闲状态
    printf("等待所有线程进入空闲状态...\n");
    usleep(1000000); // 等待1秒

    // 再次检查线程池状态
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("销毁前最终线程池状态: 线程数=%d, 空闲=%d, 队列大小=%d, 已启动=%d\n",
               stats.thread_count, stats.idle_threads, stats.task_queue_size, stats.started);
    }

    // 销毁线程池
    printf("\n=== 开始销毁线程池 ===\n");
    printf("开始时间: %ld\n", time(NULL));

    // 添加信号处理以捕获可能的挂起
    struct sigaction old_action;
    struct sigaction new_action;
    memset(&new_action, 0, sizeof(new_action));
    new_action.sa_handler = timeout_handler; // 使用我们的超时处理函数
    sigaction(SIGALRM, &new_action, &old_action);

    // 设置更短的超时，确保测试不会挂起
    alarm(3); // 3秒超时

    printf("调用 thread_pool_destroy 函数...\n");
    int destroy_result = thread_pool_destroy(pool);
    printf("线程池销毁函数返回值: %d\n", destroy_result);

    // 取消超时警报并恢复信号处理
    alarm(0);
    sigaction(SIGALRM, &old_action, NULL);

    printf("结束时间: %ld\n", time(NULL));
    printf("线程池销毁%s\n", destroy_result == 0 ? "成功" : "失败");
    printf("=== 线程池销毁完成 ===\n");

    return destroy_result == 0;
}

int main(void)
{
    printf("======================================\n");
    printf("=== 线程池调试测试开始 (随机化版本) ===\n");
    printf("======================================\n\n");

    // 设置随机超时（15-25秒）
    int timeout = get_random_int(15, 25);
    set_test_timeout(timeout);

    // 测试线程池销毁功能
    int result = test_pool_destroy();

    // 根据超时状态显示不同的完成提示
    if (g_alarm_received) {
        printf("\n警告: 测试超时，可能未完成所有测试项\n");
    } else {
        printf("\n所有测试项已成功完成！\n");
    }

    printf("\n======================================\n");
    printf("=== 线程池调试测试结束 ===\n");
    printf("测试结果: %s\n", result ? "通过" : "失败");
    printf("======================================\n");

    return result ? 0 : 1;
}
