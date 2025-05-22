// 定义 DEBUG_THREAD_POOL 以启用 TPOOL_LOG 和 TPOOL_ERROR 消息
#define DEBUG_THREAD_POOL

#include <stdio.h>      // 用于标准输入输出 (printf, fprintf)
#include <stdlib.h>     // 用于标准库函数 (malloc, free, exit)
#include <string.h>     // 用于字符串操作 (strncmp)
#include <unistd.h>     // 用于 POSIX 操作系统 API (sleep, usleep)
#include <pthread.h>    // 用于 POSIX 线程 (pthread_mutex_t)
#include "thread_pool.h" // 我们的线程池库的头文件

// 断言宏
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            /* 断言失败：打印文件、行号、条件和消息 */ \
            fprintf(stderr, "断言失败: (%s:%d) %s - %s\n", __FILE__, __LINE__, #condition, message); \
            exit(EXIT_FAILURE); /* 失败时退出程序 */ \
        } else { \
            /* 断言通过：打印消息 */ \
            printf("断言通过: %s\n", message); \
        } \
    } while (0)

// 测试用的全局变量
static int tasks_completed_count = 0; // 已完成任务的计数器
static pthread_mutex_t test_lock;    // 用于保护 tasks_completed_count 的互斥锁

// 测试任务函数

/**
 * @brief 简单的任务函数，用于测试。
 * 它增加全局任务完成计数器并释放传入的参数。
 * @param arg 指向动态分配的整数的指针。
 */
void simple_task_function(void *arg) {
    if (arg == NULL) {
        TPOOL_ERROR("简单任务: 收到 NULL 参数。");
        // 如果参数为 null，不增加计数，这可能表示测试设置错误
        return;
    }
    pthread_mutex_lock(&test_lock); // 锁定互斥锁以更新计数器
    tasks_completed_count++;
    TPOOL_LOG("简单任务 (参数: %d): 完成。总计完成: %d", *(int*)arg, tasks_completed_count);
    pthread_mutex_unlock(&test_lock); // 解锁互斥锁
    free(arg); // 释放动态分配的参数
}

/**
 * @brief 延迟的任务函数，用于测试。
 * 它模拟一些工作（睡眠），然后调用 simple_task_function。
 * @param arg 指向动态分配的整数的指针。
 */
void delayed_task_function(void *arg) {
    if (arg == NULL) {
        TPOOL_ERROR("延迟任务: 收到 NULL 参数。");
        // 如果参数为 null，不继续执行
        return;
    }
    TPOOL_LOG("延迟任务 (参数: %d): 开始，将睡眠 1 秒。", *(int*)arg);
    sleep(1); // 模拟工作
    TPOOL_LOG("延迟任务 (参数: %d): 睡眠完成，调用 simple_task_function。", *(int*)arg);
    simple_task_function(arg); // 这将释放 arg
}

// 测试用例

/**
 * @brief 测试线程池的基本创建和销毁。
 * 验证线程池能否成功创建并立即销毁。
 */
void test_pool_creation_destruction() {
    printf("\n--- 运行测试: 线程池创建和销毁 ---\n");
    thread_pool_t *pool = thread_pool_create(2); // 创建一个包含2个线程的池
    ASSERT(pool != NULL, "线程池创建成功。");
    
    // 立即销毁
    int destroy_result = thread_pool_destroy(pool);
    ASSERT(destroy_result == 0, "线程池销毁成功。");
    printf("测试线程池创建和销毁: 通过\n");
}

/**
 * @brief 测试单个任务的执行。
 * 验证线程池能否正确执行一个简单的任务。
 */
void test_single_task_execution() {
    printf("\n--- 运行测试: 单个任务执行 ---\n");
    pthread_mutex_lock(&test_lock);
    tasks_completed_count = 0; // 重置任务完成计数器
    pthread_mutex_unlock(&test_lock);

    thread_pool_t *pool = thread_pool_create(1); // 创建一个单线程池
    ASSERT(pool != NULL, "为单个任务测试创建线程池。");

    int *arg = (int *)malloc(sizeof(int)); // 为任务分配参数
    ASSERT(arg != NULL, "为单个任务分配参数。");
    *arg = 100;

    // 添加单个任务
    int add_result = thread_pool_add_task(pool, simple_task_function, arg, "单个简单任务");
    ASSERT(add_result == 0, "单个任务添加成功。");

    TPOOL_LOG("单个任务测试: 等待任务完成 (1 秒)...");
    sleep(1); // 等待任务完成的时间

    int destroy_result = thread_pool_destroy(pool); // 销毁线程池
    ASSERT(destroy_result == 0, "单个任务后销毁线程池。");
    
    pthread_mutex_lock(&test_lock);
    ASSERT(tasks_completed_count == 1, "一个任务已完成。"); // 断言任务已成功完成
    pthread_mutex_unlock(&test_lock);
    printf("测试单个任务执行: 通过\n");
}

