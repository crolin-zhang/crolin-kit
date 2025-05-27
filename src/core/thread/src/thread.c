/**
 * @file thread.c
 * @brief 线程池库的实现。
 *
 * 此文件包含线程池的内部定义和函数实现，
 * 包括任务队列管理、工作线程逻辑以及公共 API 函数。
 */
#include "thread_internal.h" // 包含内部结构和函数声明
#include "log.h"
#include <errno.h> // 用于 strerror() 函数获取错误信息
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // For sleep()

// 前向声明内部静态函数
static void *auto_adjust_thread_function(void *arg);

// --- 任务队列管理函数 (内部) ---

/**
 * @brief 按优先级向队列中添加任务 (内部函数)。
 *
 * 此函数假定调用者 (例如, `thread_pool_add_task`)
 * 持有池的锁。它分配一个新的任务节点并将其按优先级插入到队列中。
 * 优先级值越小，优先级越高，将被插入到队列的更前面位置。
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

    if (pool->head == NULL) { // 队列为空
        pool->head = new_node;
        pool->tail = new_node;
    } else {
        // 按优先级插入任务
        // 优先级值越小，优先级越高，将被插入到队列的更前面位置
        if (task.priority < pool->head->task.priority) {
            // 如果新任务优先级高于队列头部任务，插入到队列头部
            new_node->next = pool->head;
            pool->head = new_node;
        } else {
            // 否则，找到合适的位置插入
            task_node_t *current = pool->head;
            task_node_t *previous = NULL;
            
            // 遍历队列，找到第一个优先级低于新任务的节点
            while (current != NULL && current->task.priority <= task.priority) {
                previous = current;
                current = current->next;
            }
            
            // 插入新节点
            if (previous == NULL) { // 理论上不会发生，因为已经处理了头部插入的情况
                new_node->next = pool->head;
                pool->head = new_node;
            } else {
                new_node->next = previous->next;
                previous->next = new_node;
                
                // 如果插入到了队列尾部，更新tail指针
                if (previous == pool->tail) {
                    pool->tail = new_node;
                }
            }
        }
    }
    
    pool->task_queue_size++;
    TPOOL_DEBUG("任务 '%s' (优先级:%d) 已按优先级入队。线程池: %p, 队列大小: %d", 
              task.task_name, task.priority, (void *)pool, pool->task_queue_size);

    // 检查是否需要调整线程数 (在锁内进行)
    // 直接信号自动调整线程，避免多重嵌套锁定
    if (pool->auto_adjust) {
        // 如果队列大小增加，可能需要增加线程
        if (pool->task_queue_size > pool->high_watermark && pool->thread_count < pool->max_threads) {
            TPOOL_DEBUG("任务入队触发自动调整检查：任务队列大小 %d > 高水位 %d", 
                     pool->task_queue_size, pool->high_watermark);
            // 向自动调整线程发送信号，让它检查是否需要调整线程池大小
            pthread_mutex_lock(&pool->adjust_cond_lock);
            pthread_cond_signal(&pool->adjust_cond);
            pthread_mutex_unlock(&pool->adjust_cond_lock);
        }
    }
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
        TPOOL_TRACE("task_dequeue_internal: 尝试从线程池 %p 的空队列中出队。", (void *)pool);
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
    TPOOL_DEBUG("任务 '%s' 已从线程池 %p 内部出队。队列大小: %d", dequeued_task_data->task_name,
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
    TPOOL_DEBUG("线程池 %p 的内部任务队列已销毁。%d 个节点已释放。", (void *)pool, count);
}

// --- 工作线程函数 ---

// 前向声明 worker_thread_function，因为 thread_pool_resize 中创建线程时会用到
// 如果定义在 resize 之前，则不需要这个
// static void *worker_thread_function(void *arg); // 实际上定义在 resize
// 之前，所以不需要显式前向声明于此

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
    free(thread_args); // 释放参数结构

    TPOOL_LOG("工作线程 #%d (线程池 %p): 已启动。", thread_id, (void *)pool);

    // 循环直到池关闭且队列为空，或者线程被标记为退出
    while (1) {
        pthread_mutex_lock(&(pool->lock));
        TPOOL_TRACE("工作线程 #%d (线程池 %p): 已锁定池。", thread_id, (void *)pool);

        // 检查线程ID是否超出范围（可能在调整大小后发生）
        if (thread_id >= pool->thread_count) {
            TPOOL_DEBUG("工作线程 #%d (线程池 %p): 线程ID超出范围（当前线程数: %d），准备退出", thread_id,
                      (void *)pool, pool->thread_count);
            // 在此处不立即退出，让线程继续等待通知，在收到通知后再检查是否需要退出
        }

        // 先检查是否有任务，如果队列为空且线程池未关闭，则等待
        // 这是关键修改点：使用谓词条件而不是盲目等待
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 最多等待1秒，防止永久阻塞
        
        while (pool->task_queue_size == 0 && !pool->shutdown && 
              thread_id < pool->thread_count && 
              (thread_id >= pool->thread_count || pool->thread_status[thread_id] >= 0)) {
            // 使用超时等待，防止永久阻塞
            int wait_result = pthread_cond_timedwait(&(pool->notify), &(pool->lock), &timeout);
            if (wait_result == ETIMEDOUT) {
                // 超时后，检查一下队列是否有新任务，可能信号丢失
                if (pool->task_queue_size > 0) {
                    TPOOL_DEBUG("工作线程 #%d (线程池 %p): 等待超时但发现有任务，继续处理", 
                             thread_id, (void *)pool);
                    break;
                }
                // 重置超时，再等待一秒
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 1;
            }
        }
        TPOOL_TRACE("工作线程 #%d (线程池 %p): 已被唤醒。", thread_id, (void *)pool);

        // 检查是否应该退出（增加对线程ID范围的检查）
        if ((pool->shutdown && pool->task_queue_size == 0) ||
            thread_id >= pool->thread_count ||
            (thread_id < pool->thread_count && pool->thread_status[thread_id] < 0)) {
            // 如果是空闲状态，减少空闲线程计数
            if (thread_id < pool->thread_count && pool->thread_status[thread_id] == 0) {
                pool->idle_threads--;
                TPOOL_DEBUG("工作线程 #%d (线程池 %p): 退出前减少空闲线程计数，当前空闲线程数: %d",
                          thread_id, (void *)pool, pool->idle_threads);
            }

            // 清除线程状态，避免后续访问（仅当线程ID在有效范围内时）
            if (thread_id < pool->thread_count) {
                pool->thread_status[thread_id] = -1;
            }

            // 初始化退出原因变量为默认值，避克lint错误
            const char* exit_reason = "(未知原因)";
            if (thread_id >= pool->thread_count) {
                exit_reason = "(由于线程ID超出范围)";
            } else if (thread_id < pool->thread_count && pool->thread_status[thread_id] < 0) {
                exit_reason = "(由于调整大小)";
            } else if (pool->shutdown) {
                exit_reason = "(由于关闭)";
            }
            TPOOL_LOG("工作线程 #%d (线程池 %p): 正在退出。%s", thread_id, (void *)pool, exit_reason);
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }

        // 获取任务
        task_t *task = NULL;
        if (pool->task_queue_size > 0) {
            task = task_dequeue_internal(pool);
        }
        
        if (task == NULL) {
            // 队列为空，唤醒其他线程然后继续循环
            // 这有助于防止所有线程都在等待而没有线程检查任务队列的情况
            pthread_cond_broadcast(&(pool->notify));
            pthread_mutex_unlock(&(pool->lock));
            continue;
        }

        // 标记为忙碌
        if (pool->thread_status[thread_id] == 0) { // 如果是空闲状态
            pool->idle_threads--;
            TPOOL_DEBUG("工作线程 #%d (线程池 %p): 设置为忙碌，空闲线程数: %d", thread_id,
                      (void *)pool, pool->idle_threads);
        }
        pool->thread_status[thread_id] = 1; // 设置为忙碌

        // 更新运行任务名称 - 使用更安全的方式复制字符串
        // 使用snprintf而不是strncpy，避免编译器警告
        snprintf(pool->running_task_names[thread_id], MAX_TASK_NAME_LEN, "%s", task->task_name);

        TPOOL_DEBUG("工作线程 #%d (线程池 %p): 出队任务 '%s'。", thread_id, (void *)pool,
                  task->task_name);

        // 解锁池，允许其他线程访问
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_DEBUG("工作线程 #%d (线程池 %p): 开始任务 '%s'。", thread_id, (void *)pool,
                  task->task_name);

        // 执行任务
        (*(task->function))(task->arg);

        // 任务完成
        TPOOL_DEBUG("工作线程 #%d (线程池 %p): 完成任务 '%s'。", thread_id, (void *)pool,
                  task->task_name);

        // 释放任务结构
        free(task);

        // 重新锁定池以设置状态为空闲
        pthread_mutex_lock(&(pool->lock));
        TPOOL_TRACE("工作线程 #%d (线程池 %p): 已锁定池以设置状态为闲置。", thread_id, (void *)pool);

        // 检查线程ID是否仍然有效（可能在执行任务期间池被调整大小）
        if (thread_id >= pool->thread_count) {
            TPOOL_DEBUG("工作线程 #%d (线程池 %p): 任务完成后发现线程ID超出范围（当前线程数: %d）。"
                     "将在下一次循环优雅退出。",
                     thread_id, (void *)pool, pool->thread_count);
            // 不立即退出，先解锁并在下一个循环中优雅退出
            // 这样可以避免在持有锁的情况下突然退出
            pthread_mutex_unlock(&(pool->lock));
            continue;
        }

        // 设置为空闲状态
        if (pool->thread_status[thread_id] != 0) { // 如果不是已经空闲
            pool->thread_status[thread_id] = 0;
            // 状态更新已在 pool->lock 保护下
            TPOOL_DEBUG("工作线程 #%d (线程池 %p): 任务完成，准备更新状态为闲置。", thread_id,
                      (void *)pool);
            strncpy(pool->running_task_names[thread_id], "[idle]", MAX_TASK_NAME_LEN - 1);
            pool->running_task_names[thread_id][MAX_TASK_NAME_LEN - 1] = '\0';
            // 标记线程为空闲状态
            pool->thread_status[thread_id] = 0; // 空闲
            pool->idle_threads++;

            // 任务完成后信号自动调整线程检查是否需要调整线程池大小
            if (pool->auto_adjust) {
                // 如果空闲线程数量超过低水位线，可能需要减少线程
                if (pool->idle_threads > pool->low_watermark && pool->thread_count > pool->min_threads) {
                    TPOOL_DEBUG("工作线程 #%d (线程池 %p): 触发自动调整检查，空闲线程数 %d > 低水位 %d",
                              thread_id, (void *)pool, pool->idle_threads, pool->low_watermark);
                    
                    // 暂时释放线程池锁，防止死锁
                    pthread_mutex_unlock(&(pool->lock));
                    
                    // 向自动调整线程发送信号
                    pthread_mutex_lock(&pool->adjust_cond_lock);
                    pthread_cond_signal(&pool->adjust_cond);
                    pthread_mutex_unlock(&pool->adjust_cond_lock);
                    
                    // 同时唤醒所有工作线程，确保没有线程永久阻塞
                    pthread_cond_broadcast(&(pool->notify));
                    
                    // 重新获取线程池锁
                    pthread_mutex_lock(&(pool->lock));
                }
            }
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
            TPOOL_DEBUG("工作线程 #%d (线程池 %p): 发现任务为 NULL，但未关闭。将重新等待。",
                      thread_id, (void *)pool);
        }
        // 任务完成或状态变更，唤醒其他线程查看是否有新任务
        pthread_cond_broadcast(&(pool->notify));
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_TRACE("工作线程 #%d (线程池 %p): 已解锁池，继续循环。", thread_id, (void *)pool);
    }
    return NULL; // 应该无法到达
}

// --- 自动调整相关函数 ---

/**
 * @brief 自动调整线程的主执行函数。
 *
 * 此线程定期检查线程池状态并调整其大小。
 * 它使用 pthread_cond_timedwait 来等待超时或收到信号。
 *
 * @param arg 指向 thread_pool_s 实例的指针。
 * @return NULL。
 */
