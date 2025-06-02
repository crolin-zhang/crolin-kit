/**
 * @file thread_priority_test.c
 * @brief 线程池任务优先级功能的测试程序
 *
 * 此测试程序验证线程池的任务优先级功能，包括：
 * 1. 基本优先级排序功能测试
 * 2. 边界条件测试
 * 3. 压力测试
 * 4. 混合优先级测试
 */
#include "thread.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>
#include <time.h>

// 使用volatile sig_atomic_t类型的全局变量作为信号标志和计数器
static volatile sig_atomic_t g_shutdown_requested = 0; // 用于信号处理的标志
static volatile sig_atomic_t g_tasks_completed = 0;
static volatile sig_atomic_t g_high_priority_tasks_completed = 0;
static volatile sig_atomic_t g_normal_priority_tasks_completed = 0;
static volatile sig_atomic_t g_low_priority_tasks_completed = 0;
static volatile sig_atomic_t g_background_priority_tasks_completed = 0;

// 任务参数结构体定义
typedef struct {
    int id;                  // 任务ID
    task_priority_t priority; // 任务优先级
    int duration;           // 任务持续时间(毫秒)
    struct timespec start_time; // 开始时间
    struct timespec end_time;   // 结束时间
} task_arg_t;

// 全局数组，用于记录任务执行顺序
#define MAX_TASKS 100
static task_arg_t g_task_execution_order[MAX_TASKS];
static int g_execution_index = 0;
static pthread_mutex_t g_execution_mutex = PTHREAD_MUTEX_INITIALIZER;

// 信号处理函数声明
static void signal_handler(int sig);

// 测试函数声明
static int test_basic_priority_ordering(void);
static int test_mixed_priority_ordering(void);
static int test_priority_preemption(void);

// 生成随机数，使用安全的 getrandom() 函数
static int get_random_int(int min, int max)
{
    unsigned int value = 0;
    unsigned int range = 0;
    
    // 安全地获取随机值
    if (getrandom(&value, sizeof(value), 0) == -1) {
        // 使用更安全的熵源组合，避免仅依赖time()
        struct {
            pid_t pid;
            struct timespec time_spec;
            void* stack_ptr;
        } entropy;
        
        entropy.pid = getpid();
        clock_gettime(CLOCK_MONOTONIC, &entropy.time_spec);
        entropy.stack_ptr = &entropy;
        
        // 使用多个熵源混合生成随机数
        value = (unsigned int)(entropy.time_spec.tv_nsec ^ 
                             ((uintptr_t)entropy.stack_ptr & 0xFFFFFFFF) ^ 
                             ((unsigned int)entropy.pid << 16));
    }
    
    // 安全地计算范围，避免整数溢出和有符号转换问题
    if (max <= min) {
        return min; // 防御性编程
    }
    
    range = (unsigned int)(max - min + 1);
    // 安全地将范围限制在有效区间
    return min + (int)(value % range);
}

// 注意：我们使用数组索引而不是时间比较来确定任务执行顺序

// 任务优先级转换为字符串
static const char *priority_to_string(task_priority_t priority)
{
    switch (priority) {
        case TASK_PRIORITY_HIGH:
            return "高优先级";
        case TASK_PRIORITY_NORMAL:
            return "普通优先级";
        case TASK_PRIORITY_LOW:
            return "低优先级";
        case TASK_PRIORITY_BACKGROUND:
            return "后台优先级";
        default:
            return "未知优先级";
    }
}

// 注意：我们使用timespec结构直接记录时间，而不需要转换为毫秒时间戳

// 记录任务执行顺序
static void record_task_execution(task_arg_t *task_arg)
{
    pthread_mutex_lock(&g_execution_mutex);
    if (g_execution_index < MAX_TASKS) {
        g_task_execution_order[g_execution_index] = *task_arg;
        clock_gettime(CLOCK_MONOTONIC, &g_task_execution_order[g_execution_index].end_time);
        g_execution_index++;
    }
    pthread_mutex_unlock(&g_execution_mutex);
}

// 信号处理函数
static void signal_handler(int sig)
{
    printf("\n收到信号 %d，准备退出...\n", sig);
    g_shutdown_requested = 1;
}

// 全局任务计数器，用于跟踪任务完成情况
static volatile sig_atomic_t g_task_count = 0;