/**
 * @brief 测试多个任务的执行。
 * 验证线程池能否正确处理和执行多个任务。
 */
void test_multiple_task_execution() {
    printf("\n--- 运行测试: 多个任务执行 ---\n");
    pthread_mutex_lock(&test_lock);
    tasks_completed_count = 0; // 重置任务完成计数器
    pthread_mutex_unlock(&test_lock);

    const int num_tasks_to_add = 5; // 要添加的任务数量
    const int num_threads = 2;      // 线程池中的线程数
    thread_pool_t *pool = thread_pool_create(num_threads);
    ASSERT(pool != NULL, "为多个任务测试创建线程池。");

    // 添加多个任务
    for (int i = 0; i < num_tasks_to_add; i++) {
        int *arg = (int *)malloc(sizeof(int));
        ASSERT(arg != NULL, "为多任务分配参数。");
        *arg = i;
        char task_name[MAX_TASK_NAME_LEN];
        snprintf(task_name, MAX_TASK_NAME_LEN, "多任务-%d", i);
        int add_result = thread_pool_add_task(pool, simple_task_function, arg, task_name);
        ASSERT(add_result == 0, "多个任务添加成功。");
        if (add_result != 0) { // ASSERT 会处理，但作为防御性编程
            free(arg); 
        }
    }

    TPOOL_LOG("多个任务测试: 等待任务完成 (3 秒)...");
    sleep(3); // 等待任务完成的时间

    int destroy_result = thread_pool_destroy(pool); // 销毁线程池
    ASSERT(destroy_result == 0, "多个任务后销毁线程池。");

    pthread_mutex_lock(&test_lock);
    ASSERT(tasks_completed_count == num_tasks_to_add, "所有多个任务已完成。"); // 断言所有任务均已完成
    pthread_mutex_unlock(&test_lock);
    printf("测试多个任务执行: 通过\n");
}

/**
 * @brief 测试任务命名和跟踪功能。
 * 验证 `thread_pool_get_running_task_names` 是否能正确报告正在运行的任务。
 */
void test_task_naming_and_tracking() {
    printf("\n--- 运行测试: 任务命名和跟踪 ---\n");
    pthread_mutex_lock(&test_lock);
    tasks_completed_count = 0; // 重置计数器，尽管不是此测试的主要焦点
    pthread_mutex_unlock(&test_lock);

    const int num_test_threads = 1; // 测试中使用的线程数
    thread_pool_t *pool = thread_pool_create(num_test_threads);
    ASSERT(pool != NULL, "为命名测试创建线程池。");

    int *arg = (int *)malloc(sizeof(int));
    ASSERT(arg != NULL, "为命名任务分配参数。");
    *arg = 200;
    const char* task_name_to_check = "我的延迟任务-01"; // 要检查的任务名称

    // 添加一个延迟任务，以便有时间检查其名称
    int add_result = thread_pool_add_task(pool, delayed_task_function, arg, task_name_to_check);
    ASSERT(add_result == 0, "已添加命名的延迟任务。");

    TPOOL_LOG("命名测试: 短暂睡眠 (200毫秒) 以允许任务被拾取...");
    usleep(200 * 1000); // 200 毫秒

    // 获取正在运行的任务名称
    char **running_tasks = thread_pool_get_running_task_names(pool);
    ASSERT(running_tasks != NULL, "成功检索到正在运行的任务名称数组。");
    if (running_tasks) { // 由于 ASSERT，应为 true
        ASSERT(running_tasks[0] != NULL, "线程 0 的任务名称字符串不为 NULL。");
        if (running_tasks[0]) { // 应为 true
             TPOOL_LOG("命名测试: 线程 0 报告的任务名称: '%s'", running_tasks[0]);
             ASSERT(strncmp(running_tasks[0], task_name_to_check, MAX_TASK_NAME_LEN) == 0, "线程 0 上运行的是正确的任务名称。");
        }
        free_running_task_names(running_tasks, num_test_threads); // 释放名称数组
        TPOOL_LOG("命名测试: 已释放正在运行的任务名称数组。");
    }
    
    TPOOL_LOG("命名测试: 等待延迟任务完成 (2 秒)...");
    sleep(2); // 允许延迟任务完全完成

    int destroy_result = thread_pool_destroy(pool); // 销毁线程池
    ASSERT(destroy_result == 0, "命名测试后销毁线程池。");

    pthread_mutex_lock(&test_lock);
    ASSERT(tasks_completed_count == 1, "命名任务已完成。"); // 检查它最终是否运行了
    pthread_mutex_unlock(&test_lock);
    printf("测试任务命名和跟踪: 通过\n");
}

/**
 * @brief 测试线程池的关闭行为。
 * 验证 `thread_pool_destroy` 是否会等待正在执行的任务完成。
 */