static void *auto_adjust_thread_function(void *arg)
{
    thread_pool_t pool = (thread_pool_t)arg;
    struct timespec current_time;
    struct timespec timeout_spec;

    if (pool == NULL) {
        TPOOL_ERROR("自动调整线程: 收到 NULL 池参数。正在退出。");
        return NULL;
    }

    TPOOL_DEBUG("自动调整线程 (线程池 %p): 已启动。调整间隔: %d ms。", (void *)pool,
              pool->adjust_interval);

    pthread_mutex_lock(&pool->adjust_cond_lock);

    while (pool->adjust_thread_running) {
        // 首先检查线程池是否已请求关闭
        if (pool->shutdown) {
            TPOOL_DEBUG("自动调整线程 (线程池 %p): 检测到池已关闭，正在退出。", (void *)pool);
            break;
        }
        
        // 计算下一次唤醒时间
        if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
            TPOOL_ERROR("自动调整线程 (线程池 %p): clock_gettime 失败。errno: %d (%s)。将尝试 %d "
                        "ms 后重试。",
                        (void *)pool, errno, strerror(errno), pool->adjust_interval);
            timeout_spec.tv_sec = current_time.tv_sec;
            timeout_spec.tv_nsec = current_time.tv_nsec;
        }
        
        timeout_spec.tv_sec = current_time.tv_sec + pool->adjust_interval / 1000;
        timeout_spec.tv_nsec = current_time.tv_nsec + (pool->adjust_interval % 1000) * 1000000L;
        if (timeout_spec.tv_nsec >= 1000000000L) {
            timeout_spec.tv_sec++;
            timeout_spec.tv_nsec -= 1000000000L;
        }

        TPOOL_TRACE("自动调整线程 (线程池 %p): 等待 %d ms 或信号...", (void *)pool,
                  pool->adjust_interval);
        int wait_result = pthread_cond_timedwait(&pool->adjust_cond, &pool->adjust_cond_lock, &timeout_spec);

        // \u4f7f\u7528 wait_result \u7684\u7ed3\u679c\u6765\u4e00\u4e9b\u540e\u7eed\u6761\u4ef6\u7684\u60c5\u51b5
        if (wait_result == ETIMEDOUT) {
            TPOOL_TRACE("自动调整线程 (线程池 %p): 等待超时。", (void *)pool);
        } else if (wait_result != 0) {
            TPOOL_ERROR("自动调整线程 (线程池 %p): pthread_cond_timedwait 失败。errno: %d (%s)。", (void *)pool, errno, strerror(errno));
        }

        // \u518d\u6b21\u68c0\u67e5\u7ebf\u7a0b\u6c60\u72b6\u6001
        if (!pool->adjust_thread_running || pool->shutdown) {
            TPOOL_DEBUG("自动调整线程 (线程池 %p): 收到退出信号，循环终止。", (void *)pool);
            break;
        }

        // 执行自动调整检查 - 释放 adjust_cond_lock 并获取 pool->lock
        pthread_mutex_unlock(&pool->adjust_cond_lock);
        pthread_mutex_lock(&pool->lock);
        
        if (pool->auto_adjust && !pool->shutdown) {
            int current_threads = pool->thread_count;
            int tasks_in_queue = pool->task_queue_size;
            int idle = pool->idle_threads;
            int target_threads = current_threads;
            
            TPOOL_DEBUG("自动调整检查: 当前线程=%d, 任务队列=%d (高水位=%d), 空闲线程=%d (低水位=%d), "
                      "最小线程=%d, 最大线程=%d",
                      current_threads, tasks_in_queue, pool->high_watermark,
                      idle, pool->low_watermark, pool->min_threads, pool->max_threads);
            
            // 检查是否需要增加线程
            if (tasks_in_queue > pool->high_watermark && current_threads < pool->max_threads) {
                target_threads = current_threads + 1;
                TPOOL_DEBUG("自动调整: 任务队列 (%d) > 高水位 (%d)。建议增加线程数至 %d。", 
                          tasks_in_queue, pool->high_watermark, target_threads);
            }
            // 检查是否需要减少线程
            else if (idle > pool->low_watermark && current_threads > pool->min_threads) {
                target_threads = current_threads - 1;
                TPOOL_DEBUG("自动调整: 空闲线程 (%d) > 低水位 (%d)。建议减少线程数至 %d。", 
                          idle, pool->low_watermark, target_threads);
            }
            
            // 如果需要调整线程数
            if (target_threads != current_threads) {
                // 确保目标线程数在有效范围内
                if (target_threads > pool->max_threads) {
                    target_threads = pool->max_threads;
                }
                if (target_threads < pool->min_threads) {
                    target_threads = pool->min_threads;
                }
                
                // 再次检查有效性
                if (target_threads != current_threads) {
                    TPOOL_DEBUG("自动调整: 决定调整线程数从 %d 到 %d。", current_threads, target_threads);
                    
                    // 保存目标线程数并释放锁，防止死锁
                    int resize_target = target_threads;
                    pthread_mutex_unlock(&pool->lock);
                    
                    // 在锁外调用 thread_pool_resize
                    if (thread_pool_resize(pool, resize_target) != 0) {
                        TPOOL_ERROR("自动调整: thread_pool_resize(%d -> %d) 失败。", 
                                    current_threads, resize_target);
                    }
                    
                    // 重新获取 adjust_cond_lock 并继续循环
                    pthread_mutex_lock(&pool->adjust_cond_lock);
                    continue;
                }
            }
        }
        
        // 释放 pool->lock
        pthread_mutex_unlock(&pool->lock);
        
        // 重新获取 adjust_cond_lock
        pthread_mutex_lock(&pool->adjust_cond_lock);
    }

    pthread_mutex_unlock(&pool->adjust_cond_lock);
    TPOOL_DEBUG("自动调整线程 (线程池 %p): 已退出。", (void *)pool);
    return NULL;
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
        // 获取环境变量中设置的日志级别
        const char *log_level_str = getenv("LOG_LEVEL");
        log_level_t level = LOG_LEVEL_INFO; // 默认使用 INFO 级别
        
        if (log_level_str != NULL) {
            if (strcasecmp(log_level_str, "FATAL") == 0) {
                level = LOG_LEVEL_FATAL;
            } else if (strcasecmp(log_level_str, "ERROR") == 0) {
                level = LOG_LEVEL_ERROR;
            } else if (strcasecmp(log_level_str, "WARN") == 0) {
                level = LOG_LEVEL_WARN;
            } else if (strcasecmp(log_level_str, "INFO") == 0) {
                level = LOG_LEVEL_INFO;
            } else if (strcasecmp(log_level_str, "DEBUG") == 0) {
                level = LOG_LEVEL_DEBUG;
            } else if (strcasecmp(log_level_str, "TRACE") == 0) {
                level = LOG_LEVEL_TRACE;
            }
        }
        
        // 使用环境变量中设置的日志级别初始化日志模块
        log_init("thread_pool.log", level);
        log_initialized = 1;
    }

    TPOOL_DEBUG("尝试创建包含 %d 个线程的线程池。", num_threads);
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
    pool->min_threads = 1;               // 默认最小线程数为1
    pool->max_threads = num_threads * 2; // 默认最大线程数为初始线程数的两倍
    pool->idle_threads = 0;              // 初始时没有空闲线程
    pool->shutdown = 0;                  // 池处于活动状态
    pool->resize_shutdown = 0;           // 没有线程需要由于缩小而退出
    pool->started = 0;                   // 尚未启动任何线程
    pool->head = NULL;
    pool->tail = NULL;
    pool->task_queue_size = 0;

    // 初始化自动调整相关字段
    pool->auto_adjust = 0;                 // 默认禁用自动调整
    pool->high_watermark = num_threads;    // 默认任务队列高水位线为线程数
    pool->low_watermark = num_threads / 2; // 默认空闲线程高水位线为线程数的一半
    pool->adjust_interval = 5000;          // 默认调整间隔为5秒
    pool->last_adjust_time = time(NULL);   // 初始化上次调整时间

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
        pthread_mutex_destroy(&pool->lock);        // 清理已成功初始化的互斥锁
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
        TPOOL_DEBUG("已为线程池 %p 成功创建工作线程 #%d。", (void *)pool, i);
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
 * @param priority 任务的优先级，决定执行顺序。默认为 TASK_PRIORITY_NORMAL。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，function 为 NULL，
 *         池正在关闭，任务节点的内存分配失败)。
 */
