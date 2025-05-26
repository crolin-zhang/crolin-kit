/**
 * @file thread.c
 * @brief 线程池库的实现。
 *
 * 此文件包含线程池的内部定义和函数实现，
 * 包括任务队列管理、工作线程逻辑以及公共 API 函数。
 */
#include "thread_internal.h" // 包含内部结构和函数声明
#include <pthread.h>         // 用于所有 pthread 操作
#include <stdio.h>           // 用于 TPOOL_LOG/TPOOL_ERROR 和 perror
#include <stdlib.h>          // 用于 malloc, free, calloc
#include <string.h>          // 用于 strncpy
#include "log.h"

// --- 任务队列管理函数 (内部) ---

/**
 * @brief 向队列尾部添加任务 (内部函数)。
 *
 * 此函数假定调用者 (例如, `thread_pool_add_task`)
 * 持有池的锁。它分配一个新的任务节点并将其附加到队列。
 *
 * @param pool 指向 thread_pool_s 实例的指针。
 * @param task 要入队的 task_t 数据。
 * @return 成功时返回 0，新任务节点内存分配错误时返回 -1。
 */
static int task_enqueue_internal(thread_pool_t pool, task_t task)
{
    task_node_t *new_node = (task_node_t *)malloc(sizeof(task_node_t));
    if (new_node == NULL) {
        TPOOL_ERROR("task_enqueue_internal: 未能为新任务节点分配内存");
        return -1;
    }
    new_node->task = task; // 复制任务数据
    new_node->next = NULL;

    if (pool->tail == NULL) { // 队列为空
        pool->head = new_node;
        pool->tail = new_node;
    } else {
        pool->tail->next = new_node;
        pool->tail = new_node;
    }
    pool->task_queue_size++;
    TPOOL_LOG("任务 '%s' 已内部入队。线程池: %p, 队列大小: %d", task.task_name, (void *)pool,
              pool->task_queue_size);
    return 0;
}

/**
 * @brief 从队列头部移除任务 (内部函数)。
 *
 * 此函数假定调用者 (通常是工作线程) 持有池的锁，
 * 并且已经检查过队列不为空，以及池在队列为空时没有正在关闭。
 * 它为新的 `task_t` 结构分配内存，将出队任务的数据复制到其中，
 * 释放 `task_node_t`，并返回指向新分配的 `task_t` 的指针。
 * 调用者负责在执行后释放返回的 `task_t`。
 *
 * @param pool 指向 thread_pool_s 实例的指针。
 * @return 指向出队的 `task_t` (在堆上分配) 的指针，如果队列为空 (调用者应预先检查)
 *         或为 `task_t` 副本分配内存失败，则返回 NULL。
 */
static task_t *task_dequeue_internal(thread_pool_t pool)
{
    if (pool->head == NULL) { // 防御性检查，尽管调用者应确保队列不为空。
        TPOOL_LOG("task_dequeue_internal: 尝试从线程池 %p 的空队列中出队。", (void *)pool);
        return NULL;
    }

    task_node_t *node_to_dequeue = pool->head;
    // 分配内存以保存任务数据的副本。此副本将由
    // 工作线程处理并在执行后由其释放。
    task_t *dequeued_task_data = (task_t *)malloc(sizeof(task_t));

    if (dequeued_task_data == NULL) {
        TPOOL_ERROR("task_dequeue_internal: 未能为线程池 %p 的出队任务数据分配内存", (void *)pool);
        // 在这种特定的错误情况下，节点仍保留在队列中。
        // 工作线程将循环并可能重试或根据关闭状态退出。
        return NULL;
    }

    *dequeued_task_data = node_to_dequeue->task; // 复制任务数据

    pool->head = node_to_dequeue->next;
    if (pool->head == NULL) {
        pool->tail = NULL; // 队列变为空
    }
    pool->task_queue_size--;
    TPOOL_LOG("任务 '%s' 已从线程池 %p 内部出队。队列大小: %d", dequeued_task_data->task_name,
              (void *)pool, pool->task_queue_size);

    free(node_to_dequeue);     // 释放队列节点本身
    return dequeued_task_data; // 返回堆分配的任务数据
}

/**
 * @brief 释放队列中所有剩余的任务节点 (内部函数)。
 *
 * 此函数通常在线程池销毁期间，在所有线程都已连接后调用。
 * 它遍历队列并释放所有 `task_node_t` 结构。
 * 注意：此函数 *不* 释放可能由 `task_dequeue_internal` 分配的 `task_t` 数据，
 * 因为这是工作线程的责任，或者如果任务从未执行，则由特定的清理逻辑负责。
 * 在这里，它仅释放队列容器节点。
 * 假定持有池锁或没有其他线程正在访问队列。
 *
 * @param pool 指向 thread_pool_s 实例的指针。
 */