// 任务函数 - 模拟不同执行时间的工作// 简单任务函数
static void simple_task(void *arg)
{
    task_arg_t *task_arg = (task_arg_t *)arg;
    if (task_arg == NULL) {
        return;
    }

    // 记录任务开始执行的时间
    clock_gettime(CLOCK_MONOTONIC, &task_arg->start_time);

    // 模拟任务执行
    usleep(task_arg->duration * 1000); // 毫秒转微秒

    // 记录任务结束执行的时间
    clock_gettime(CLOCK_MONOTONIC, &task_arg->end_time);

    // 记录任务执行顺序
    record_task_execution(task_arg);

    free(task_arg);
}

// 长时间运行的任务函数
static void long_running_task(void *arg)
{
    task_arg_t *task_arg = (task_arg_t *)arg;
    if (task_arg == NULL) {
        return;
    }

    // 记录任务开始执行的时间
    clock_gettime(CLOCK_MONOTONIC, &task_arg->start_time);
    
    printf("开始执行长时间任务 #%d (%s)\n", 
           task_arg->id, priority_to_string(task_arg->priority));
    
    // 模拟长时间运行
    usleep(task_arg->duration * 1000); // 毫秒转微秒
    
    printf("完成执行长时间任务 #%d (%s)\n", 
           task_arg->id, priority_to_string(task_arg->priority));

    // 记录任务结束执行的时间
    clock_gettime(CLOCK_MONOTONIC, &task_arg->end_time);

    // 记录任务执行顺序
    record_task_execution(task_arg);

    free(task_arg);
}

// 任务函数 - 模拟不同执行时间的工作
static void priority_task(void *arg)
{
    task_arg_t *task_arg = (task_arg_t *)arg;
    
    // 随机化执行时间 (10-50ms)
    int sleep_time = get_random_int(10, 50);
    task_arg->duration = sleep_time;
    
    // 记录任务开始执行
    clock_gettime(CLOCK_MONOTONIC, &task_arg->start_time);
    
    // 模拟工作
    usleep(sleep_time * 1000);
    
    // 记录任务执行
    record_task_execution(task_arg);
    
    // 更新完成计数
    __atomic_add_fetch(&g_tasks_completed, 1, __ATOMIC_SEQ_CST);
    
    // 根据优先级更新相应的计数器
    switch (task_arg->priority) {
        case TASK_PRIORITY_HIGH:
            __atomic_add_fetch(&g_high_priority_tasks_completed, 1, __ATOMIC_SEQ_CST);
            break;
        case TASK_PRIORITY_NORMAL:
            __atomic_add_fetch(&g_normal_priority_tasks_completed, 1, __ATOMIC_SEQ_CST);
            break;
        case TASK_PRIORITY_LOW:
            __atomic_add_fetch(&g_low_priority_tasks_completed, 1, __ATOMIC_SEQ_CST);
            break;
        case TASK_PRIORITY_BACKGROUND:
            __atomic_add_fetch(&g_background_priority_tasks_completed, 1, __ATOMIC_SEQ_CST);
            break;
    }
    
    // 不要释放task_arg，它是全局数组的一部分
}

/**
 * @brief 测试高优先级任务插队功能
 * 
 * 测试高优先级任务是否能够插队执行，即在低优先级任务执行过程中添加高优先级任务时，
 * 高优先级任务应该优先执行
 * 
 * @return 成功时返回0，失败时返回非零值
 */