int thread_pool_add_task(thread_pool_t pool, void (*function)(void *), void *arg,
                         const char *task_name, task_priority_t priority)
{
    if (pool == NULL || function == NULL) {
        TPOOL_ERROR("thread_pool_add_task: 无效参数 (pool: %p, function: %p)", (void *)pool,
                    (void *)function);
        return -1;
    }

    pthread_mutex_lock(&(pool->lock));

    if (pool->shutdown) {
        TPOOL_ERROR("thread_pool_add_task: 尝试向正在关闭的线程池 %p 添加任务", (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        return -1;
    }

    // 准备任务数据
    task_t task;
    task.function = function;
    task.arg = arg;
    task.priority = priority; // 设置任务优先级

    // 复制任务名称，确保不会超出缓冲区
    if (task_name == NULL) {
        strncpy(task.task_name, "unnamed_task", MAX_TASK_NAME_LEN - 1);
    } else {
        strncpy(task.task_name, task_name, MAX_TASK_NAME_LEN - 1);
    }
    task.task_name[MAX_TASK_NAME_LEN - 1] = '\0'; // 确保以空字符结尾

    // 将任务添加到队列
    int result = task_enqueue_internal(pool, task);
    if (result == 0) {
        // 通知一个等待的工作线程有新任务
        pthread_cond_signal(&(pool->notify));
    }

    // 释放锁以避免死锁
    pthread_mutex_unlock(&(pool->lock));
    
    // 如果启用了自动调整，则向自动调整线程发送信号
    if (pool->auto_adjust) {
        pthread_mutex_lock(&pool->adjust_cond_lock);
        pthread_cond_signal(&pool->adjust_cond);
        pthread_mutex_unlock(&pool->adjust_cond_lock);
    }

    TPOOL_DEBUG("任务 '%s' 已添加到线程池 %p。已通知工作线程。", task.task_name, (void *)pool);
    return result;
}

/**
 * @brief向线程池的队列中添加一个新任务（使用默认优先级）。
 *
 * 该任务将被一个可用的工作线程拾取以执行，使用TASK_PRIORITY_NORMAL优先级。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @param function 指向定义任务的函数的指针。不能为空。
 * @param arg 要传递给任务函数的参数。如果函数期望，可以为 NULL。
 * @param task_name 任务的描述性名称。如果为 NULL，将使用 "unnamed_task"。
 *                  该名称被复制到任务结构中。
 * @return 成功时返回 0，错误时返回 -1 (例如，pool 为 NULL，function 为 NULL，
 *         池正在关闭，任务节点的内存分配失败)。
 */
int thread_pool_add_task_default(thread_pool_t pool, void (*function)(void *), void *arg,
                                const char *task_name)
{
    return thread_pool_add_task(pool, function, arg, task_name, TASK_PRIORITY_NORMAL);
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
/**
 * @brief 启用线程池自动动态调整功能
 *
 * 根据任务队列长度和空闲线程数量自动调整线程池大小。
 * 当任务队列长度超过high_watermark时，会增加线程数量。
 * 当空闲线程数量超过low_watermark时，会减少线程数量。
 * 线程数量的调整会在指定的时间间隔内最多进行一次，以避免频繁调整。
 * 线程数量始终保持在通过 thread_pool_set_limits 或 thread_pool_create 设置的 min_threads 和
 * max_threads 之间。
 *
 * @param pool 指向线程池实例的指针。
 * @param high_watermark 任务队列高水位线，当任务队列长度超过此值时增加线程。
 * @param low_watermark 空闲线程高水位线，当空闲线程数超过此值时减少线程。
 * @param adjust_interval 调整检查间隔（毫秒）。
 * @return 成功返回0，失败返回-1（例如，pool为NULL，参数无效，或池内min/max线程数未正确设置）。
 */
int thread_pool_enable_auto_adjust(thread_pool_t pool, int high_watermark, int low_watermark,
                                   int adjust_interval)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 池为 NULL。");
        return -1;
    }

    // 检查池中预设的 min_threads 和 max_threads 是否有效
    // 这些值应由 thread_pool_create 或 thread_pool_set_limits 设置
    pthread_mutex_lock(&pool->lock); // 需要锁来安全地读取 min_threads/max_threads
    if (pool->min_threads <= 0 || pool->max_threads <= 0 || pool->min_threads > pool->max_threads) {
        pthread_mutex_unlock(&pool->lock);
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 池中预设的 min_threads (%d) 或 max_threads "
                    "(%d) 无效。请先通过 thread_pool_set_limits 设置。",
                    pool->min_threads, pool->max_threads);
        return -1;
    }
    // 立即释放锁，因为后续参数检查不需要它
    // 如果后续操作需要锁，则在适当的时候重新获取
    // pthread_mutex_unlock(&pool->lock); // 暂时注释掉，因为下面很快会再次用到

    if (high_watermark <= 0 || low_watermark < 0 || adjust_interval <= 0) { // low_watermark 可以为0
        pthread_mutex_unlock(&pool->lock); // 确保在所有错误返回路径上都释放锁
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 无效的水位线或调整间隔。 high_watermark=%d, "
                    "low_watermark=%d, interval=%dms",
                    high_watermark, low_watermark, adjust_interval);
        return -1;
    }

    // pthread_mutex_lock(&pool->lock); // 已在上面获取
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->lock);
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 线程池 %p 正在关闭，无法启用自动调整。",
                    (void *)pool);
        return -1;
    }

    if (pool->auto_adjust) {
        TPOOL_DEBUG("线程池 %p 自动调整已启用。正在使用新参数更新...", (void *)pool);
        pthread_mutex_lock(&pool->adjust_cond_lock);
        pool->high_watermark = high_watermark;
        pool->low_watermark = low_watermark;
        pool->adjust_interval = adjust_interval;
        pthread_cond_signal(&pool->adjust_cond);
        pthread_mutex_unlock(&pool->adjust_cond_lock);
        pthread_mutex_unlock(&pool->lock); // 释放外层 pool->lock
        TPOOL_DEBUG("线程池 %p 自动调整参数已更新。high_wm=%d, low_wm=%d, interval=%dms",
                  high_watermark, low_watermark, adjust_interval);
        return 0;
    }

    // 设置参数
    pool->high_watermark = high_watermark;
    pool->low_watermark = low_watermark;
    pool->adjust_interval = adjust_interval;
    pool->auto_adjust = 1;

    if (pthread_mutex_init(&pool->adjust_cond_lock, NULL) != 0) {
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 初始化 adjust_cond_lock 失败。");
        pool->auto_adjust = 0; // 回滚
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }
    if (pthread_cond_init(&pool->adjust_cond, NULL) != 0) {
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 初始化 adjust_cond 失败。");
        pthread_mutex_destroy(&pool->adjust_cond_lock);
        pool->auto_adjust = 0; // 回滚
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    pool->adjust_thread_running = 1;

    if (pthread_create(&pool->adjust_thread, NULL, auto_adjust_thread_function, (void *)pool) !=
        0) {
        TPOOL_ERROR("thread_pool_enable_auto_adjust: 创建自动调整线程失败。");
        pool->adjust_thread_running = 0;
        pool->auto_adjust = 0;
        pthread_mutex_destroy(&pool->adjust_cond_lock);
        pthread_cond_destroy(&pool->adjust_cond);
        pthread_mutex_unlock(&pool->lock);
        return -1;
    }

    TPOOL_LOG(
        "线程池 %p 已成功启用自动调整功能。min=%d, max=%d, high_wm=%d, low_wm=%d, interval=%dms",
        (void *)pool, pool->min_threads, pool->max_threads, high_watermark, low_watermark,
        adjust_interval);
    pthread_mutex_unlock(&pool->lock);
    return 0;
}