static void task_queue_destroy_internal(thread_pool_t pool)
{
    task_node_t *current = pool->head;
    task_node_t *next_node = NULL;
    int count = 0;
    while (current != NULL) {
        next_node = current->next;
        // current->task 中的 task_t 在此不被释放，因为：
        // 1. 如果它是通过 thread_pool_add_task 添加的，其 'arg' 可能由外部或任务函数管理。
        // 2. 此函数用于在关闭期间清理队列结构本身。
        //    由工作线程获取的任何任务，其 task_t (来自 task_dequeue_internal) 将由工作线程释放。
        //    队列中剩余的任务只是被丢弃；它们的内部数据不被处理。
        free(current); // 释放节点结构
        current = next_node;
        count++;
    }
    pool->head = NULL;
    pool->tail = NULL;
    pool->task_queue_size = 0;
    TPOOL_LOG("线程池 %p 的内部任务队列已销毁。%d 个节点已释放。", (void *)pool, count);
}

// --- 工作线程函数 ---

/**
 * @brief 池中每个工作线程执行的主函数。
 *
 * 工作线程持续监控任务队列。当有可用任务且池处于活动状态时，
 * 它们将任务出队，执行它，然后释放由 `task_dequeue_internal` 分配的任务数据结构。
 * 如果池正在关闭且任务队列变空，则线程将退出。
 *
 * @param arg 指向 `thread_args_t` 结构的指针，包含池实例和线程的 ID。
 *            此结构由工作线程释放。
 * @return 线程终止时返回 NULL。
 */
static void *worker_thread_function(void *arg)
{
    thread_args_t *thread_args = (thread_args_t *)arg;
    thread_pool_t pool = thread_args->pool;
    int thread_id = thread_args->thread_id;
    free(thread_args); // 参数结构不再需要

    TPOOL_LOG("工作线程 #%d (ID: %lu) 已为线程池 %p 启动。", thread_id,
              (unsigned long)pthread_self(), (void *)pool);
    task_t *current_task_data = NULL; // 将指向由 task_dequeue_internal 分配的 task_t

    while (1) {
        pthread_mutex_lock(&(pool->lock));
        TPOOL_LOG("工作线程 #%d (线程池 %p): 已锁定池。", thread_id, (void *)pool);

        // 等待任务或关闭信号
        while (pool->task_queue_size == 0 && !pool->shutdown) {
            TPOOL_LOG("工作线程 #%d (线程池 %p): 等待任务。", thread_id, (void *)pool);
            // 标记线程为空闲状态
            pool->thread_status[thread_id] = 0; // 空闲
            pool->idle_threads++;
            pthread_cond_wait(&(pool->notify), &(pool->lock));
            TPOOL_LOG("工作线程 #%d (线程池 %p): 已被唤醒。", thread_id, (void *)pool);
            // 检查是否需要由于线程池缩小而退出
            if (pool->thread_status[thread_id] == -1) {
                TPOOL_LOG("工作线程 #%d (线程池 %p): 由于线程池缩小而退出。", thread_id, (void *)pool);
                pool->idle_threads--;
                pthread_mutex_unlock(&(pool->lock));
                pthread_exit(NULL);
            }
            pool->idle_threads--;
            pool->thread_status[thread_id] = 1; // 忙碌
        }

        // 退出循环的条件：池正在关闭 并且 队列为空
        if (pool->shutdown && pool->task_queue_size == 0) {
            pthread_mutex_unlock(&(pool->lock));
            TPOOL_LOG("工作线程 #%d (线程池 %p): 正在关闭 (队列为空)。", thread_id, (void *)pool);
            pthread_exit(NULL);
        }

        // 尝试出队一个任务
        current_task_data = task_dequeue_internal(pool);

        if (current_task_data != NULL) {
            // 任务成功出队
            TPOOL_LOG("工作线程 #%d (线程池 %p): 出队任务 '%s'。", thread_id, (void *)pool,
                      current_task_data->task_name);
            // 更新此线程的正在运行的任务名称
            snprintf(pool->running_task_names[thread_id], MAX_TASK_NAME_LEN, "%s",
                     current_task_data->task_name);
            // snprintf 会自动添加空终止符

            pthread_mutex_unlock(&(pool->lock)); // 执行任务前解锁
            TPOOL_LOG("工作线程 #%d (线程池 %p): 已解锁池，开始任务 '%s'。", thread_id,
                      (void *)pool, current_task_data->task_name);

            // 执行任务
            current_task_data->function(current_task_data->arg);
            TPOOL_LOG("工作线程 #%d (线程池 %p): 完成任务 '%s'。", thread_id, (void *)pool,
                      current_task_data->task_name);

            // 释放由 task_dequeue_internal 分配的 task_t 结构
            free(current_task_data);
            current_task_data = NULL;

            // 重新锁定以将状态更新为闲置
            pthread_mutex_lock(&(pool->lock));
            TPOOL_LOG("工作线程 #%d (线程池 %p): 已锁定池以设置状态为闲置。", thread_id,
                      (void *)pool);
            strncpy(pool->running_task_names[thread_id], "[idle]", MAX_TASK_NAME_LEN - 1);
            pool->running_task_names[thread_id][MAX_TASK_NAME_LEN - 1] = '\0';
            // 标记线程为空闲状态
            pool->thread_status[thread_id] = 0; // 空闲
            pool->idle_threads++;
        } else if (pool->shutdown) {
            // 如果 task_dequeue_internal 返回 NULL (例如，出队期间 malloc 失败) 并且 池正在关闭。
            // 这确保即使在关闭期间出队失败，线程也会退出。
            pthread_mutex_unlock(&(pool->lock));
            TPOOL_LOG(
                "工作线程 #%d (线程池 %p): 正在关闭 (任务为 NULL，可能在关闭或出队错误期间)。",
                thread_id, (void *)pool);
            pthread_exit(NULL);
        } else {
            // 任务为 NULL，但没有关闭。这表示 task_dequeue_internal 失败 (例如，malloc)。
            // task_dequeue_internal 会记录错误。
            // 工作线程继续循环并将重新评估条件。
            TPOOL_LOG("工作线程 #%d (线程池 %p): 发现任务为 NULL，但未关闭。将重新等待。",
                      thread_id, (void *)pool);
        }
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_LOG("工作线程 #%d (线程池 %p): 已解锁池，继续循环。", thread_id, (void *)pool);
    }
    return NULL; // 应该无法到达
}

