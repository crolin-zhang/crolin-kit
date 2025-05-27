/**
 * @file thread_priority_example.c
 * @brief 演示线程池任务优先级功能的示例程序
 *
 * 该示例程序创建一个线程池，并添加不同优先级的任务，
 * 展示高优先级任务会优先于低优先级任务执行。
 */
#include "thread.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/random.h>

// 日志级别已在log.h中定义

// 全局变量，用于信号处理
volatile sig_atomic_t g_shutdown_requested = 0;
volatile sig_atomic_t g_tasks_completed = 0;
volatile sig_atomic_t g_total_tasks = 0;

// 信号处理函数
static void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\n收到Ctrl+C，正在优雅退出...\n");
        g_shutdown_requested = 1;
    }
}

// 生成随机数（0到max-1之间）
static int get_random(int max) {
    unsigned int value = 0; // 初始化为0，避免未初始化警告
    if (getrandom(&value, sizeof(value), 0) == -1) {
        // 如果getrandom失败，使用time作为备选
        value = (unsigned int)time(NULL);
    }
    return (int)(value % (unsigned int)max); // 显式转换，避免隐式转换警告
}

// 任务函数 - 模拟不同执行时间的工作
static void task_function(void *arg) {
    int task_id = *((int *)arg);
    int sleep_time = 100 + get_random(400); // 100-500ms的随机执行时间
    
    printf("开始执行任务 #%d (休眠 %d ms)\n", task_id, sleep_time);
    
    // 模拟工作
    usleep(sleep_time * 1000);
    
    printf("完成任务 #%d\n", task_id);
    free(arg); // 释放在添加任务时分配的内存
    
    __atomic_fetch_add(&g_tasks_completed, 1, __ATOMIC_SEQ_CST);
}

// 打印线程池状态
static void print_pool_status(thread_pool_t pool) {
    thread_pool_stats_t stats;
    if (thread_pool_get_stats(pool, &stats) == 0) {
        printf("线程池状态: 线程数=%d, 空闲=%d, 队列大小=%d, 已完成任务=%d/%d\n",
               stats.thread_count, stats.idle_threads, stats.task_queue_size,
               g_tasks_completed, g_total_tasks);
    }
}

int main(void) {
    // 设置信号处理
    struct sigaction sig_action; // 更长的变量名，避免变量名太短警告
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = signal_handler;
    sigaction(SIGINT, &sig_action, NULL);

    // 初始化日志
    log_init(NULL, LOG_LEVEL_INFO); // 使用NULL作为日志文件路径（只输出到控制台），日志级别为INFO
    
    printf("线程池任务优先级示例\n");
    printf("按Ctrl+C退出\n\n");

    // 创建线程池，4个工作线程
    thread_pool_t pool = thread_pool_create(4);
    if (pool == NULL) {
        printf("创建线程池失败\n");
        return 1;
    }

    // 设置线程池限制
    thread_pool_set_limits(pool, 2, 8);

    // 添加不同优先级的任务
    int num_tasks = 20;
    g_total_tasks = num_tasks;
    
    printf("添加%d个任务，包括高、普通、低和后台优先级...\n", num_tasks);
    
    // 随机添加不同优先级的任务
    task_priority_t priorities[] = {
        TASK_PRIORITY_HIGH,
        TASK_PRIORITY_NORMAL,
        TASK_PRIORITY_LOW,
        TASK_PRIORITY_BACKGROUND
    };
    
    const char* priority_names[] = {
        "高优先级",
        "普通优先级",
        "低优先级",
        "后台优先级"
    };
    
    for (int i = 0; i < num_tasks; i++) {
        int *task_id = malloc(sizeof(int));
        if (task_id == NULL) {
            printf("内存分配失败\n");
            continue;
        }
        
        *task_id = i + 1;
        
        // 选择优先级 - 为了演示效果，确保有各种优先级的任务
        task_priority_t priority = TASK_PRIORITY_NORMAL; // 默认初始化为普通优先级，避免未初始化警告
        if (i < 5) {
            // 前5个任务使用随机优先级
            priority = priorities[get_random(4)];
        } else if (i >= 5 && i < 10) {
            // 接下来5个任务都是高优先级
            priority = TASK_PRIORITY_HIGH;
        } else if (i >= 10 && i < 15) {
            // 接下来5个任务都是普通优先级
            priority = TASK_PRIORITY_NORMAL;
        } else {
            // 最后5个任务都是低优先级或后台优先级
            priority = priorities[2 + get_random(2)]; // 只选择低优先级或后台优先级
        }
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "任务#%d-%s", *task_id, priority_names[priority]);
        
        if (thread_pool_add_task(pool, task_function, task_id, task_name, priority) != 0) {
            printf("添加任务#%d失败\n", *task_id);
            free(task_id);
            continue;
        }
        
        printf("已添加: %s\n", task_name);
        
        // 短暂延迟，使输出更易读
        usleep(50000); // 50ms
    }
    
    // 主循环 - 每秒打印状态，直到所有任务完成或用户请求退出
    int timeout_counter = 0;
    while (!g_shutdown_requested && g_tasks_completed < g_total_tasks) {
        print_pool_status(pool);
        
        // 检查是否所有任务都已完成
        if (g_tasks_completed >= g_total_tasks) {
            printf("所有任务已完成！\n");
            break;
        }
        
        // 超时检查 - 30秒后自动退出
        if (timeout_counter++ > 30) {
            printf("超时 - 30秒后自动退出\n");
            break;
        }
        
        sleep(1);
    }
    
    // 最终状态
    print_pool_status(pool);
    
    // 销毁线程池
    printf("销毁线程池...\n");
    thread_pool_destroy(pool);
    
    // 关闭日志
    log_deinit();
    
    printf("示例程序已完成\n");
    return 0;
}