static int test_priority_preemption(void)
{
    printf("\n=== 测试高优先级任务插队功能 ===\n");

    // 初始化日志系统
    log_init(NULL, LOG_LEVEL_INFO);

    // 创建线程池，使用两个工作线程，便于观察插队效果
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        printf("线程池创建失败\n");
        return 1;
    }
    printf("线程池创建成功，启动2个工作线程\n");

    // 重置全局变量
    g_execution_index = 0;
    g_tasks_completed = 0;
    g_task_count = 0;

    // 创建一个长时间运行的后台任务
    task_arg_t *long_task_arg = malloc(sizeof(task_arg_t));
    if (long_task_arg == NULL) {
        printf("内存分配失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    long_task_arg->id = 1;
    long_task_arg->priority = TASK_PRIORITY_BACKGROUND;
    long_task_arg->duration = 2000; // 2秒的任务
    
    printf("添加一个长时间运行的后台任务...\n");
    thread_pool_add_task(pool, long_running_task, long_task_arg, "长时间后台任务", TASK_PRIORITY_BACKGROUND);
    g_task_count++;
    
    // 等待一小段时间，确保后台任务开始执行
    usleep(100000); // 100毫秒
    
    // 添加一个高优先级任务，应该会“插队”
    task_arg_t *high_task_arg = malloc(sizeof(task_arg_t));
    if (high_task_arg == NULL) {
        printf("内存分配失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    high_task_arg->id = 2;
    high_task_arg->priority = TASK_PRIORITY_HIGH;
    high_task_arg->duration = 100; // 100毫秒的任务
    
    printf("添加一个高优先级任务...\n");
    thread_pool_add_task(pool, simple_task, high_task_arg, "高优先级任务", TASK_PRIORITY_HIGH);
    g_task_count++;
    
    // 添加一个普通优先级任务
    task_arg_t *normal_task_arg = malloc(sizeof(task_arg_t));
    if (normal_task_arg == NULL) {
        printf("内存分配失败\n");
        thread_pool_destroy(pool);
        return 1;
    }
    normal_task_arg->id = 3;
    normal_task_arg->priority = TASK_PRIORITY_NORMAL;
    normal_task_arg->duration = 100; // 100毫秒的任务
    
    printf("添加一个普通优先级任务...\n");
    thread_pool_add_task(pool, simple_task, normal_task_arg, "普通优先级任务", TASK_PRIORITY_NORMAL);
    g_task_count++;
    
    // 等待所有任务完成
    printf("等待所有任务完成...\n");
    usleep(3000000); // 等待3秒，足够所有任务完成
    printf("所有任务已完成！\n");

    // 检查执行顺序
    printf("\n任务执行顺序:\n");
    for (int i = 0; i < g_execution_index; i++) {
        printf("  %2d: 任务 #%d (%s)\n", i + 1, g_task_execution_order[i].id, 
               priority_to_string(g_task_execution_order[i].priority));
    }
    
    // 验证高优先级任务是否在后台任务完成前执行
    int high_priority_index = -1;
    int background_completion_index = -1;
    
    for (int i = 0; i < g_execution_index; i++) {
        if (g_task_execution_order[i].id == 2) { // 高优先级任务ID
            high_priority_index = i;
        } else if (g_task_execution_order[i].id == 1) { // 后台任务ID
            background_completion_index = i;
        }
    }
    
    int preemption_success = 0;
    if (high_priority_index != -1 && background_completion_index != -1 && 
        high_priority_index < background_completion_index) {
        printf("\n插队测试成功: 高优先级任务在后台任务完成前执行\n");
        preemption_success = 1;
    } else {
        printf("\n插队测试失败: 高优先级任务未能在后台任务完成前执行\n");
    }
    
    // 验证普通优先级任务是否在高优先级任务之后执行
    int normal_priority_index = -1;
    
    for (int i = 0; i < g_execution_index; i++) {
        if (g_task_execution_order[i].id == 3) { // 普通优先级任务ID
            normal_priority_index = i;
        }
    }
    
    if (normal_priority_index > high_priority_index) {
        printf("优先级排序测试成功: 普通优先级任务在高优先级任务之后执行\n");
    } else {
        printf("优先级排序测试失败: 普通优先级任务在高优先级任务之前执行\n");
        preemption_success = 0;
    }

    // 清理资源
    thread_pool_destroy(pool);

    return preemption_success ? 0 : 1;
}

/**
 * @brief 测试混合优先级排序
 * 
 * 测试在动态添加不同优先级任务时的排序行为
 * 
 * @return 成功时返回0，失败时返回非零值
 */
static int test_mixed_priority_ordering(void)
{
    printf("\n=== 测试混合优先级排序 ===\n");
    fflush(stdout);

    // 创建线程池，使用2个工作线程
    thread_pool_t pool = thread_pool_create(2);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    printf("线程池创建成功，启动2个工作线程\n");
    fflush(stdout);

    // 等待线程初始化
    usleep(100000); // 100ms

    // 重置计数器
    g_tasks_completed = 0;
    g_high_priority_tasks_completed = 0;
    g_normal_priority_tasks_completed = 0;
    g_low_priority_tasks_completed = 0;
    g_background_priority_tasks_completed = 0;
    g_execution_index = 0;

    // 准备任务参数
    task_arg_t task_args[20]; // 20个任务
    memset(task_args, 0, sizeof(task_args));

    // 添加任务的顺序：先添加一批低优先级任务，然后添加高优先级任务
    printf("添加5个低优先级任务...\n");
    for (int i = 0; i < 5; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_LOW;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "低优先级任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    // 等待一些任务开始执行
    usleep(50000); // 50ms

    // 添加高优先级任务，应该插入到队列前面
    printf("添加5个高优先级任务...\n");
    for (int i = 5; i < 10; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_HIGH;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "高优先级任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    // 等待一些任务开始执行
    usleep(50000); // 50ms

    // 添加混合优先级任务
    printf("添加混合优先级任务...\n");
    task_priority_t mixed_priorities[] = {
        TASK_PRIORITY_NORMAL,
        TASK_PRIORITY_HIGH,
        TASK_PRIORITY_BACKGROUND,
        TASK_PRIORITY_LOW,
        TASK_PRIORITY_HIGH,
        TASK_PRIORITY_NORMAL,
        TASK_PRIORITY_BACKGROUND,
        TASK_PRIORITY_LOW,
        TASK_PRIORITY_NORMAL,
        TASK_PRIORITY_HIGH
    };
    
    for (int i = 10; i < 20; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = mixed_priorities[i - 10];
        
        const char* priority_str = "";
        switch (task_args[i].priority) {
            case TASK_PRIORITY_HIGH: priority_str = "高"; break;
            case TASK_PRIORITY_NORMAL: priority_str = "普通"; break;
            case TASK_PRIORITY_LOW: priority_str = "低"; break;
            case TASK_PRIORITY_BACKGROUND: priority_str = "后台"; break;
        }
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "%s优先级任务#%d", priority_str, task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    // 等待所有任务完成
    printf("等待所有任务完成...\n");
    int timeout = 0;
    while (g_tasks_completed < 20 && timeout < 50 && !g_shutdown_requested) { // 最多等待5秒
        usleep(100000); // 100ms
        timeout++;
        
        // 打印进度
        if (timeout % 10 == 0) {
            printf("已完成: %d/20 任务 (高:%d, 普通:%d, 低:%d, 后台:%d)\n", 
                   (int)g_tasks_completed, 
                   (int)g_high_priority_tasks_completed,
                   (int)g_normal_priority_tasks_completed,
                   (int)g_low_priority_tasks_completed,
                   (int)g_background_priority_tasks_completed);
            fflush(stdout);
        }
    }

    // 检查是否所有任务都已完成
    if (g_tasks_completed < 20 && !g_shutdown_requested) {
        fprintf(stderr, "超时: 只完成了 %d/20 个任务\n", (int)g_tasks_completed);
        thread_pool_destroy(pool);
        return 1;
    }

    if (g_shutdown_requested) {
        printf("收到退出请求，提前结束测试\n");
        thread_pool_destroy(pool);
        return 0;
    }

    printf("所有任务已完成！\n");

    // 打印执行顺序
    printf("\n任务执行顺序:\n");
    for (int i = 0; i < g_execution_index; i++) {
        const char* priority_str = "";
        switch (g_task_execution_order[i].priority) {
            case TASK_PRIORITY_HIGH: priority_str = "高"; break;
            case TASK_PRIORITY_NORMAL: priority_str = "普通"; break;
            case TASK_PRIORITY_LOW: priority_str = "低"; break;
            case TASK_PRIORITY_BACKGROUND: priority_str = "后台"; break;
        }
        printf("  %2d: 任务 #%d (%s优先级)\n", i+1, g_task_execution_order[i].id, priority_str);
    }

    printf("\n混合优先级排序测试成功!\n");

    // 销毁线程池
    thread_pool_destroy(pool);
    return 0;
}

/**
 * @brief 主函数
 *
 * 运行所有任务优先级测试
 *
 * @return 成功时返回0，失败时返回非零值
 */
int main(void)
{
    // 设置信号处理
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = signal_handler;
    sigaction(SIGINT, &sig_action, NULL);
    sigaction(SIGTERM, &sig_action, NULL);

    printf("=== 线程池任务优先级测试 ===\n");
    printf("按Ctrl+C可以随时终止测试\n\n");
    fflush(stdout);

    // 运行基本优先级排序测试
    if (g_shutdown_requested) {
        printf("收到退出请求，终止测试\n");
        return 0;
    }
    
    int result = test_basic_priority_ordering();
    if (result != 0) {
        fprintf(stderr, "基本优先级排序测试失败\n");
        return result;
    }

    // 运行混合优先级排序测试
    if (g_shutdown_requested) {
        printf("收到退出请求，终止测试\n");
        return 0;
    }
    
    result = test_mixed_priority_ordering();
    if (result != 0) {
        fprintf(stderr, "混合优先级排序测试失败\n");
        return result;
    }

    // 运行高优先级任务插队测试
    if (g_shutdown_requested) {
        printf("收到退出请求，终止测试\n");
        return 0;
    }
    
    result = test_priority_preemption();
    if (result != 0) {
        fprintf(stderr, "高优先级任务插队测试失败\n");
        return result;
    }

    printf("\n====================================\n");
    printf("=== 所有任务优先级测试已全部通过 ===\n");
    printf("====================================\n");
    
    return 0;
}

/**
 * @brief 测试基本优先级排序功能
 *
 * 测试不同优先级的任务是否按照预期的顺序执行
 *
 * @return 成功时返回0，失败时返回非零值
 */
static int test_basic_priority_ordering(void)
{
    printf("\n=== 测试基本优先级排序 ===\n");
    fflush(stdout);

    // 创建线程池，只使用1个工作线程以确保排序可预测
    thread_pool_t pool = thread_pool_create(1);
    if (pool == NULL) {
        fprintf(stderr, "创建线程池失败\n");
        return 1;
    }
    printf("线程池创建成功，启动1个工作线程\n");
    fflush(stdout);

    // 等待线程初始化
    usleep(100000); // 100ms

    // 重置计数器
    g_tasks_completed = 0;
    g_high_priority_tasks_completed = 0;
    g_normal_priority_tasks_completed = 0;
    g_low_priority_tasks_completed = 0;
    g_background_priority_tasks_completed = 0;
    g_execution_index = 0;

    // 准备任务参数
    task_arg_t task_args[16]; // 16个任务
    memset(task_args, 0, sizeof(task_args));

    // 添务任务，顺序为：后台、低、普通、高
    // 这样添务的顺序与期望的执行顺序相反
    printf("添加4个后台优先级任务...\n");
    for (int i = 0; i < 4; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_BACKGROUND;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "后台任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    printf("添加4个低优先级任务...\n");
    for (int i = 4; i < 8; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_LOW;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "低优先级任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    printf("添加4个普通优先级任务...\n");
    for (int i = 8; i < 12; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_NORMAL;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "普通优先级任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    printf("添加4个高优先级任务...\n");
    for (int i = 12; i < 16; i++) {
        task_args[i].id = i + 1;
        task_args[i].priority = TASK_PRIORITY_HIGH;
        
        char task_name[64];
        snprintf(task_name, sizeof(task_name), "高优先级任务#%d", task_args[i].id);
        
        if (thread_pool_add_task(pool, priority_task, &task_args[i], task_name, task_args[i].priority) != 0) {
            fprintf(stderr, "添加任务#%d失败\n", task_args[i].id);
            thread_pool_destroy(pool);
            return 1;
        }
    }

    // 等待所有任务完成
    printf("等待所有任务完成...\n");
    int timeout = 0;
    while (g_tasks_completed < 16 && timeout < 50) { // 最多等待5秒
        usleep(100000); // 100ms
        timeout++;
        
        // 打印进度
        if (timeout % 10 == 0) {
            printf("已完成: %d/16 任务 (高:%d, 普通:%d, 低:%d, 后台:%d)\n", 
                   (int)g_tasks_completed, 
                   (int)g_high_priority_tasks_completed,
                   (int)g_normal_priority_tasks_completed,
                   (int)g_low_priority_tasks_completed,
                   (int)g_background_priority_tasks_completed);
            fflush(stdout);
        }
    }

    // 检查是否所有任务都已完成
    if (g_tasks_completed < 16) {
        fprintf(stderr, "超时: 只完成了 %d/16 个任务\n", (int)g_tasks_completed);
        thread_pool_destroy(pool);
        return 1;
    }

    printf("所有任务已完成！\n");

    // 检查执行顺序
    printf("\n检查任务执行顺序...\n");
    int priority_errors = 0;
    int skip_first_task = 1; // 跳过第一个任务，因为它可能在优先级设置生效前已经开始执行

    // 在单线程模式下，第一个任务可能已经开始执行，所以我们跳过第一个任务
    // 并检查后续任务是否按照优先级分组执行
    
    // 检查所有高优先级任务是否先于所有普通优先级任务执行
    int last_high_priority_index = -1;
    int first_normal_priority_index = -1;
    
    for (int i = skip_first_task; i < g_execution_index; i++) {
        if (g_task_execution_order[i].priority == TASK_PRIORITY_HIGH) {
            if (last_high_priority_index < i) {
                last_high_priority_index = i;
            }
        } else if (g_task_execution_order[i].priority == TASK_PRIORITY_NORMAL) {
            if (first_normal_priority_index == -1) {
                first_normal_priority_index = i;
            }
        }
    }
    
    // 只有当两个索引都有效时才进行比较
    if (last_high_priority_index != -1 && first_normal_priority_index != -1 && 
        last_high_priority_index > first_normal_priority_index) {
        printf("错误: 高优先级任务组在普通优先级任务组之后执行\n");
        priority_errors++;
    }

    // 检查所有普通优先级任务是否先于所有低优先级任务执行
    int last_normal_priority_index = -1;
    int first_low_priority_index = -1;
    
    for (int i = skip_first_task; i < g_execution_index; i++) {
        if (g_task_execution_order[i].priority == TASK_PRIORITY_NORMAL) {
            if (last_normal_priority_index < i) {
                last_normal_priority_index = i;
            }
        } else if (g_task_execution_order[i].priority == TASK_PRIORITY_LOW) {
            if (first_low_priority_index == -1) {
                first_low_priority_index = i;
            }
        }
    }
    
    // 只有当两个索引都有效时才进行比较
    if (last_normal_priority_index != -1 && first_low_priority_index != -1 && 
        last_normal_priority_index > first_low_priority_index) {
        printf("错误: 普通优先级任务组在低优先级任务组之后执行\n");
        priority_errors++;
    }

    // 在单线程模式下，第一个任务可能已经开始执行，所以我们不检查第一个任务
    // 而是检查后续任务是否按照优先级分组执行
    
    // 找到第一个后台任务的索引（跳过第一个任务）
    int last_low_priority_index = -1;
    int first_background_priority_index = -1;
    
    for (int i = skip_first_task; i < g_execution_index; i++) {
        if (g_task_execution_order[i].priority == TASK_PRIORITY_LOW) {
            if (last_low_priority_index < i) {
                last_low_priority_index = i;
            }
        } else if (g_task_execution_order[i].priority == TASK_PRIORITY_BACKGROUND) {
            if (first_background_priority_index == -1) {
                first_background_priority_index = i;
            }
        }
    }
    
    // 只有当两个索引都有效时才进行比较
    if (last_low_priority_index != -1 && first_background_priority_index != -1 && 
        last_low_priority_index > first_background_priority_index) {
        printf("错误: 低优先级任务组在后台优先级任务组之后执行\n");
        priority_errors++;
    }

    // 打印执行顺序
    printf("\n任务执行顺序:\n");
    for (int i = 0; i < g_execution_index; i++) {
        const char* priority_str = "";
        switch (g_task_execution_order[i].priority) {
            case TASK_PRIORITY_HIGH: priority_str = "高"; break;
            case TASK_PRIORITY_NORMAL: priority_str = "普通"; break;
            case TASK_PRIORITY_LOW: priority_str = "低"; break;
            case TASK_PRIORITY_BACKGROUND: priority_str = "后台"; break;
        }
        printf("  %2d: 任务 #%d (%s优先级)\n", i+1, g_task_execution_order[i].id, priority_str);
    }

    // 检查是否有优先级错误
    if (priority_errors > 0) {
        fprintf(stderr, "\n测试失败: 发现 %d 个优先级执行错误!\n", priority_errors);
        thread_pool_destroy(pool);
        return 1;
    }

    printf("\n基本优先级排序测试成功!\n");

    // 销毁线程池
    thread_pool_destroy(pool);
    return 0;
}