// --- 公共 API 函数实现 ---

/**
 * @brief 创建一个新的线程池。
 *
 * 使用指定数量的工作线程初始化线程池。
 *
 * @param num_threads 要在池中创建的工作线程数。必须为正数。
 * @return 成功时返回指向新创建的 thread_pool_t 实例的指针，
 *         错误时返回 NULL (例如，内存分配失败，无效参数)。
 */
thread_pool_t thread_pool_create(int num_threads)
{
    // 确保日志模块已初始化
    static int log_initialized = 0;
    if (!log_initialized) {
        log_init("thread_pool.log", LOG_LEVEL_INFO);
        log_initialized = 1;
    }

    TPOOL_LOG("尝试创建包含 %d 个线程的线程池。", num_threads);
    if (num_threads <= 0) {
        TPOOL_ERROR("线程数必须为正。请求数: %d", num_threads);
        return NULL;
    }

    // 分配 thread_pool_s 结构本身
    thread_pool_t pool = (thread_pool_t)calloc(1, sizeof(struct thread_pool_s));
    if (pool == NULL) {
        TPOOL_ERROR("未能为线程池结构分配内存。");
        // perror("calloc for thread_pool_s"); // 更详细的系统错误
        return NULL;
    }

    pool->thread_count = num_threads;
    pool->min_threads = 1; // 默认最小线程数为1
    pool->max_threads = num_threads * 2; // 默认最大线程数为初始线程数的两倍
    pool->idle_threads = 0; // 初始时没有空闲线程
    pool->shutdown = 0; // 池处于活动状态
    pool->resize_shutdown = 0; // 没有线程需要由于缩小而退出
    pool->started = 0;  // 尚未启动任何线程
    pool->head = NULL;
    pool->tail = NULL;
    pool->task_queue_size = 0;

    // 初始化同步原语
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        TPOOL_ERROR("未能为线程池 %p 初始化互斥锁。", (void *)pool);
        // perror("pthread_mutex_init");
        free(pool);
        return NULL;
    }
    
    // 初始化调整大小互斥锁
    if (pthread_mutex_init(&pool->resize_lock, NULL) != 0) {
        TPOOL_ERROR("未能为线程池 %p 初始化调整大小互斥锁。", (void *)pool);
        pthread_mutex_destroy(&pool->lock);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->notify, NULL) != 0) {
        TPOOL_ERROR("未能为线程池 %p 初始化条件变量。", (void *)pool);
        // perror("pthread_cond_init");
        pthread_mutex_destroy(&pool->resize_lock); // 清理已成功初始化的调整大小互斥锁
        pthread_mutex_destroy(&pool->lock); // 清理已成功初始化的互斥锁
        free(pool);
        return NULL;
    }

    // 为线程 ID 分配数组
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        TPOOL_ERROR("未能为线程池 %p 的线程数组分配内存。", (void *)pool);
        // perror("malloc for pool->threads");
        pthread_mutex_destroy(&pool->resize_lock);
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        free(pool);
        return NULL;
    }
    
    // 为线程状态分配数组
    pool->thread_status = (int *)calloc(num_threads, sizeof(int));
    if (pool->thread_status == NULL) {
        TPOOL_ERROR("未能为线程池 %p 的线程状态数组分配内存。", (void *)pool);
        free(pool->threads);
        pthread_mutex_destroy(&pool->resize_lock);
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        free(pool);
        return NULL;
    }
    
    // 初始化线程状态为1（忙碌），因为线程创建后将立即开始工作
    for (int i = 0; i < num_threads; i++) {
        pool->thread_status[i] = 1;
    }

    // 为正在运行的任务名称分配数组
    pool->running_task_names = (char **)malloc(sizeof(char *) * num_threads);
    if (pool->running_task_names == NULL) {
        TPOOL_ERROR("未能为线程池 %p 的 running_task_names 数组分配内存。", (void *)pool);
        // perror("malloc for pool->running_task_names");
        free(pool->threads);
        pthread_mutex_destroy(&pool->lock);
        pthread_cond_destroy(&pool->notify);
        free(pool);
        return NULL;
    }

    // 为每个正在运行的任务名称分配单独的字符串并初始化为 "[idle]"
    for (int i = 0; i < num_threads; ++i) {
        pool->running_task_names[i] = (char *)malloc(MAX_TASK_NAME_LEN);
        if (pool->running_task_names[i] == NULL) {
            TPOOL_ERROR("未能为线程池 %p 的 running_task_name 字符串 #%d 分配内存。", i,
                        (void *)pool);
            // perror("malloc for pool->running_task_names[i]");
            // 清理先前分配的名称字符串
            for (int j = 0; j < i; ++j) {
                free(pool->running_task_names[j]);
            }
            free(pool->running_task_names);
            free(pool->threads);
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->notify);
            free(pool);
            return NULL;
        }
        strncpy(pool->running_task_names[i], "[idle]", MAX_TASK_NAME_LEN - 1);
        pool->running_task_names[i][MAX_TASK_NAME_LEN - 1] = '\0'; // 确保空终止
    }

    // 创建工作线程
    for (int i = 0; i < num_threads; ++i) {
        thread_args_t *args = (thread_args_t *)malloc(sizeof(thread_args_t));
        if (args == NULL) {
            TPOOL_ERROR("未能为线程池 %p 的线程 #%d 分配线程参数内存。", i, (void *)pool);
            // 这是一个严重错误；尝试优雅地关闭已创建的资源。
            pool->shutdown = 1; // 通知任何可能（但此时不太可能）正在运行的线程停止
            // 连接任何可能在先前迭代中成功创建的线程（如果这是第一次失败则不太可能）
            for (int k = 0; k < i; ++k) {
                pthread_join(pool->threads[k], NULL); // 此处为简洁起见省略了对 join 的错误检查
            }

            // 释放所有已分配的资源
            for (int j = 0; j < num_threads; ++j) { // 如果已分配，则释放所有名称槽
                if (pool->running_task_names[j]) {
                    free(pool->running_task_names[j]);
                }
            }
            free(pool->running_task_names);
            free(pool->threads);
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->notify);
            free(pool);
            return NULL;
        }
        args->pool = pool; // 传递不透明的池指针
        args->thread_id = i;

        if (pthread_create(&(pool->threads[i]), NULL, worker_thread_function, (void *)args) != 0) {
            TPOOL_ERROR("未能为线程池 %p 创建工作线程 #%d。", i, (void *)pool);
            // perror("pthread_create");
            free(args);         // 释放失败线程的参数
            pool->shutdown = 1; // 通知其他线程停止
            // 连接成功创建的线程
            for (int k = 0; k < i; ++k) {
                pthread_join(pool->threads[k], NULL);
            }

            for (int j = 0; j < num_threads; ++j) {
                if (pool->running_task_names[j]) {
                    free(pool->running_task_names[j]);
                }
            }
            free(pool->running_task_names);
            free(pool->threads);
            pthread_mutex_destroy(&pool->lock);
            pthread_cond_destroy(&pool->notify);
            free(pool);
            return NULL;
        }
        TPOOL_LOG("已为线程池 %p 成功创建工作线程 #%d。", i, (void *)pool);
        pool->started++; // 增加成功启动的线程计数
    }
    TPOOL_LOG("线程池 %p 已成功创建，包含 %d 个线程。", (void *)pool, pool->started);
    return pool;
}

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
                         const char *task_name)
{
    if (pool == NULL || function == NULL) {
        // 不将函数指针转换为 void*，避免警告
        TPOOL_ERROR("thread_pool_add_task: 池 (%p) 或函数指针为 NULL。", (void *)pool);
        return -1;
    }

    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_ERROR("无法添加任务 '%s'：线程池 %p 正在关闭。", task_name ? task_name : "unnamed",
                    (void *)pool);
        return -1;
    }

    task_t new_task; // task_t 在 .h 中定义，其大小已知
    new_task.function = function;
    new_task.arg = arg;

    // 准备任务名称
    if (task_name != NULL) {
        strncpy(new_task.task_name, task_name, MAX_TASK_NAME_LEN - 1);
        new_task.task_name[MAX_TASK_NAME_LEN - 1] = '\0'; // 确保空终止
    } else {
        strncpy(new_task.task_name, "unnamed_task", MAX_TASK_NAME_LEN - 1);
        new_task.task_name[MAX_TASK_NAME_LEN - 1] = '\0';
    }

    // 内部将任务入队
    if (task_enqueue_internal(pool, new_task) != 0) {
        pthread_mutex_unlock(&(pool->lock));
        // task_enqueue_internal 已记录错误
        TPOOL_ERROR("未能将任务 '%s' 入队到线程池 %p。", new_task.task_name, (void *)pool);
        return -1;
    }

    // 通知一个等待的工作线程
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
    TPOOL_LOG("任务 '%s' 已添加到线程池 %p。已通知工作线程。", new_task.task_name, (void *)pool);
    return 0;
}

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
int thread_pool_destroy(thread_pool_t pool)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_destroy: 池为 NULL。");
        return -1;
    }
    TPOOL_LOG("正在销毁线程池 %p。", (void *)pool);

    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) { // 检查是否已在关闭
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_LOG("线程池 %p 已标记为关闭或已销毁。", (void *)pool);
        // 允许多次调用 destroy，如果已关闭，则后续调用视为无操作。
        return 0;
    }
    pool->shutdown = 1; // 设置关闭标志
    TPOOL_LOG("线程池 %p 已标记为关闭。正在向所有工作线程广播。", (void *)pool);
    // 唤醒所有工作线程，以便它们可以检查关闭标志
    pthread_cond_broadcast(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));

    // 连接所有工作线程
    for (int i = 0; i < pool->thread_count; ++i) {
        TPOOL_LOG("正在连接线程池 %p 的线程 #%d (ID: %lu)。", (void *)pool, i,
                  (unsigned long)pool->threads[i]);
        if (pthread_join(pool->threads[i], NULL) != 0) {
            TPOOL_ERROR("未能连接线程池 %p 的线程 #%d (ID: %lu)。", (void *)pool, i,
                        (unsigned long)pool->threads[i]);
            // perror("pthread_join");
            // 即使一个失败，也继续尝试连接其他线程
        } else {
            TPOOL_LOG("已成功连接线程池 %p 的线程 #%d (ID: %lu)。", (void *)pool, i,
                      (unsigned long)pool->threads[i]);
        }
    }
    TPOOL_LOG("线程池 %p 的所有线程已连接。", (void *)pool);

    // 此时，所有工作线程都已退出。
    // 清理共享资源是安全的，对于队列销毁不需要主锁，
    // 但如果 destroy 可能被并发调用（它不应该），其他字段可能仍需要它。
    // 对于 task_queue_destroy_internal，由于工作线程已消失，因此不需要锁。
    task_queue_destroy_internal(pool);

    // 释放 running_task_names 数组及其字符串
    for (int i = 0; i < pool->thread_count; ++i) {
        if (pool->running_task_names[i]) {
            free(pool->running_task_names[i]);
        }
    }
    free(pool->running_task_names);
    TPOOL_LOG("已清理线程池 %p 的 running_task_names。", (void *)pool);

    // 销毁互斥锁和条件变量
    pthread_mutex_destroy(&(pool->lock));
    pthread_mutex_destroy(&(pool->resize_lock));
    pthread_cond_destroy(&(pool->notify));

    // 在释放池之前记录日志，避免释放后使用
    TPOOL_LOG("线程池 (%p) 即将销毁。", (void *)pool);

    // 释放线程数组、线程状态数组和池结构本身
    free(pool->threads);
    free(pool->thread_status);
    free(pool); // 释放 struct thread_pool_s

    // 注意：我们不在这里关闭日志模块，因为其他模块可能仍在使用它
    return 0;
}

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
char **thread_pool_get_running_task_names(thread_pool_t pool)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_get_running_task_names: 池为 NULL");
        return NULL;
    }
    TPOOL_LOG("正在获取线程池 %p 的正在运行的任务名称。", (void *)pool);

    // 用户需要知道 pool->thread_count 才能正确使用 free_running_task_names。
    // 此计数在池创建时固定。
    char **task_names_copy = (char **)malloc(sizeof(char *) * pool->thread_count);
    if (task_names_copy == NULL) {
        TPOOL_ERROR("未能为任务名称数组副本分配内存 (线程池 %p)。", (void *)pool);
        return NULL;
    }

    pthread_mutex_lock(&(pool->lock));
    for (int i = 0; i < pool->thread_count; ++i) {
        task_names_copy[i] = (char *)malloc(MAX_TASK_NAME_LEN);
        if (task_names_copy[i] == NULL) {
            TPOOL_ERROR("未能为任务名称字符串副本 #%d 分配内存 (线程池 %p)。", i, (void *)pool);
            // 清理此数组副本中先前分配的字符串
            for (int j = 0; j < i; ++j) {
                free(task_names_copy[j]);
            }
            free(task_names_copy); // 释放数组本身
            pthread_mutex_unlock(&(pool->lock));
            return NULL;
        }
        // pool->running_task_names[i] 保证为 MAX_TASK_NAME_LEN 且以空字符结尾
        strncpy(task_names_copy[i], pool->running_task_names[i], MAX_TASK_NAME_LEN);
        // 防御性空终止，尽管从已知大小、以空字符结尾的源进行 strncpy，
        // 其中目标缓冲区大小相同，如果使用 MAX_TASK_NAME_LEN，则应能处理。
        // 如果 strncpy 复制了 MAX_TASK_NAME_LEN 个字符而未找到空字符，则它不会进行空终止。
        // 然而，pool->running_task_names[i] 始终保持空终止。
        task_names_copy[i][MAX_TASK_NAME_LEN - 1] = '\0';
    }
    pthread_mutex_unlock(&(pool->lock));
    TPOOL_LOG("已成功复制线程池 %p 的正在运行的任务名称。", (void *)pool);
    return task_names_copy;
}