void test_shutdown_behavior() {
    printf("\n--- 运行测试: 关闭行为 ---\n");
    pthread_mutex_lock(&test_lock);
    tasks_completed_count = 0; // 重置任务完成计数器
    pthread_mutex_unlock(&test_lock);

    const int num_threads_shutdown_test = 2;    // 关闭测试中的线程数
    const int num_tasks_shutdown_test = 10;     // 要添加的任务数，所有都是延迟任务

    thread_pool_t *pool = thread_pool_create(num_threads_shutdown_test);
    ASSERT(pool != NULL, "为关闭测试创建线程池。");

    // 添加多个延迟任务
    for (int i = 0; i < num_tasks_shutdown_test; i++) {
        int *arg = (int *)malloc(sizeof(int));
        ASSERT(arg != NULL, "为关闭测试任务分配参数。");
        *arg = 300 + i;
        char task_name[MAX_TASK_NAME_LEN];
        snprintf(task_name, MAX_TASK_NAME_LEN, "关闭测试任务-%d", i);
        // delayed_task_function 睡眠 1 秒然后增加计数
        if (thread_pool_add_task(pool, delayed_task_function, arg, task_name) != 0) {
            TPOOL_ERROR("关闭测试: 添加任务 %s 失败，正在释放参数。", task_name);
            free(arg); // 如果添加失败则释放参数
        }
    }

    TPOOL_LOG("关闭测试: 已添加 %d 个延迟任务。睡眠 500毫秒...", num_tasks_shutdown_test);
    usleep(500 * 1000); // 500 毫秒，允许一些任务开始执行

    TPOOL_LOG("关闭测试: 调用 thread_pool_destroy()...");
    int destroy_result = thread_pool_destroy(pool); // 这应该等待正在进行的任务。
    ASSERT(destroy_result == 0, "thread_pool_destroy 成功完成。");
    TPOOL_LOG("关闭测试: thread_pool_destroy() 完成。");

    pthread_mutex_lock(&test_lock);
    // 期望：如果任务被拾取并且 `destroy` 等待了，至少 num_threads 个任务应该已完成。
    // 如果任务需要 1 秒，并且我们在 destroy 前等待了 0.5 秒，
    // num_threads 个任务应该正在运行。`destroy` 会等待这些任务。
    // 因此，tasks_completed_count 理想情况下应该是 num_threads。
    // 如果 destroy 太快或任务拾取缓慢，则可能较少。
    // 关键是 destroy 不会死锁并能完成。
    // 并且那些 *确定* 正在运行的任务已完成。
    // 鉴于 delayed_task 中有 1 秒的睡眠和 destroy 前有 0.5 秒的睡眠，
    // 很可能 'num_threads_shutdown_test' 个任务已被拾取并运行至完成。
    ASSERT(tasks_completed_count >= 0 && tasks_completed_count <= num_tasks_shutdown_test, "已完成任务数在预期范围内。");
    // 更精确的断言需要确切知道在调用 destroy 时有多少任务正在 *执行中*。
    // 对于此测试，我们主要确保 destroy 工作并等待。
    // 如果 tasks_completed_count 等于 num_threads_shutdown_test，这是一个好迹象。
    TPOOL_LOG("关闭测试: 已完成任务数: %d", tasks_completed_count);
    ASSERT(tasks_completed_count >= num_threads_shutdown_test, "如果任务被拾取，至少应有与线程数相同的任务完成。");
    // 这假设任务被相当快地拾取。如果不是，这可能过于严格。
    // 一个更健壮的测试可能是：
    // ASSERT(tasks_completed_count > 0, "一些任务在关闭前/期间完成。");
    // ASSERT(tasks_completed_count <= num_tasks_shutdown_test, "完成的任务数不超过添加的任务数。");
    pthread_mutex_unlock(&test_lock);

    printf("测试关闭行为: 通过 (Destroy 完成，完成了 %d 个任务)\n", tasks_completed_count);
}


// 主函数，运行测试
int main() {
    printf("开始线程池测试套件...\n");

    // 初始化测试互斥锁
    if (pthread_mutex_init(&test_lock, NULL) != 0) {
        fprintf(stderr, "未能初始化 test_lock 互斥锁。\n");
        return EXIT_FAILURE;
    }

    // 依次运行所有测试用例
    test_pool_creation_destruction();
    test_single_task_execution();
    test_multiple_task_execution();
    test_task_naming_and_tracking();
    test_shutdown_behavior();

    // 销毁测试互斥锁
    if (pthread_mutex_destroy(&test_lock) != 0) {
        fprintf(stderr, "未能销毁 test_lock 互斥锁。\n");
        return EXIT_FAILURE;
    }

    printf("\n所有测试成功通过！\n");
    
    // 编译提示
    printf("\n编译提醒:\n");
    printf("gcc -o test_pool test_thread_pool.c thread_pool.c -DDEBUG_THREAD_POOL -pthread\n");
    printf("或者使用 Wall 和 Werror:\n");
    printf("gcc -Wall -Werror -o test_pool test_thread_pool.c thread_pool.c -DDEBUG_THREAD_POOL -pthread\n");


    return EXIT_SUCCESS; // 程序成功退出
}