int thread_pool_destroy(thread_pool_t pool)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_destroy: 尝试销毁 NULL 池");
        return -1;
    }

    // 如果自动调整已启用，则先禁用它
    // 这将确保自动调整线程被正确停止和清理
    if (pool->auto_adjust) {
        TPOOL_LOG("线程池 %p 销毁：自动调整已启用，正在禁用...", (void *)pool);
        // 确保禁用自动调整，多次尝试如果需要
        int disable_attempts = 0;
        int max_disable_attempts = 3;
        int disable_result = -1;
        
        while (disable_attempts < max_disable_attempts && disable_result != 0) {
            disable_result = thread_pool_disable_auto_adjust(pool);
            if (disable_result != 0) {
                // 短暂等待后重试
                const struct timespec retry_wait = {0, 100000000}; // 100ms
                nanosleep(&retry_wait, NULL);
                disable_attempts++;
                TPOOL_ERROR("线程池 %p 销毁：禁用自动调整失败，尝试 %d/%d", 
                          (void *)pool, disable_attempts, max_disable_attempts);
            }
        }
        
        if (disable_result != 0) {
            // 即使多次尝试禁用失败，也尝试继续销毁池的其余部分，但记录错误
            TPOOL_ERROR("线程池 %p 销毁：多次尝试禁用自动调整失败，但仍继续销毁。", (void *)pool);
        }
    }

    TPOOL_LOG("正在销毁线程池 %p。", (void *)pool);

    // 如果已经关闭，直接返回
    if (pool->shutdown) {
        TPOOL_DEBUG("thread_pool_destroy: 线程池 %p 已经关闭。", (void *)pool);
        return 0;
    }

    // 再次确认自动调整功能已禁用
    pthread_mutex_lock(&pool->resize_lock);
    if (pool->auto_adjust) {
        pool->auto_adjust = 0;
        TPOOL_LOG("线程池 %p 销毁过程中再次禁用自动调整功能。", (void *)pool);
        // 直接唤醒自动调整线程
        if (pool->adjust_thread != 0) {
            pthread_cond_broadcast(&pool->adjust_cond);
            TPOOL_LOG("线程池 %p 向自动调整线程发送广播信号。", (void *)pool);
        }
    }
    pthread_mutex_unlock(&pool->resize_lock);

    // 首先重置空闲线程计数，确保一致性
    pthread_mutex_lock(&pool->lock);
    int calculated_idle_threads = 0;
    for (int i = 0; i < pool->thread_count; i++) {
        if (pool->thread_status[i] == 0) { // 空闲状态
            calculated_idle_threads++;
        }
    }
    if (calculated_idle_threads != pool->idle_threads) {
        TPOOL_LOG("thread_pool_destroy: 修正空闲线程计数 %d -> %d", pool->idle_threads,
                  calculated_idle_threads);
        pool->idle_threads = calculated_idle_threads;
    }

    // 标记线程池为关闭状态
    pool->shutdown = 1;
    TPOOL_DEBUG("thread_pool_destroy: 线程池 %p 已标记为关闭。正在向所有工作线程广播。",
              (void *)pool);

    // 广播给所有等待的线程
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // 等待一小段时间，让线程有机会响应关闭信号
    const struct timespec wait_time = {0, 100000000}; // 100ms
    nanosleep(&wait_time, NULL);

    // 再次广播，确保所有线程都收到信号
    pthread_mutex_lock(&pool->lock);
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // 再次广播给自动调整线程，确保它退出
    pthread_mutex_lock(&pool->resize_lock);
    pthread_cond_broadcast(&pool->adjust_cond);
    pthread_mutex_unlock(&pool->resize_lock);

    // 等待短暂时间，给线程机会退出
    struct timespec short_wait = {0, 200000000}; // 200ms
    nanosleep(&short_wait, NULL);

    // 再次广播
    pthread_mutex_lock(&pool->lock);
    pthread_cond_broadcast(&pool->notify);
    pthread_mutex_unlock(&pool->lock);

    // 如果存在自动调整线程，先处理它
    if (pool->adjust_thread != 0) {
        TPOOL_DEBUG("thread_pool_destroy: 正在连接自动调整线程 (ID: %lu)", 
                 (unsigned long)pool->adjust_thread);

        // 使用超时机制来连接自动调整线程
        struct timespec join_timeout = {0, 0};
        clock_gettime(CLOCK_REALTIME, &join_timeout);
        join_timeout.tv_sec += 1; // 1秒超时

        // 最后一次尝试通知自动调整线程退出
        pthread_mutex_lock(&pool->resize_lock);
        pthread_cond_broadcast(&pool->adjust_cond);
        pthread_mutex_unlock(&pool->resize_lock);

        // 尝试连接自动调整线程
        int join_result = pthread_join(pool->adjust_thread, NULL);
        if (join_result == 0) {
            TPOOL_DEBUG("thread_pool_destroy: 自动调整线程已成功连接。");
        } else {
            TPOOL_ERROR("thread_pool_destroy: 连接自动调整线程失败: %s", 
                       strerror(join_result));
            // 尝试取消线程
            pthread_cancel(pool->adjust_thread);
            join_result = pthread_join(pool->adjust_thread, NULL);
            if (join_result == 0) {
                TPOOL_DEBUG("thread_pool_destroy: 自动调整线程在取消后成功连接。");
            } else {
                TPOOL_ERROR("thread_pool_destroy: 无法在取消后连接自动调整线程: %s", 
                           strerror(join_result));
            }
        }
        pool->adjust_thread = 0;
    }

    // 线程连接处理

    for (int i = 0; i < pool->thread_count; i++) {
        if (pool->threads[i] == 0) {
            continue; // 跳过无效线程ID
        }

        TPOOL_DEBUG("thread_pool_destroy: 正在连接线程池 %p 的线程 #%d (ID: %lu)。", (void *)pool, i,
                  (unsigned long)pool->threads[i]);

        // 创建一个单独的线程来处理连接
        pthread_t thread_id = pool->threads[i];
        int join_attempted = 0;

        // 在连接前再次发送广播信号
        pthread_mutex_lock(&pool->lock);
        pthread_cond_broadcast(&pool->notify);
        pthread_mutex_unlock(&pool->lock);

        // 使用超时机制尝试连接
        struct timespec join_start_time;
        clock_gettime(CLOCK_MONOTONIC, &join_start_time);
        struct timespec current_time;
        
        // 设置超时时间为500ms
        struct timespec end_time = join_start_time;
        end_time.tv_nsec += 500000000; // 500ms
        if (end_time.tv_nsec >= 1000000000) {
            end_time.tv_sec += 1;
            end_time.tv_nsec -= 1000000000;
        }

        // 尝试使用标准pthread_join
        int join_result = pthread_join(thread_id, NULL);

        if (join_result == 0) {
            // 成功连接
            join_attempted = 1;
            TPOOL_DEBUG("thread_pool_destroy: 线程 #%d (ID: %lu) 已成功连接。", i,
                      (unsigned long)thread_id);
        } else {
            // 连接失败，可能是线程已经分离或其他原因
            TPOOL_ERROR("thread_pool_destroy: 连接线程 #%d (ID: %lu) 失败: %s", i,
                        (unsigned long)thread_id, strerror(join_result));
            
            // 检查是否超时
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            if ((current_time.tv_sec > end_time.tv_sec) || 
                (current_time.tv_sec == end_time.tv_sec && current_time.tv_nsec > end_time.tv_nsec)) {
                TPOOL_ERROR("thread_pool_destroy: 连接线程 #%d 超时", i);
            }
        }

        // 如果连接失败，尝试取消线程
        if (!join_attempted) {
            TPOOL_ERROR("thread_pool_destroy: 无法连接线程 #%d (ID: %lu), 尝试取消。", i,
                        (unsigned long)thread_id);

            // 尝试取消线程
            pthread_cancel(thread_id);

            // 再次尝试连接，设置短超时
            join_result = pthread_join(thread_id, NULL);
            if (join_result == 0) {
                TPOOL_DEBUG("thread_pool_destroy: 线程 #%d (ID: %lu) 在取消后成功连接。", i,
                          (unsigned long)thread_id);
            } else {
                TPOOL_ERROR("thread_pool_destroy: 无法在取消后连接线程 #%d (ID: %lu): %s", i,
                            (unsigned long)thread_id, strerror(join_result));
            }
        }

        // 清除线程 ID，避免重复连接
        pool->threads[i] = 0;
    }

    // 销毁任务队列
    task_queue_destroy_internal(pool);

    // 销毁互斥锁和条件变量
    pthread_mutex_destroy(&pool->lock);
    pthread_mutex_destroy(&pool->resize_lock);
    pthread_cond_destroy(&pool->notify);

    // 释放 running_task_names 数组及其字符串
    for (int i = 0; i < pool->thread_count; ++i) {
        if (pool->running_task_names[i]) {
            free(pool->running_task_names[i]);
        }
    }
    free(pool->running_task_names);
    TPOOL_DEBUG("已清理线程池 %p 的 running_task_names。", (void *)pool);

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
        TPOOL_ERROR("thread_pool_resize: pool is NULL");
        return -1;
    }

    // 使用 resize_lock 来串行化 resize 操作本身，防止并发 resize
    pthread_mutex_lock(&(pool->resize_lock));

    // 在 resize_lock 保护下读取 min/max threads 是安全的
    if (new_thread_count < pool->min_threads || new_thread_count > pool->max_threads) {
        TPOOL_ERROR(
            "thread_pool_resize: new_thread_count (%d) is out of range [%d, %d] for pool %p",
            new_thread_count, pool->min_threads, pool->max_threads, (void *)pool);
        pthread_mutex_unlock(&(pool->resize_lock));
        return -1;
    }

    // 获取主锁以安全地访问和修改池的共享状态
    pthread_mutex_lock(&(pool->lock));

    if (pool->shutdown) {
        TPOOL_ERROR("thread_pool_resize: pool %p is shutting down", (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        pthread_mutex_unlock(&(pool->resize_lock));
        return -1;
    }

    int old_thread_count = pool->thread_count;
    TPOOL_DEBUG("Resizing thread pool %p from %d to %d threads.", (void *)pool, old_thread_count,
              new_thread_count);

    if (new_thread_count == old_thread_count) {
        TPOOL_DEBUG("thread_pool_resize: new_thread_count is the same as current for pool %p, no "
                  "action needed.",
                  (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        pthread_mutex_unlock(&(pool->resize_lock));
        return 0;
    }

    if (new_thread_count > old_thread_count) { // 增加线程
        // 分别为线程数组分配新内存，避免 realloc 失败导致的不一致状态
        pthread_t *new_threads_ptr = (pthread_t *)malloc(new_thread_count * sizeof(pthread_t));
        char **new_running_task_names_ptr = (char **)malloc(new_thread_count * sizeof(char *));
        int *new_thread_status_ptr = (int *)malloc(new_thread_count * sizeof(int));

        if (new_threads_ptr == NULL || new_running_task_names_ptr == NULL ||
            new_thread_status_ptr == NULL) {
            TPOOL_ERROR(
                "thread_pool_resize: Failed to allocate memory for thread arrays for pool %p",
                (void *)pool);
            // 清理所有已分配的内存
            if (new_threads_ptr != NULL) {
                free(new_threads_ptr);
            }
            if (new_running_task_names_ptr != NULL) {
                free(new_running_task_names_ptr);
            }
            if (new_thread_status_ptr != NULL) {
                free(new_thread_status_ptr);
            }
            pthread_mutex_unlock(&(pool->lock));
            pthread_mutex_unlock(&(pool->resize_lock));
            return -1;
        }
        
        // 复制现有数组数据到新数组
        memcpy(new_threads_ptr, pool->threads, old_thread_count * sizeof(pthread_t));
        memcpy(new_running_task_names_ptr, pool->running_task_names, old_thread_count * sizeof(char *));
        memcpy(new_thread_status_ptr, pool->thread_status, old_thread_count * sizeof(int));
        // 释放旧数组并更新指针
        free(pool->threads);
        free(pool->running_task_names);
        free(pool->thread_status);
        
        pool->threads = new_threads_ptr;
        pool->running_task_names = new_running_task_names_ptr;
        pool->thread_status = new_thread_status_ptr;

        for (int i = old_thread_count; i < new_thread_count; ++i) {
            pool->running_task_names[i] = (char *)malloc(MAX_TASK_NAME_LEN);
            if (pool->running_task_names[i] == NULL) {
                TPOOL_ERROR("thread_pool_resize: Failed to allocate memory for task name for new "
                            "thread %d in pool %p",
                            i, (void *)pool);
                // 回滚：释放已分配的，并将数组大小调回 old_thread_count
                for (int k = old_thread_count; k < i; ++k) {
                    free(pool->running_task_names[k]);
                }
                // 此处不尝试 realloc 缩小数组，因为可能再次失败。标记池为错误状态可能更好。
                pthread_mutex_unlock(&(pool->lock));
                pthread_mutex_unlock(&(pool->resize_lock));
                return -1;
            }
            snprintf(pool->running_task_names[i], MAX_TASK_NAME_LEN, "[idle]");
            pool->thread_status[i] = 0; // 0 for idle

            thread_args_t *args = (thread_args_t *)malloc(sizeof(thread_args_t));
            if (!args) {
                TPOOL_ERROR("thread_pool_resize: Failed to allocate memory for thread_args_t for "
                            "thread %d in pool %p",
                            i, (void *)pool);
                free(pool->running_task_names[i]);
                pthread_mutex_unlock(&(pool->lock));
                pthread_mutex_unlock(&(pool->resize_lock));
                return -1;
            }
            args->pool = pool;
            args->thread_id = i;

            if (pthread_create(&(pool->threads[i]), NULL, worker_thread_function, (void *)args) !=
                0) {
                TPOOL_ERROR("thread_pool_resize: Failed to create new thread %d for pool %p: %s", i,
                            (void *)pool, strerror(errno));
                // 清理已分配的资源
                free(pool->running_task_names[i]);
                free(args);
                
                // 回滚：释放先前创建的线程
                for (int k = old_thread_count; k < i; ++k) {
                    // 标记这些线程需要退出
                    pool->thread_status[k] = -2; // -2 表示因为调整大小而退出
                }
                // 广播通知线程退出
                pthread_cond_broadcast(&(pool->notify));
                
                // 更新线程计数为实际成功创建的线程数
                pool->thread_count = old_thread_count + (i - old_thread_count);
                
                pthread_mutex_unlock(&(pool->lock));
                pthread_mutex_unlock(&(pool->resize_lock));
                return -1;
            }
            TPOOL_DEBUG("Thread %d (ID: %lu) created successfully for pool %p.", i,
                      (unsigned long)pool->threads[i], (void *)pool);
            pool->idle_threads++;
            pool->started++;
        }
        pool->resize_shutdown = 0; // 确保增加线程时，缩减标志是关闭的
    } else {                       // 减少线程 (new_thread_count < old_thread_count)
        pool->resize_shutdown = 1; // 标记有线程需要因为缩小而退出
        // 工作线程会在其循环中检查 args->thread_id >= new_thread_count (在 resize 后是
        // pool->thread_count) 并自行退出。主锁已持有。
        TPOOL_LOG(
            "Thread pool %p: resize_shutdown initiated. Target count %d. Broadcasting to workers.",
            (void *)pool, new_thread_count);
        pthread_cond_broadcast(&(pool->notify)); // 唤醒所有线程检查状态
    }

    pool->thread_count = new_thread_count; // 更新逻辑线程计数

    pthread_mutex_unlock(&(pool->lock));
    pthread_mutex_unlock(&(pool->resize_lock));

    TPOOL_DEBUG("Thread pool %p successfully resized to %d threads (logical).", (void *)pool,
              new_thread_count);
    return 0;
}

/**
 * @brief 检索由工作线程当前执行的任务名称的副本。
 *
 * 调用者负责使用 `free_running_task_names()` 释放返回的数组及其中的每个字符串。
 * 返回的数组将以 NULL 结尾，因此调用者可以迭代直到 NULL 或使用获取时的线程数作为计数。
 *
 * @param pool 指向 thread_pool_t 实例的指针。
 * @return 一个动态分配的字符串数组 (char **)。此数组的大小
 *         等于池中的线程数，每个字符串包含相应线程的任务名称，
 *         或 "[idle]" 如果线程空闲。数组以 NULL 指针结束。
 *         错误时返回 NULL (例如，pool 为 NULL，内存分配失败)。
 */
char **thread_pool_get_running_task_names(thread_pool_t pool)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_get_running_task_names: pool is NULL");
        return NULL;
    }

    pthread_mutex_lock(&(pool->lock));

    if (pool->shutdown) {
        TPOOL_LOG("thread_pool_get_running_task_names: pool %p is shutdown. Returning NULL.",
                  (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        return NULL;
    }

    int current_thread_count = pool->thread_count;
    if (current_thread_count < 0) {
        current_thread_count = 0; // 防御性检查
    }

    char **task_names_copy = (char **)malloc((current_thread_count + 1) * sizeof(char *));
    if (task_names_copy == NULL) {
        TPOOL_ERROR("thread_pool_get_running_task_names: Failed to allocate memory for task names "
                    "array copy for pool %p",
                    (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        return NULL;
    }

    for (int i = 0; i < current_thread_count; ++i) {
        // 为每个任务名分配内存
        task_names_copy[i] = (char *)malloc(MAX_TASK_NAME_LEN);
        if (task_names_copy[i] == NULL) {
            TPOOL_ERROR("thread_pool_get_running_task_names: Failed to allocate memory for "
                        "task name string copy (index %d) for pool %p",
                        i, (void *)pool);
            for (int j = 0; j < i; ++j) {
                free(task_names_copy[j]);
            }
            free(task_names_copy);
            pthread_mutex_unlock(&(pool->lock));
            return NULL;
        }
        
        // 检查线程是否正在执行任务
        if (pool->running_task_names != NULL && i < pool->thread_count && 
            pool->running_task_names[i] != NULL &&
            pool->thread_status != NULL && pool->thread_status[i] > 0) {
            // 线程正在执行任务，复制任务名
            strncpy(task_names_copy[i], pool->running_task_names[i], MAX_TASK_NAME_LEN - 1);
            task_names_copy[i][MAX_TASK_NAME_LEN - 1] = '\0';
        } else {
            // 检查线程状态并设置适当的状态标记
            if (pool->thread_status != NULL && i < pool->thread_count) {
                if (pool->thread_status[i] == -2) {
                    snprintf(task_names_copy[i], MAX_TASK_NAME_LEN, "[exiting_resize]");
                } else if (pool->thread_status[i] == -1) {
                    snprintf(task_names_copy[i], MAX_TASK_NAME_LEN, "[exiting_shutdown]");
                } else {
                    snprintf(task_names_copy[i], MAX_TASK_NAME_LEN, "[idle]");
                }
            } else {
                // 无效状态或数组未初始化
                snprintf(task_names_copy[i], MAX_TASK_NAME_LEN, "[unknown]");
            }
        }
    }
    task_names_copy[current_thread_count] = NULL; // NULL 终止数组

    pthread_mutex_unlock(&(pool->lock));
    return task_names_copy;
}

/**
 * @brief 释放由 `thread_pool_get_running_task_names` 返回的任务名称数组。
 *
 * @param task_names 要释放的字符串数组 (char **).
 * @param count 数组中的字符串数量 (应与调用 `thread_pool_get_running_task_names` 时
 *              池的 thread_count 相匹配)。如果 task_names 是 NULL 结尾的，count 可以被忽略。
 */
void free_running_task_names(char **task_names, int count)
{
    if (task_names == NULL) {
        return;
    }
    // 使用 count 参数，因为头文件中有它。或者依赖 NULL 终止符。
    // 如果依赖NULL终止符： for (int i = 0; task_names[i] != NULL; ++i) free(task_names[i]);
    if (count < 0) { // 如果 count 无效，尝试NULL终止符
        for (int i = 0; task_names[i] != NULL; ++i) {
            free(task_names[i]);
        }
    } else {
        for (int i = 0; i < count; ++i) {
            if (task_names[i] != NULL) {
                free(task_names[i]);
            }
        }
    }
    free(task_names);
}

int thread_pool_set_limits(thread_pool_t pool, int min_threads, int max_threads)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_set_limits: 池为 NULL。");
        return -1;
    }

    if (min_threads <= 0 || max_threads < min_threads) {
        TPOOL_ERROR("thread_pool_set_limits: 无效的线程数量范围 [%d, %d]。", min_threads,
                    max_threads);
        return -1;
    }

    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_ERROR("thread_pool_set_limits: 池正在关闭，不能设置限制。");
        return -1;
    }

    pool->min_threads = min_threads;
    pool->max_threads = max_threads;

    TPOOL_LOG("thread_pool_set_limits: 线程池 %p 的线程数量范围已更新为 [%d, %d]。", (void *)pool,
              pool->min_threads, pool->max_threads);

    // 检查当前线程数是否在新限制之外，如果是，则调整
    // 注意：这里我们直接调用 thread_pool_resize，它内部会处理锁和边界检查
    // 我们需要在解锁 pool->lock 之后调用，因为 resize 自己会加锁
    int current_thread_count = pool->thread_count;
    int needs_adjust = 0;
    int target_threads = current_thread_count;

    if (current_thread_count < pool->min_threads) {
        target_threads = pool->min_threads;
        needs_adjust = 1;
        TPOOL_LOG("thread_pool_set_limits: 当前线程数 %d 小于新下限 %d，计划增加线程。",
                  current_thread_count, pool->min_threads);
    } else if (current_thread_count > pool->max_threads) {
        target_threads = pool->max_threads;
        needs_adjust = 1;
        TPOOL_LOG("thread_pool_set_limits: 当前线程数 %d 大于新上限 %d，计划减少线程。",
                  current_thread_count, pool->max_threads);
    }
    pthread_mutex_unlock(&(pool->lock));

    if (needs_adjust) {
        // thread_pool_resize 内部会处理锁和错误
        if (thread_pool_resize(pool, target_threads) != 0) {
            TPOOL_ERROR("thread_pool_set_limits: 调整线程池大小到 %d 失败。", target_threads);
            // 即使调整失败，限制也已设置，但池可能处于非最佳状态
            return -1; // 或者返回0，表示限制已设置但调整有问题？当前返回-1表示操作未完全成功
        }
    }

    TPOOL_LOG("thread_pool_set_limits: 成功设置线程池 %p 的线程数量范围为 [%d, %d]。", (void *)pool,
              pool->min_threads, pool->max_threads);
    return 0;
}

int thread_pool_get_stats(thread_pool_t pool, thread_pool_stats_t *stats)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_get_stats: 池为 NULL。");
        return -1;
    }
    if (stats == NULL) {
        TPOOL_ERROR("thread_pool_get_stats: stats 指针为 NULL。");
        return -1;
    }

    pthread_mutex_lock(&(pool->lock));
    if (pool->shutdown) { // 检查池是否正在关闭
        pthread_mutex_unlock(&(pool->lock));
        TPOOL_ERROR("thread_pool_get_stats: 池正在关闭或已销毁。");
        // 可以选择清零stats或返回错误
        memset(stats, 0, sizeof(thread_pool_stats_t));
        return -1;
    }

    stats->thread_count = pool->thread_count;
    stats->min_threads = pool->min_threads;
    stats->max_threads = pool->max_threads;
    stats->idle_threads = pool->idle_threads;
    stats->task_queue_size = pool->task_queue_size;
    stats->started = pool->started; // 使用 thread_pool_s 中的 'started' 成员

    pthread_mutex_unlock(&(pool->lock));

    TPOOL_DEBUG("thread_pool_get_stats: 线程池 %p: 总线程=%d, 最小=%d, 最大=%d, 空闲=%d, 队列=%d, "
              "已启动=%d",
              (void *)pool, stats->thread_count, stats->min_threads, stats->max_threads,
              stats->idle_threads, stats->task_queue_size, stats->started);
    return 0;
}

