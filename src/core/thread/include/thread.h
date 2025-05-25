/**
 * @file thread.h
 * @brief C 线程池库的公共头文件。
 *
 * 定义了用于创建、管理和使用线程池的公共 API。
 * 这包括任务定义、线程池管理功能以及用于监控和调试的实用程序。
 */
#ifndef THREAD_H
#define THREAD_H

#include <pthread.h> // 用于 pthread_t，尽管现在是不透明结构的一部分。
// 保留它是为了通用完整性，尽管如果所有 pthread 类型都隐藏在不透明 API 后面，
// 则并非严格需要。

/**
 * @brief 任务名称的最大长度，包括空终止符。
 *
 * 此宏定义用于确定 `task_t` 结构中 `task_name` 缓冲区的大小，
 * 以及库中其他地方存储任务名称的缓冲区的大小。
 */
#define MAX_TASK_NAME_LEN 64

// 日志宏 (使用日志模块)
#include "log.h" // 包含日志模块的头文件

/**
 * @def TPOOL_LOG
 * @brief 用于一般日志消息的宏。
 *
 * 使用日志模块的 LOG_INFO 函数记录日志。
 */
#define TPOOL_LOG(fmt, ...) LOG_INFO(LOG_MODULE_THREAD, fmt, ##__VA_ARGS__)

/**
 * @def TPOOL_ERROR
 * @brief 用于错误日志消息的宏。
 *
 * 使用日志模块的 LOG_ERROR 函数记录错误日志。
 */
#define TPOOL_ERROR(fmt, ...)                                                                      \
    LOG_ERROR(LOG_MODULE_THREAD, "(%s:%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

/**
 * @struct task_t
 * @brief 表示将由线程池执行的任务。
 *
 * 此结构传递给 `thread_pool_add_task` 以定义要执行的工作。
 */
typedef struct {
    void (*function)(void *arg); /**< 指向要执行的函数的指针。 */
    void *arg;                   /**< 要传递给函数的参数。 */
    char task_name[MAX_TASK_NAME_LEN]; /**< 任务的名称，用于日志记录/监控。以空字符结尾。 */
} task_t;

/**
 * @typedef thread_pool_t
 * @brief 线程池实例的不透明句柄。
 *
 * 此类型用于与线程池交互。实际的结构定义是库内部的。
 */
typedef struct thread_pool_s *thread_pool_t;

// 公共函数声明

/**
 * @brief 创建一个新的线程池。
 *
 * 使用指定数量的工作线程初始化线程池。
 *
 * @param num_threads 要在池中创建的工作线程数。必须为正数。
 * @return 成功时返回指向新创建的 thread_pool_t 实例的指针，
 *         错误时返回 NULL (例如，内存分配失败，无效参数)。
 */
thread_pool_t thread_pool_create(int num_threads);

/**
 * @brief向线程池的队列中添加一个新任务。
 *
 * 该任务将被一个可用的工作线程拾取以执行。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param function 指向定义任务的函数的指针。不能为空。
 * @param arg 要传递给任务函数的参数。如果函数期望，可以为 NULL。
 * @param task_name 任务的描述性名称。如果为 NULL，将使用 "unnamed_task"。
 *                  该名称被复制到任务结构中。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，function 为 NULL，
 *         池正在关闭，任务节点的内存分配失败)。
 */
int thread_pool_add_task(thread_pool_t pool, void (*function)(void *), void *arg,
                         const char *task_name);

/**
 * @brief 销毁线程池。
 *
 * 通知所有工作线程关闭。如果当前有正在执行的任务，
 * 此函数将等待它们完成。队列中剩余的任务将被丢弃。
 * 所有相关资源都将被释放。
 *
 * @param pool 指向要销毁的 thread_pool_t 实例的指针。
 * @return 成功时返回 0，如果池指针为 NULL 则返回 -1。如果池
 *         已在关闭或已销毁，则可能返回 0 作为无操作。
 */
int thread_pool_destroy(thread_pool_t pool);

/**
 * @brief 检索由工作线程当前执行的任务名称的副本。
 *
 * 调用者负责使用 `free_running_task_names()` 释放返回的数组及其中的每个字符串。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @return 一个动态分配的字符串数组 (char **)。此数组的大小
 *         等于池中的线程数。每个字符串是
 *         相应线程正在执行的任务名称的副本，或者如果线程
 *         当前未执行任务，则为 "[idle]"。
 *         错误时返回 NULL (例如，pool 为 NULL，内存分配失败)。
 */
char **thread_pool_get_running_task_names(thread_pool_t pool);

/**
 * @brief 释放由 `thread_pool_get_running_task_names` 返回的任务名称数组。
 *
 * @param task_names 要释放的字符串数组 (char **).
 * @param count 数组中的字符串数量 (应与调用 `thread_pool_get_running_task_names` 时
 *              池的 thread_count 相匹配)。
 */
void free_running_task_names(char **task_names, int count);

/**
 * @struct thread_pool_stats_t
 * @brief 线程池的统计信息。
 *
 * 此结构提供线程池的当前状态信息，包括线程数量、任务队列长度等。
 */
typedef struct {
    int thread_count;    /**< 当前线程数量 */
    int min_threads;     /**< 最小线程数量 */
    int max_threads;     /**< 最大线程数量 */
    int idle_threads;    /**< 空闲线程数量 */
    int task_queue_size; /**< 当前任务队列长度 */
    int started;         /**< 已启动的线程数量 */
} thread_pool_stats_t;

/**
 * @brief 调整线程池大小。
 *
 * 根据指定的新线程数量调整线程池大小。如果新线程数量大于当前数量，
 * 将创建新的线程。如果新线程数量小于当前数量，将优雅地减少线程数量。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param new_thread_count 新的线程数量。必须大于等于 min_threads 且小于等于 max_threads。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，新线程数量超出范围，
 *         池正在关闭，线程创建失败)。
 */
int thread_pool_resize(thread_pool_t pool, int new_thread_count);

/**
 * @brief 获取线程池的统计信息。
 *
 * 返回线程池的当前状态信息，包括线程数量、任务队列长度等。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param stats 指向 thread_pool_stats_t 结构的指针，用于存储统计信息。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，stats 为 NULL)。
 */
int thread_pool_get_stats(thread_pool_t pool, thread_pool_stats_t *stats);

/**
 * @brief 设置线程池的最小和最大线程数量。
 *
 * 设置线程池允许的线程数量范围。当前线程数量将保持不变，
 * 除非当前数量超出新的范围。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param min_threads 最小线程数量，必须大于 0。
 * @param max_threads 最大线程数量，必须大于等于 min_threads。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，无效的线程数量范围)。
 */
int thread_pool_set_limits(thread_pool_t pool, int min_threads, int max_threads);

#endif /* THREAD_H */