/**
 * @brief 释放由 `thread_pool_get_running_task_names` 返回的任务名称数组。
 *
 * @param task_names 要释放的字符串数组 (char **)。
 * @param count 数组中的字符串数量 (应与调用 `thread_pool_get_running_task_names` 时
 *              池的 thread_count 相匹配)。
 */
void free_running_task_names(char **task_names, int count)
{
    if (task_names == NULL) {
        TPOOL_LOG("free_running_task_names: task_names 数组为 NULL，无需释放。");
        return;
    }
    // 负数计数无效。计数为 0 表示空数组 (只需释放 task_names)。
    if (count < 0) {
        TPOOL_ERROR("free_running_task_names: 无效计数 %d。仅释放外部数组。", count);
        // 如果计数不可靠，则仅释放主数组以防止潜在的越界访问。
        free(task_names);
        return;
    }

    TPOOL_LOG("正在释放 %d 个复制的任务名称。", count);
    for (int i = 0; i < count; i++) {
        // 数组中的字符串指针可能为 NULL，如果 thread_pool_get_running_task_names
        // 中途分配失败（尽管它会尝试清理）。
        // free(NULL) 是安全的。
        free(task_names[i]); // 释放每个单独的字符串
    }
    free(task_names); // 释放数组本身
    TPOOL_LOG("复制的任务名称数组已释放。");
}

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
int thread_pool_resize(thread_pool_t pool, int new_thread_count)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_resize: 池为 NULL。");
        return -1;
    }

    if (new_thread_count < pool->min_threads || new_thread_count > pool->max_threads) {
        TPOOL_ERROR("thread_pool_resize: 新线程数量 %d 超出范围 [%d, %d]。",
                   new_thread_count, pool->min_threads, pool->max_threads);
        return -1;
    }

    // 如果池正在关闭，不允许调整大小
    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_ERROR("thread_pool_resize: 池正在关闭，不能调整大小。");
        return -1;
    }
    pthread_mutex_unlock(&(pool->lock));

    // 锁定调整大小互斥锁
    pthread_mutex_lock(&(pool->resize_lock));

    // 如果新线程数量与当前线程数量相同，不需要调整
    if (new_thread_count == pool->thread_count) {
        pthread_mutex_unlock(&(pool->resize_lock));
        TPOOL_LOG("thread_pool_resize: 新线程数量与当前线程数量相同，不需要调整。");
        return 0;
    }

    // 处理增加或减少线程数量的情况
    if (new_thread_count > pool->thread_count) {
        // 增加线程数量
        int threads_to_add = new_thread_count - pool->thread_count;
        TPOOL_LOG("thread_pool_resize: 增加 %d 个线程。", threads_to_add);

        // 重新分配线程数组和线程状态数组
        pthread_t *new_threads = (pthread_t *)realloc(pool->threads, sizeof(pthread_t) * new_thread_count);
        if (new_threads == NULL) {
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配线程数组失败。");
            return -1;
        }
        pool->threads = new_threads;

        int *new_thread_status = (int *)realloc(pool->thread_status, sizeof(int) * new_thread_count);
        if (new_thread_status == NULL) {
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配线程状态数组失败。");
            return -1;
        }
        pool->thread_status = new_thread_status;

        // 初始化新线程状态
        for (int i = pool->thread_count; i < new_thread_count; i++) {
            pool->thread_status[i] = 0; // 空闲
        }

        // 重新分配运行任务名称数组
        pthread_mutex_lock(&(pool->lock));
        char **new_running_task_names = (char **)realloc(pool->running_task_names, sizeof(char *) * new_thread_count);
        if (new_running_task_names == NULL) {
            // 注意锁的释放顺序：先释放内层锁，再释放外层锁
            // 获取锁的顺序是：先 resize_lock，再 lock
            // 所以释放顺序应该是：先 lock，再 resize_lock
            pthread_mutex_unlock(&(pool->lock));
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配运行任务名称数组失败。");
            return -1;
        }
        pool->running_task_names = new_running_task_names;

        // 为新线程分配运行任务名称字符串
        for (int i = pool->thread_count; i < new_thread_count; i++) {
            pool->running_task_names[i] = (char *)malloc(MAX_TASK_NAME_LEN);
            if (pool->running_task_names[i] == NULL) {
                // 如果分配失败，释放已分配的内存
                for (int j = pool->thread_count; j < i; j++) {
                    free(pool->running_task_names[j]);
                }
                pthread_mutex_unlock(&(pool->lock));
                pthread_mutex_unlock(&(pool->resize_lock));
                TPOOL_ERROR("thread_pool_resize: 为新线程分配运行任务名称字符串失败。");
                return -1;
            }
            strncpy(pool->running_task_names[i], "[idle]", MAX_TASK_NAME_LEN - 1);
            pool->running_task_names[i][MAX_TASK_NAME_LEN - 1] = '\0';
        }
        pthread_mutex_unlock(&(pool->lock));

        // 创建新线程
        for (int i = pool->thread_count; i < new_thread_count; i++) {
            thread_args_t *thread_args = (thread_args_t *)malloc(sizeof(thread_args_t));
            if (thread_args == NULL) {
                pthread_mutex_unlock(&(pool->resize_lock));
                TPOOL_ERROR("thread_pool_resize: 为新线程分配参数结构失败。");
                return -1;
            }
            thread_args->pool = pool;
            thread_args->thread_id = i;

            if (pthread_create(&(pool->threads[i]), NULL, worker_thread_function, thread_args) != 0) {
                free(thread_args);
                pthread_mutex_unlock(&(pool->resize_lock));
                TPOOL_ERROR("thread_pool_resize: 创建新线程失败。");
                return -1;
            }
            pool->started++;
            TPOOL_LOG("thread_pool_resize: 创建新线程 #%d。", i);
        }

        // 更新线程数量
        pool->thread_count = new_thread_count;
        pthread_mutex_unlock(&(pool->resize_lock));
        TPOOL_LOG("thread_pool_resize: 成功增加线程数量到 %d。", new_thread_count);
        return 0;
    }
    
    // 减少线程数量
    if (new_thread_count < pool->thread_count) {
        int threads_to_remove = pool->thread_count - new_thread_count;
        TPOOL_LOG("thread_pool_resize: 减少 %d 个线程。", threads_to_remove);

        // 标记需要退出的线程
        pthread_mutex_lock(&(pool->lock));
        int marked_count = 0;
        for (int i = 0; i < pool->thread_count && marked_count < threads_to_remove; i++) {
            if (pool->thread_status[i] == 0) { // 只标记空闲线程
                pool->thread_status[i] = -1; // 标记为退出
                marked_count++;
            }
        }

        // 如果标记的线程数量少于需要移除的线程数量，等待更多线程变为空闲
        if (marked_count < threads_to_remove) {
            TPOOL_LOG("thread_pool_resize: 只标记了 %d 个线程，需要等待更多线程变为空闲。", marked_count);
            pool->resize_shutdown = 1; // 设置缩小标志
        }

        // 唯醒所有线程，使其检查退出标志
        pthread_cond_broadcast(&(pool->notify));
        pthread_mutex_unlock(&(pool->lock));

        // 等待标记的线程退出
        int join_attempts = 0;
        const int max_join_attempts = 3; // 最大尝试次数
        for (int i = 0; i < pool->thread_count && marked_count > 0; i++) {
            if (pool->thread_status[i] == -1) {
                if (pthread_join(pool->threads[i], NULL) != 0) {
                    TPOOL_ERROR("thread_pool_resize: 等待线程 #%d 退出失败。", i);
                    join_attempts++;
                    if (join_attempts >= max_join_attempts) {
                        // 达到最大尝试次数，强制减少marked_count
                        marked_count--;
                        TPOOL_ERROR("thread_pool_resize: 放弃等待线程 #%d 退出。", i);
                    }
                } else {
                    TPOOL_LOG("thread_pool_resize: 线程 #%d 成功退出。", i);
                    marked_count--;
                }
            }
        }

        // 重新组织线程数组，将活动线程移到前面
        pthread_mutex_lock(&(pool->lock));
        int active_index = 0;
        for (int i = 0; i < pool->thread_count; i++) {
            if (pool->thread_status[i] != -1) { // 如果线程仍然活动
                if (active_index != i) {
                    // 移动线程 ID
                    pool->threads[active_index] = pool->threads[i];
                    // 移动线程状态
                    pool->thread_status[active_index] = pool->thread_status[i];
                    // 移动运行任务名称
                    char *temp = pool->running_task_names[active_index];
                    pool->running_task_names[active_index] = pool->running_task_names[i];
                    pool->running_task_names[i] = temp;
                }
                active_index++;
            }
        }

        // 释放不再需要的运行任务名称字符串
        for (int i = new_thread_count; i < pool->thread_count; i++) {
            free(pool->running_task_names[i]);
        }

        // 重新分配数组
        pthread_t *new_threads = (pthread_t *)realloc(pool->threads, sizeof(pthread_t) * new_thread_count);
        if (new_threads == NULL) {
            // 如果重新分配失败，标记已释放的指针为 NULL，避免内存泄漏
            for (int i = new_thread_count; i < pool->thread_count; i++) {
                pool->running_task_names[i] = NULL; // 标记为已释放
            }
            // 注意锁的释放顺序：先释放内层锁，再释放外层锁
            pthread_mutex_unlock(&(pool->lock));
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配线程数组失败。");
            return -1;
        }
        pool->threads = new_threads;

        int *new_thread_status = (int *)realloc(pool->thread_status, sizeof(int) * new_thread_count);
        if (new_thread_status == NULL) {
            // 如果重新分配失败，标记已释放的指针为 NULL，避免内存泄漏
            for (int i = new_thread_count; i < pool->thread_count; i++) {
                pool->running_task_names[i] = NULL; // 标记为已释放
            }
            // 注意锁的释放顺序：先释放内层锁，再释放外层锁
            pthread_mutex_unlock(&(pool->lock));
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配线程状态数组失败。");
            return -1;
        }
        pool->thread_status = new_thread_status;

        char **new_running_task_names = (char **)realloc(pool->running_task_names, sizeof(char *) * new_thread_count);
        if (new_running_task_names == NULL) {
            // 如果重新分配失败，标记已释放的指针为 NULL，避免内存泄漏
            for (int i = new_thread_count; i < pool->thread_count; i++) {
                pool->running_task_names[i] = NULL; // 标记为已释放
            }
            // 注意锁的释放顺序：先释放内层锁，再释放外层锁
            pthread_mutex_unlock(&(pool->lock));
            pthread_mutex_unlock(&(pool->resize_lock));
            TPOOL_ERROR("thread_pool_resize: 重新分配运行任务名称数组失败。");
            return -1;
        }
        pool->running_task_names = new_running_task_names;

        // 重置缩小标志
        pool->resize_shutdown = 0;

        // 更新线程数量
        pool->thread_count = new_thread_count;
        
        // 更新线程状态数组中的线程 ID
        // 这确保了在重新组织线程数组后，每个工作线程的 ID 与其在数组中的索引一致
        // 这对于下一次调整线程池大小时的正确性至关重要
        for (int i = 0; i < new_thread_count; i++) {
            // 注意：我们不能直接修改线程 ID，因为线程已经在运行
            // 但我们可以更新线程状态数组中的索引信息，以便于下一次调整
            // 在工作线程函数中，线程 ID 是通过 thread_args 传递的
            // 因此这里的更新不会影响已经运行的线程
            // 但会确保下一次调整时的正确性
            pool->thread_status[i] = (pool->thread_status[i] == 0) ? 0 : 1; // 保持空闲/忙碌状态不变
        }
        
        pthread_mutex_unlock(&(pool->lock));

        pthread_mutex_unlock(&(pool->resize_lock));
        TPOOL_LOG("thread_pool_resize: 成功减少线程数量到 %d。", new_thread_count);
        return 0;
    }
    // 如果新线程数量等于当前线程数量，不需要调整，已在前面处理
    return 0;
}

