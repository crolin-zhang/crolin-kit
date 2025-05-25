/**
 * @file thread_internal.h
 * @brief 线程池库的内部头文件。
 *
 * 此文件包含线程池实现的内部结构和函数声明，
 * 不应被外部代码直接包含。
 */
#ifndef THREAD_INTERNAL_H
#define THREAD_INTERNAL_H

#include "../include/thread.h"
// log.h 已在 thread.h 中包含
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/**
 * @struct task_node_s
 * @brief 用于表示任务队列的链表节点结构。
 *
 * 每个节点包含一个任务和指向队列中下一个任务的指针。
 * 此结构是线程池实现的内部结构。
 */
typedef struct task_node_s {
    task_t task; /**< 实际的任务数据 (函数、参数、名称)。task_t 在 thread.h 中定义。 */
    struct task_node_s *next; /**< 指向队列中下一个任务节点的指针。 */
} task_node_t;                /**< 内部使用的类型定义。 */

/**
 * @struct thread_pool_s
 * @brief 线程池的内部表示。
 *
 * 此结构持有与线程池实例相关的所有状态，
 * 包括同步原语、线程管理信息、
 */
struct thread_pool_s {
    pthread_mutex_t lock;  /**< 用于保护任务队列和其他共享状态的互斥锁。 */
    pthread_mutex_t resize_lock; /**< 用于保护线程池大小调整操作的互斥锁。 */
    pthread_cond_t notify; /**< 条件变量，用于通知工作线程有新任务或池正在关闭。 */
    pthread_t *threads;    /**< 工作线程 ID 数组。 */
    struct task_node_s *head; /**< 任务队列头指针。 */
    struct task_node_s *tail; /**< 任务队列尾指针。 */
    int thread_count;     /**< 池中的线程数量。 */
    int min_threads;      /**< 池中允许的最小线程数量。 */
    int max_threads;      /**< 池中允许的最大线程数量。 */
    int idle_threads;     /**< 当前空闲的线程数量。 */
    int task_queue_size;  /**< 当前任务队列中的任务数量。 */
    int shutdown;         /**< 标志，指示池是否正在关闭 (1) 或活动 (0)。 */
    int resize_shutdown;  /**< 标志，指示是否有线程需要由于缩小而退出 (1) 或不需要 (0)。 */
    int started;  /**< 已成功启动的线程数量。 */
    char **running_task_names; /**< 字符串数组，每个字符串存储
                                     相应工作线程当前正在执行的任务的名称。
                                     每个字符串的最大长度为 MAX_TASK_NAME_LEN。 */
    int *thread_status; /**< 线程状态数组，0表示空闲，1表示忙碌，-1表示应该退出。 */
};

/**
 * @struct thread_args_t
 * @brief 用于向新创建的工作线程传递参数的结构。
 *
 * 此结构在内部用于为每个工作线程提供指向线程池
 * 及其唯一线程 ID 的指针。
 */
typedef struct {
    thread_pool_t pool; /**< 指向线程池实例的指针 (来自公共 API 的不透明类型)。 */
    int thread_id;      /**< 工作线程的唯一标识符 (索引)。 */
} thread_args_t;

// 内部函数声明
static void *worker_thread_function(void *arg);
static int task_enqueue_internal(thread_pool_t pool, task_t task);
static task_t *task_dequeue_internal(thread_pool_t pool);
static void task_queue_destroy_internal(thread_pool_t pool);

#endif /* THREAD_INTERNAL_H */
