/**
 * @file log.c
 * @brief CrolinKit 日志模块实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include "log.h"

// 日志级别名称
const char *log_level_names[] = {
    "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
};

// 日志模块名称
static const char *module_names[] = {
    "CORE", "THREAD", "LOG"
};

// 日志文件的最大大小（10MB）
#define MAX_LOG_FILE_SIZE (10 * 1024 * 1024)

// 默认日志轮转配置
static struct {
    size_t max_file_size;      // 单个日志文件最大大小（字节）
    int max_file_count;        // 最大日志文件数量
    bool rotate_on_size;       // 是否按大小轮转
    bool rotate_on_time;       // 是否按时间轮转
    int rotate_interval_hours; // 时间轮转间隔（小时）
    time_t last_rotate_time;   // 上次轮转时间
} g_log_rotation = {
    .max_file_size = MAX_LOG_FILE_SIZE,
    .max_file_count = 5,
    .rotate_on_size = true,
    .rotate_on_time = false,
    .rotate_interval_hours = 24,
    .last_rotate_time = 0
};

// 全局日志配置
static struct {
    FILE *log_file;                        // 日志文件指针
    char log_file_path[256];               // 日志文件路径
    pthread_mutex_t mutex;                 // 日志互斥锁
    module_log_config_t modules[LOG_MODULE_MAX]; // 模块配置
    log_format_options_t format;           // 格式选项
    bool initialized;                       // 是否已初始化
    
    // 回调函数
    struct {
        log_callback_t func;               // 回调函数
        void *user_data;                   // 用户数据
    } callbacks[10];                        // 最多支持10个回调
    int callback_count;                     // 回调函数数量
    
    // 线程本地存储
    pthread_key_t context_key;             // 上下文键
    bool context_key_initialized;          // 上下文键是否已初始化
} g_log_config = {0};

// 获取线程ID
static pid_t log_gettid(void) {
#if defined(__linux__)
    return syscall(SYS_gettid);
#else
    // 其他系统可能需要不同的实现
    return (pid_t)pthread_self();
#endif
}

// 释放线程本地存储的上下文
static void free_thread_context(void *ptr) {
    if (ptr) {
        log_context_t *context = (log_context_t *)ptr;
        free(context);
    }
}

// 初始化线程本地存储键
static void init_context_key(void) {
    if (!g_log_config.context_key_initialized) {
        pthread_key_create(&g_log_config.context_key, free_thread_context);
        g_log_config.context_key_initialized = true;
    }
}

// 前向声明
int log_rotate_now(void);

// 检查并轮转日志文件
static void check_log_file_rotate(void) {
    if (!g_log_config.log_file || !g_log_config.log_file_path[0]) {
        return;
    }
    
    struct stat st;
    if (stat(g_log_config.log_file_path, &st) != 0) {
        return;
    }
    
    bool need_rotate = false;
    
    // 检查文件大小
    if (g_log_rotation.rotate_on_size && st.st_size >= (off_t)g_log_rotation.max_file_size) {
        need_rotate = true;
    }
    
    // 检查时间
    time_t now = time(NULL);
    if (g_log_rotation.rotate_on_time && 
        (now - g_log_rotation.last_rotate_time) >= g_log_rotation.rotate_interval_hours * 3600) {
        need_rotate = true;
    }
    
    if (need_rotate) {
        log_rotate_now();
    }
}

// 获取日志模块名称
const char* log_get_module_name(log_module_t module) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        return module_names[module];
    }
    return "UNKNOWN";
}

// 初始化日志系统
int log_init(const char *log_file, log_level_t level) {
    if (g_log_config.initialized) {
        return 0;  // 已经初始化
    }
    
    pthread_mutex_init(&g_log_config.mutex, NULL);
    
    // 设置默认格式选项
    g_log_config.format.show_time = true;
    g_log_config.format.show_tid = true;
    g_log_config.format.show_module = true;
    g_log_config.format.show_file_line = true;
    g_log_config.format.show_function = true;
    g_log_config.format.use_colors = true;
    g_log_config.format.use_iso_time = true;
    strcpy(g_log_config.format.time_format, "%Y-%m-%d %H:%M:%S");
    
    // 初始化模块配置
    for (int i = 0; i < LOG_MODULE_MAX; i++) {
        g_log_config.modules[i].level = level;
        g_log_config.modules[i].console_output = true;
        g_log_config.modules[i].file_output = true;
        g_log_config.modules[i].enabled = true;
        g_log_config.modules[i].custom_file = NULL;
    }
    
    // 打开日志文件
    if (log_file) {
        g_log_config.log_file = fopen(log_file, "a");
        if (g_log_config.log_file) {
            strncpy(g_log_config.log_file_path, log_file, sizeof(g_log_config.log_file_path) - 1);
        } else {
            fprintf(stderr, "Failed to open log file: %s\n", log_file);
        }
    }
    
    // 初始化上下文键
    init_context_key();
    
    g_log_config.initialized = true;
    
    // 记录初始化日志
    LOG_INFO(LOG_MODULE_LOG, "日志系统初始化完成，默认级别: %s", log_level_names[level]);
    
    return 0;
}

// 设置模块的日志级别
void log_set_module_level(log_module_t module, log_level_t level) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        g_log_config.modules[module].level = level;
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 设置模块的输出目标
void log_set_module_output(log_module_t module, bool console_on, bool file_on) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        g_log_config.modules[module].console_output = console_on;
        g_log_config.modules[module].file_output = file_on;
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 设置模块是否启用
void log_set_module_enable(log_module_t module, bool enable) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        g_log_config.modules[module].enabled = enable;
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 获取模块的日志级别
log_level_t log_get_module_level(log_module_t module) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        log_level_t level = g_log_config.modules[module].level;
        pthread_mutex_unlock(&g_log_config.mutex);
        return level;
    }
    return LOG_LEVEL_INFO;  // 默认级别
}

// 获取模块是否启用
bool log_get_module_enable(log_module_t module) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        bool enabled = g_log_config.modules[module].enabled;
        pthread_mutex_unlock(&g_log_config.mutex);
        return enabled;
    }
    return false;
}

// 检查指定级别的日志是否会被记录
bool log_is_level_enabled(log_module_t module, log_level_t level) {
    if (module >= 0 && module < LOG_MODULE_MAX) {
        pthread_mutex_lock(&g_log_config.mutex);
        bool enabled = g_log_config.modules[module].enabled && 
                      (level <= g_log_config.modules[module].level);
        pthread_mutex_unlock(&g_log_config.mutex);
        return enabled;
    }
    return false;
}

// 设置日志格式选项
void log_set_format_options(const log_format_options_t *options) {
    if (options) {
        pthread_mutex_lock(&g_log_config.mutex);
        memcpy(&g_log_config.format, options, sizeof(log_format_options_t));
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 获取日志格式选项
void log_get_format_options(log_format_options_t *options) {
    if (options) {
        pthread_mutex_lock(&g_log_config.mutex);
        memcpy(options, &g_log_config.format, sizeof(log_format_options_t));
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 写入日志
void log_write(log_level_t level, log_module_t module, const char *file,
               int line, const char *func, const char *fmt, ...) {
    if (!g_log_config.initialized) {
        return;
    }
    
    if (!log_is_level_enabled(module, level)) {
        return;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    // 检查日志文件轮转
    check_log_file_rotate();
    
    // 获取当前时间
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    
    char time_str[32];
    strftime(time_str, sizeof(time_str), g_log_config.format.time_format, tm_info);
    
    // 格式化日志消息
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // 构建完整日志行
    char log_line[2048];
    int pos = 0;
    
    // 添加时间戳
    if (g_log_config.format.show_time) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s.%03ld] ", 
                       time_str, tv.tv_usec / 1000);
    }
    
    // 添加日志级别
    pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s] ", 
                   log_level_names[level]);
    
    // 添加线程ID
    if (g_log_config.format.show_tid) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[TID:%ld] ", 
                       (long)log_gettid());
    }
    
    // 添加模块名
    if (g_log_config.format.show_module) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s] ", 
                       log_get_module_name(module));
    }
    
    // 添加文件名和行号
    if (g_log_config.format.show_file_line) {
        // 提取文件名（不包括路径）
        const char *filename = strrchr(file, '/');
        filename = filename ? filename + 1 : file;
        
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s:%d] ", 
                       filename, line);
    }
    
    // 添加函数名
    if (g_log_config.format.show_function) {
        pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[%s] ", func);
    }
    
    // 添加上下文信息（如果有）
    log_context_t *context = NULL;
    if (g_log_config.context_key_initialized) {
        context = pthread_getspecific(g_log_config.context_key);
        if (context) {
            if (context->context_id) {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[CTX:%s] ", 
                               context->context_id);
            }
            if (context->session_id) {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[SID:%s] ", 
                               context->session_id);
            }
            if (context->user_id) {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[UID:%s] ", 
                               context->user_id);
            }
            if (context->transaction_id) {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos, "[TXN:%s] ", 
                               context->transaction_id);
            }
        }
    }
    
    // 添加日志消息
    snprintf(log_line + pos, sizeof(log_line) - pos, "%s", message);
    
    // 输出到控制台
    if (g_log_config.modules[module].console_output) {
        // 根据日志级别选择输出流
        FILE *out = (level <= LOG_LEVEL_ERROR) ? stderr : stdout;
        
        // 添加颜色（如果启用）
        if (g_log_config.format.use_colors) {
            const char *color_code = "";
            switch (level) {
                case LOG_LEVEL_FATAL: color_code = "\033[1;31m"; break; // 亮红色
                case LOG_LEVEL_ERROR: color_code = "\033[31m"; break;   // 红色
                case LOG_LEVEL_WARN:  color_code = "\033[33m"; break;   // 黄色
                case LOG_LEVEL_INFO:  color_code = "\033[32m"; break;   // 绿色
                case LOG_LEVEL_DEBUG: color_code = "\033[36m"; break;   // 青色
                case LOG_LEVEL_TRACE: color_code = "\033[37m"; break;   // 白色
                default: break;
            }
            fprintf(out, "%s%s\033[0m\n", color_code, log_line);
        } else {
            fprintf(out, "%s\n", log_line);
        }
        fflush(out);
    }
    
    // 输出到文件
    if (g_log_config.log_file && g_log_config.modules[module].file_output) {
        fprintf(g_log_config.log_file, "%s\n", log_line);
        fflush(g_log_config.log_file);
    }
    
    // 调用回调函数
    for (int i = 0; i < g_log_config.callback_count; i++) {
        if (g_log_config.callbacks[i].func) {
            g_log_config.callbacks[i].func(level, module, file, line, func, 
                                         message, g_log_config.callbacks[i].user_data);
        }
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

// 设置日志上下文
void log_set_context(const log_context_t *context) {
    if (!g_log_config.context_key_initialized) {
        init_context_key();
    }
    
    log_context_t *thread_context = pthread_getspecific(g_log_config.context_key);
    if (!thread_context) {
        thread_context = (log_context_t *)malloc(sizeof(log_context_t));
        if (!thread_context) {
            return;
        }
        memset(thread_context, 0, sizeof(log_context_t));
        pthread_setspecific(g_log_config.context_key, thread_context);
    }
    
    if (context) {
        // 复制上下文信息
        thread_context->context_id = context->context_id ? strdup(context->context_id) : NULL;
        thread_context->session_id = context->session_id ? strdup(context->session_id) : NULL;
        thread_context->user_id = context->user_id ? strdup(context->user_id) : NULL;
        thread_context->transaction_id = context->transaction_id ? strdup(context->transaction_id) : NULL;
    }
}

// 清除日志上下文
void log_clear_context(void) {
    if (!g_log_config.context_key_initialized) {
        return;
    }
    
    log_context_t *thread_context = pthread_getspecific(g_log_config.context_key);
    if (thread_context) {
        if (thread_context->context_id) free((void*)thread_context->context_id);
        if (thread_context->session_id) free((void*)thread_context->session_id);
        if (thread_context->user_id) free((void*)thread_context->user_id);
        if (thread_context->transaction_id) free((void*)thread_context->transaction_id);
        
        memset(thread_context, 0, sizeof(log_context_t));
    }
}

// 获取当前线程的日志上下文
log_context_t log_get_thread_context(void) {
    log_context_t empty_context = {0};
    
    if (!g_log_config.context_key_initialized) {
        return empty_context;
    }
    
    log_context_t *thread_context = pthread_getspecific(g_log_config.context_key);
    if (thread_context) {
        return *thread_context;
    }
    
    return empty_context;
}

// 带上下文的日志写入函数
void log_write_with_context(log_level_t level, log_module_t module, const log_context_t *context,
                           const char *file, int line, const char *func, const char *fmt, ...) {
    if (!g_log_config.initialized) {
        return;
    }
    
    if (!log_is_level_enabled(module, level)) {
        return;
    }
    
    // 保存当前上下文
    log_context_t old_context = log_get_thread_context();
    
    // 设置新上下文
    if (context) {
        log_set_context(context);
    }
    
    // 格式化日志消息
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    // 写入日志
    log_write(level, module, file, line, func, "%s", message);
    
    // 恢复旧上下文
    log_set_context(&old_context);
    
    // 清理临时上下文
    log_clear_context();
}

// 注册日志回调函数
int log_register_callback(log_callback_t callback, void *user_data) {
    if (!callback) {
        return -1;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    // 检查是否已经注册
    for (int i = 0; i < g_log_config.callback_count; i++) {
        if (g_log_config.callbacks[i].func == callback) {
            pthread_mutex_unlock(&g_log_config.mutex);
            return -2;  // 已经注册
        }
    }
    
    // 检查是否达到最大回调数
    if (g_log_config.callback_count >= (int)(sizeof(g_log_config.callbacks) / sizeof(g_log_config.callbacks[0]))) {
        pthread_mutex_unlock(&g_log_config.mutex);
        return -3;  // 回调数量已达上限
    }
    
    // 注册回调
    g_log_config.callbacks[g_log_config.callback_count].func = callback;
    g_log_config.callbacks[g_log_config.callback_count].user_data = user_data;
    g_log_config.callback_count++;
    
    pthread_mutex_unlock(&g_log_config.mutex);
    return 0;
}

// 注销日志回调函数
int log_unregister_callback(log_callback_t callback) {
    if (!callback) {
        return -1;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    // 查找回调
    int found = -1;
    for (int i = 0; i < g_log_config.callback_count; i++) {
        if (g_log_config.callbacks[i].func == callback) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        pthread_mutex_unlock(&g_log_config.mutex);
        return -2;  // 回调未注册
    }
    
    // 移除回调（通过移动后面的回调）
    for (int i = found; i < g_log_config.callback_count - 1; i++) {
        g_log_config.callbacks[i] = g_log_config.callbacks[i + 1];
    }
    g_log_config.callback_count--;
    
    pthread_mutex_unlock(&g_log_config.mutex);
    return 0;
}

// 设置日志轮转配置
void log_set_rotation_config(const log_rotation_config_t *config) {
    if (config) {
        pthread_mutex_lock(&g_log_config.mutex);
        g_log_rotation.max_file_size = config->max_file_size;
        g_log_rotation.max_file_count = config->max_file_count;
        g_log_rotation.rotate_on_size = config->rotate_on_size;
        g_log_rotation.rotate_on_time = config->rotate_on_time;
        g_log_rotation.rotate_interval_hours = config->rotate_interval_hours;
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 获取日志轮转配置
void log_get_rotation_config(log_rotation_config_t *config) {
    if (config) {
        pthread_mutex_lock(&g_log_config.mutex);
        config->max_file_size = g_log_rotation.max_file_size;
        config->max_file_count = g_log_rotation.max_file_count;
        config->rotate_on_size = g_log_rotation.rotate_on_size;
        config->rotate_on_time = g_log_rotation.rotate_on_time;
        config->rotate_interval_hours = g_log_rotation.rotate_interval_hours;
        pthread_mutex_unlock(&g_log_config.mutex);
    }
}

// 立即执行日志轮转
int log_rotate_now(void) {
    if (!g_log_config.log_file || !g_log_config.log_file_path[0]) {
        return -1;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    // 关闭当前日志文件
    fclose(g_log_config.log_file);
    g_log_config.log_file = NULL;
    
    // 生成轮转文件名
    char rotate_path[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", tm_info);
    
    snprintf(rotate_path, sizeof(rotate_path), "%s.%s", g_log_config.log_file_path, time_str);
    
    // 重命名当前日志文件
    if (rename(g_log_config.log_file_path, rotate_path) != 0) {
        // 重命名失败，尝试重新打开原文件
        g_log_config.log_file = fopen(g_log_config.log_file_path, "a");
        pthread_mutex_unlock(&g_log_config.mutex);
        return -2;
    }
    
    // 打开新的日志文件
    g_log_config.log_file = fopen(g_log_config.log_file_path, "a");
    if (!g_log_config.log_file) {
        pthread_mutex_unlock(&g_log_config.mutex);
        return -3;
    }
    
    // 更新上次轮转时间
    g_log_rotation.last_rotate_time = now;
    
    pthread_mutex_unlock(&g_log_config.mutex);
    return 0;
}

// 关闭日志系统
void log_deinit(void) {
    if (!g_log_config.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    // 关闭日志文件
    if (g_log_config.log_file) {
        fclose(g_log_config.log_file);
        g_log_config.log_file = NULL;
    }
    
    // 清除回调
    g_log_config.callback_count = 0;
    
    // 销毁上下文键
    if (g_log_config.context_key_initialized) {
        pthread_key_delete(g_log_config.context_key);
        g_log_config.context_key_initialized = false;
    }
    
    g_log_config.initialized = false;
    
    pthread_mutex_unlock(&g_log_config.mutex);
    pthread_mutex_destroy(&g_log_config.mutex);
}
