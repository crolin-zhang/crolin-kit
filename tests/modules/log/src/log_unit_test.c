/**
 * @file log_unit_test.c
 * @brief 日志模块单元测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include "log.h"

// 测试日志文件路径
#define TEST_LOG_FILE "log_unit_test.log"

// 测试回调标志
static int callback_called = 0;
static log_level_t callback_level = 0;
static char callback_message[256] = {0};

// 测试回调函数
void test_log_callback(log_level_t level, log_module_t module, 
                      const char *file, int line, const char *func, 
                      const char *message, void *user_data)
{
    (void)module;
    (void)file;
    (void)line;
    (void)func;
    (void)user_data;
    
    callback_called = 1;
    callback_level = level;
    strncpy(callback_message, message, sizeof(callback_message) - 1);
}

// 检查文件是否存在
int file_exists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// 测试日志初始化和关闭
void test_log_init_deinit(void)
{
    printf("测试日志初始化和关闭...\n");
    
    // 删除可能存在的旧日志文件
    unlink(TEST_LOG_FILE);
    
    // 测试初始化
    int ret = log_init(TEST_LOG_FILE, LOG_LEVEL_INFO);
    assert(ret == 0);
    
    // 写入一条日志
    LOG_INFO(LOG_MODULE_CORE, "测试日志初始化");
    
    // 关闭日志
    log_deinit();
    
    // 验证日志文件已创建
    assert(file_exists(TEST_LOG_FILE));
    
    printf("测试通过!\n");
}

// 测试不同日志级别
void test_log_levels(void)
{
    printf("测试日志级别...\n");
    
    log_init(TEST_LOG_FILE, LOG_LEVEL_INFO);
    
    // 设置不同模块的日志级别
    log_set_module_level(LOG_MODULE_CORE, LOG_LEVEL_DEBUG);
    log_set_module_level(LOG_MODULE_THREAD, LOG_LEVEL_ERROR);
    
    // 验证日志级别设置
    assert(log_get_module_level(LOG_MODULE_CORE) == LOG_LEVEL_DEBUG);
    assert(log_get_module_level(LOG_MODULE_THREAD) == LOG_LEVEL_ERROR);
    
    // 验证日志级别过滤
    assert(log_is_level_enabled(LOG_MODULE_CORE, LOG_LEVEL_DEBUG) == true);
    assert(log_is_level_enabled(LOG_MODULE_CORE, LOG_LEVEL_TRACE) == false);
    assert(log_is_level_enabled(LOG_MODULE_THREAD, LOG_LEVEL_ERROR) == true);
    assert(log_is_level_enabled(LOG_MODULE_THREAD, LOG_LEVEL_INFO) == false);
    
    log_deinit();
    printf("测试通过!\n");
}

// 测试日志回调
void test_log_callback_func(void)
{
    printf("测试日志回调...\n");
    
    log_init(TEST_LOG_FILE, LOG_LEVEL_INFO);
    
    // 注册回调
    callback_called = 0;
    memset(callback_message, 0, sizeof(callback_message));
    
    int ret = log_register_callback(test_log_callback, NULL);
    assert(ret == 0);
    
    // 写入日志，触发回调
    LOG_ERROR(LOG_MODULE_CORE, "测试回调消息");
    
    // 验证回调被调用
    assert(callback_called == 1);
    assert(callback_level == LOG_LEVEL_ERROR);
    assert(strstr(callback_message, "测试回调消息") != NULL);
    
    // 注销回调
    ret = log_unregister_callback(test_log_callback);
    assert(ret == 0);
    
    // 重置回调标志
    callback_called = 0;
    memset(callback_message, 0, sizeof(callback_message));
    
    // 再次写入日志，回调不应被触发
    LOG_ERROR(LOG_MODULE_CORE, "回调已注销");
    assert(callback_called == 0);
    
    log_deinit();
    printf("测试通过!\n");
}

// 测试日志格式选项
void test_log_format_options(void)
{
    printf("测试日志格式选项...\n");
    
    log_init(TEST_LOG_FILE, LOG_LEVEL_INFO);
    
    // 获取默认格式选项
    log_format_options_t options;
    log_get_format_options(&options);
    
    // 修改格式选项
    options.show_time = false;
    options.show_tid = false;
    log_set_format_options(&options);
    
    // 验证格式选项已更新
    log_format_options_t new_options;
    log_get_format_options(&new_options);
    assert(new_options.show_time == false);
    assert(new_options.show_tid == false);
    
    log_deinit();
    printf("测试通过!\n");
}

// 测试日志轮转配置
void test_log_rotation(void)
{
    printf("测试日志轮转配置...\n");
    
    log_init(TEST_LOG_FILE, LOG_LEVEL_INFO);
    
    // 设置轮转配置
    log_rotation_config_t config = {
        .max_file_size = 1024,  // 1KB，便于测试
        .max_file_count = 3,
        .rotate_on_size = true,
        .rotate_on_time = false,
        .rotate_interval_hours = 24
    };
    log_set_rotation_config(&config);
    
    // 验证轮转配置已更新
    log_rotation_config_t new_config;
    log_get_rotation_config(&new_config);
    assert(new_config.max_file_size == 1024);
    assert(new_config.max_file_count == 3);
    
    // 手动触发轮转
    int ret = log_rotate_now();
    assert(ret == 0);
    
    log_deinit();
    printf("测试通过!\n");
}

// 主函数
int main(void)
{
    printf("开始日志模块单元测试...\n\n");
    
    // 运行测试用例
    test_log_init_deinit();
    test_log_levels();
    test_log_callback_func();
    test_log_format_options();
    test_log_rotation();
    
    printf("\n所有测试通过!\n");
    return 0;
}
