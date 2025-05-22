/**
 * @file log_advanced_example.c
 * @brief 日志模块高级功能示例
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "log.h"

// 自定义日志回调函数
void my_log_callback(log_level_t level, log_module_t module, 
                    const char *file, int line, const char *func, 
                    const char *message, void *user_data)
{
    // 为了演示，我们只使用日志级别和消息内容
    // 忽略其他参数: module, file, line, func, user_data
    (void)module;
    (void)file;
    (void)line;
    (void)func;
    (void)user_data;
    
    printf("回调收到日志: [%s] %s\n", log_level_names[level], message);
}

// 线程函数
void *thread_function(void *arg)
{
    int thread_id = *(int*)arg;
    
    // 设置线程本地日志上下文
    log_context_t context = {0};
    char context_id[32];
    snprintf(context_id, sizeof(context_id), "Thread-%d", thread_id);
    context.context_id = context_id;
    context.session_id = "SESSION-123";
    context.user_id = "USER-456";
    
    log_set_context(&context);
    
    // 使用上下文记录日志
    LOG_INFO(LOG_MODULE_THREAD, "线程 %d 正在执行任务", thread_id);
    sleep(1);
    LOG_DEBUG(LOG_MODULE_THREAD, "线程 %d 任务完成", thread_id);
    
    return NULL;
}

int main(void)
{
    // 初始化日志系统
    int ret = log_init("log_advanced_example.log", LOG_LEVEL_DEBUG);
    if (ret != 0) {
        fprintf(stderr, "日志系统初始化失败\n");
        return -1;
    }
    
    // 配置日志格式选项
    log_format_options_t format_options;
    log_get_format_options(&format_options);
    format_options.show_time = true;
    format_options.show_tid = true;
    format_options.show_module = true;
    format_options.show_file_line = true;
    format_options.show_function = true;
    format_options.use_colors = true;
    log_set_format_options(&format_options);
    
    // 配置日志轮转
    log_rotation_config_t rotation_config = {
        .max_file_size = 1024 * 1024, // 1MB
        .max_file_count = 3,
        .rotate_on_size = true,
        .rotate_on_time = false,
        .rotate_interval_hours = 24
    };
    log_set_rotation_config(&rotation_config);
    
    // 注册日志回调
    log_register_callback(my_log_callback, NULL);
    
    LOG_INFO(LOG_MODULE_CORE, "高级日志示例开始");
    
    // 创建多个线程，演示线程本地上下文
    pthread_t threads[3];
    int thread_ids[3] = {1, 2, 3};
    
    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 手动触发日志轮转
    LOG_INFO(LOG_MODULE_CORE, "手动触发日志轮转");
    log_rotate_now();
    
    // 生成大量日志，测试轮转
    LOG_INFO(LOG_MODULE_CORE, "生成大量日志测试轮转");
    for (int i = 0; i < 1000; i++) {
        LOG_DEBUG(LOG_MODULE_CORE, "这是第 %d 条测试日志消息", i);
    }
    
    LOG_INFO(LOG_MODULE_CORE, "高级日志示例结束");
    
    // 关闭日志系统
    log_deinit();
    printf("高级日志示例完成，请查看 log_advanced_example.log 文件及轮转文件\n");
    
    return 0;
}