// 确保这个函数定义在 thread_pool_destroy 之前，或者 thread_pool_destroy 在它之后
int thread_pool_disable_auto_adjust(thread_pool_t pool)
{
    if (pool == NULL) {
        TPOOL_ERROR("thread_pool_disable_auto_adjust: pool 为 NULL");
        return -1;
    }

    // 注意：这里对 pool->lock 的加锁/解锁是为了保护对 auto_adjust 标志的访问
    // 以及确保在检查 adjust_thread_running 时的原子性。
    // adjust_cond_lock 用于保护 adjust_cond。

    pthread_mutex_lock(&(pool->lock));
    if (!pool->auto_adjust) {
        TPOOL_LOG("thread_pool_disable_auto_adjust: 线程池 %p 的自动调整功能尚未启用。",
                  (void *)pool);
        pthread_mutex_unlock(&(pool->lock));
        return 0;
    }

    // 检查调整线程是否实际在运行
    // adjust_thread_running 应该在 adjust_cond_lock 的保护下被 auto_adjust_thread_function 修改
    // 但在这里，我们主要通过 auto_adjust 标志来判断是否“应该”有调整线程
    // 并且在 enable 时，线程的创建和 adjust_thread_running 的设置是一起的。
    // 如果 adjust_thread 成员为0（或其他无效值），也表示线程未运行或已清理。
    // 为简单起见，我们依赖 adjust_thread_running，它在 enable 时设置，在 disable 时清除。

    if (pool->adjust_thread_running) {
        TPOOL_DEBUG("线程池 %p 正在停止自动调整线程...", (void *)pool);
        pool->adjust_thread_running = 0; // 请求线程停止

        // 必须在解锁 pool->lock 之后再操作 adjust_cond_lock 和 join，以避免死锁
        // 因为 auto_adjust_thread_function 可能会尝试获取 pool->lock
        pthread_mutex_unlock(&(pool->lock)); // 尽早释放主锁

        // 发送多个信号，确保自动调整线程能够唤醒并注意到退出标志
        pthread_mutex_lock(&pool->adjust_cond_lock);
        pthread_cond_broadcast(&pool->adjust_cond); // 使用broadcast而非signal，确保所有等待的线程都能被唤醒
        pthread_mutex_unlock(&pool->adjust_cond_lock);
        
        // 等待短暂停，给线程时间响应
        usleep(10000); // 10ms
        
        // 再次发送信号，以防线程处于正在等待状态
        pthread_mutex_lock(&pool->adjust_cond_lock);
        pthread_cond_broadcast(&pool->adjust_cond);
        pthread_mutex_unlock(&pool->adjust_cond_lock);

        // 设置超时时间，避免无限期等待
        struct timespec join_timeout;
        clock_gettime(CLOCK_REALTIME, &join_timeout);
        join_timeout.tv_sec += 2; // 2秒超时
        
        int join_result = 0;
        #ifdef PTHREAD_TIMEDJOIN_NP
        join_result = pthread_timedjoin_np(pool->adjust_thread, NULL, &join_timeout);
        #else
        // 如果不支持超时join，使用普通join
        join_result = pthread_join(pool->adjust_thread, NULL);
        #endif
        
        if (join_result != 0) {
            // 如果无法正常join，尝试取消线程
            TPOOL_ERROR("thread_pool_disable_auto_adjust: 等待自动调整线程 %lu 退出失败: %s",
                        (unsigned long)pool->adjust_thread, strerror(errno));
            #ifdef PTHREAD_CANCEL
            pthread_cancel(pool->adjust_thread);
            usleep(10000); // 给取消一些时间生效
            #endif
            // 即使 join 失败，也尝试清理资源
        } else {
            TPOOL_DEBUG("自动调整线程 (线程池 %p) 已成功 join。", (void *)pool);
        }

        // 清理条件变量和锁
        if (pthread_cond_destroy(&pool->adjust_cond) != 0) {
            TPOOL_ERROR("thread_pool_disable_auto_adjust: 销毁 adjust_cond 失败: %s",
                        strerror(errno));
        }
        if (pthread_mutex_destroy(&pool->adjust_cond_lock) != 0) {
            TPOOL_ERROR("thread_pool_disable_auto_adjust: 销毁 adjust_cond_lock 失败: %s",
                        strerror(errno));
        }
        pool->adjust_thread = 0; // 标记线程已处理

        pthread_mutex_lock(&(pool->lock)); // 重新获取锁以更新 auto_adjust
    } else {
        TPOOL_LOG("线程池 %p：自动调整已标记为启用，但调整线程似乎未在运行。", (void *)pool);
        // 可能是在 enable 过程中失败了，或者状态不一致
    }

    pool->auto_adjust = 0; // 正式标记为禁用
    TPOOL_LOG("线程池 %p 已禁用自动调整。", (void *)pool);
    pthread_mutex_unlock(&(pool->lock));
    return 0;
}