/**
 * @brief 获取线程池的统计信息。
 *
 * 返回线程池的当前状态信息，包括线程数量、任务队列长度等。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param stats 指向 thread_pool_stats_t 结构的指针，用于存储统计信息。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，stats 为 NULL)。
 */
int thread_pool_get_stats(thread_pool_t pool, thread_pool_stats_t *stats)
{
    if (pool == NULL || stats == NULL) {
        TPOOL_ERROR("thread_pool_get_stats: 池或统计信息结构为 NULL。");
        return -1;
    }

    pthread_mutex_lock(&(pool->lock));
    stats->thread_count = pool->thread_count;
    stats->min_threads = pool->min_threads;
    stats->max_threads = pool->max_threads;
    stats->idle_threads = pool->idle_threads;
    stats->task_queue_size = pool->task_queue_size;
    stats->started = pool->started;
    pthread_mutex_unlock(&(pool->lock));

    TPOOL_LOG("thread_pool_get_stats: 成功获取线程池 %p 的统计信息。", (void *)pool);
    return 0;
}

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
int thread_pool_set_limits(thread_pool_t pool, int min_threads, int max_threads)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_set_limits: 池为 NULL。");
        return -1;
    }

    if (min_threads <= 0 || max_threads < min_threads) {
        TPOOL_ERROR("thread_pool_set_limits: 无效的线程数量范围 [%d, %d]。", min_threads, max_threads);
        return -1;
    }

    // 如果池正在关闭，不允许设置限制
    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_ERROR("thread_pool_set_limits: 池正在关闭，不能设置限制。");
        return -1;
    }
    pthread_mutex_unlock(&(pool->lock));

    // 锁定调整大小互斥锁
    pthread_mutex_lock(&(pool->resize_lock));

    // 更新线程数量范围
    pool->min_threads = min_threads;
    pool->max_threads = max_threads;

    // 如果当前线程数量超出新的范围，调整线程数量
    int current_thread_count = pool->thread_count;
    pthread_mutex_unlock(&(pool->resize_lock));

    // 如果当前线程数量小于新的最小线程数量，增加线程
    if (current_thread_count < min_threads) {
        TPOOL_LOG("thread_pool_set_limits: 当前线程数量 %d 小于新的最小线程数量 %d，增加线程。",
                  current_thread_count, min_threads);
        return thread_pool_resize(pool, min_threads);
    }
    
    // 如果当前线程数量大于新的最大线程数量，减少线程
    if (current_thread_count > max_threads) {
        TPOOL_LOG("thread_pool_set_limits: 当前线程数量 %d 大于新的最大线程数量 %d，减少线程。",
                  current_thread_count, max_threads);
        return thread_pool_resize(pool, max_threads);
    }

    TPOOL_LOG("thread_pool_set_limits: 成功设置线程池 %p 的线程数量范围为 [%d, %d]。",
              (void *)pool, min_threads, max_threads);
    return 0;
}
