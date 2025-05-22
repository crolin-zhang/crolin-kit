#ifndef CROLINKIT_LOG_H
#define CROLINKIT_LOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file log.h
 * @brief 统一的日志接口，提供日志记录、控制和配置功能
 *
 * 该模块提供了一套完整的日志系统，支持多级别日志、模块化日志分类、
 * 灵活的输出配置和运行时可配置的日志级别和格式。
 *
 * 主要功能包括：
 * - 基本日志记录（不同级别）
 * - 模块化日志控制（启用/禁用、级别控制）
 * - 日志上下文管理（线程本地存储）
 * - 日志回调机制（自定义日志处理）
 * - 日志轮转功能（基于大小和时间）
 */

// 日志级别名称
extern const char *log_level_names[];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 日志级别定义
 */
typedef enum {
    LOG_LEVEL_FATAL = 0,  // 致命错误，系统无法继续运行
    LOG_LEVEL_ERROR,      // 一般错误，功能无法正常工作
    LOG_LEVEL_WARN,       // 警告信息，可能存在问题
    LOG_LEVEL_INFO,       // 一般信息，重要操作和状态变化
    LOG_LEVEL_DEBUG,      // 调试信息，详细的程序执行信息
    LOG_LEVEL_TRACE       // 跟踪信息，最详细的调试数据
} log_level_t;

/**
 * @brief 日志模块定义
 */
typedef enum {
    LOG_MODULE_CORE = 0,  // 核心模块
    LOG_MODULE_THREAD,    // 线程模块
    LOG_MODULE_LOG,       // 日志模块
    LOG_MODULE_MAX        // 模块数量
} log_module_t;

/**
 * @brief 获取日志模块名称
 * @param module 模块枚举值
 * @return 模块名称字符串
 */
const char* log_get_module_name(log_module_t module);

/**
 * @brief 日志格式选项
 */
typedef struct {
    bool show_time;       // 是否显示时间戳
    bool show_tid;        // 是否显示线程ID
    bool show_module;     // 是否显示模块名
    bool show_file_line;  // 是否显示文件名和行号
    bool show_function;   // 是否显示函数名
    bool use_colors;      // 是否使用颜色
    bool use_iso_time;    // 是否使用ISO 8601时间格式
    char time_format[32]; // 自定义时间格式（strftime格式）
} log_format_options_t;

/**
 * @brief 每个模块的日志配置
 */
typedef struct {
    log_level_t level;     // 日志级别
    bool console_output;   // 是否输出到控制台
    bool file_output;      // 是否输出到文件
    bool enabled;          // 是否启用
    char *custom_file;     // 模块专用日志文件（可选）
} module_log_config_t;

/**
 * @brief 日志轮转配置
 */
typedef struct {
    size_t max_file_size;      // 单个日志文件最大大小（字节）
    int max_file_count;        // 最大日志文件数量
    bool rotate_on_size;       // 是否按大小轮转
    bool rotate_on_time;       // 是否按时间轮转
    int rotate_interval_hours; // 时间轮转间隔（小时）
} log_rotation_config_t;

/**
 * @brief 日志上下文结构
 * 
 * 用于在日志消息中添加上下文信息，如会话ID、用户ID等。
 * 这些上下文信息存储在线程本地存储中，对每个线程独立。
 */
typedef struct {
    const char *context_id;     // 上下文标识符
    const char *session_id;     // 会话ID
    const char *user_id;        // 用户ID
    const char *transaction_id; // 事务ID
} log_context_t;

/**
 * @brief 日志回调函数类型
 * 
 * 当日志消息被写入时，所有注册的回调函数都会被调用。
 * 可以用于自定义日志处理，如发送到远程服务器、写入数据库等。
 * 
 * @param level 日志级别
 * @param module 日志模块
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param message 格式化后的日志消息
 * @param user_data 用户数据，由注册回调时提供
 */
typedef void (*log_callback_t)(log_level_t level, log_module_t module, 
                              const char *file, int line, const char *func, 
                              const char *message, void *user_data);

/*******************************************************************************
 * 基本日志接口
 *******************************************************************************/

/**
 * @brief 初始化日志系统
 * 
 * @param log_file 日志文件路径，如果为NULL则使用默认路径
 * @param level 默认日志级别
 * @return 成功返回0，失败返回负数错误码
 */
int log_init(const char *log_file, log_level_t level);

/**
 * @brief 关闭日志系统
 */
void log_deinit(void);

/**
 * @brief 写入日志
 * 
 * 核心日志写入函数，通常通过宏调用而不是直接调用。
 * 
 * @param level 日志级别
 * @param module 日志模块
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_write(log_level_t level, log_module_t module, const char *file,
              int line, const char *func, const char *fmt, ...);

/*******************************************************************************
 * 模块级别的日志控制接口
 *******************************************************************************/

/**
 * @brief 设置模块的日志级别
 * 
 * @param module 日志模块
 * @param level 日志级别
 */
void log_set_module_level(log_module_t module, log_level_t level);

/**
 * @brief 设置模块的输出目标
 * 
 * @param module 日志模块
 * @param console_on 是否输出到控制台
 * @param file_on 是否输出到文件
 */
void log_set_module_output(log_module_t module, bool console_on, bool file_on);

/**
 * @brief 设置模块是否启用
 * 
 * @param module 日志模块
 * @param enable 是否启用
 */
void log_set_module_enable(log_module_t module, bool enable);

/**
 * @brief 获取模块的日志级别
 * 
 * @param module 日志模块
 * @return 日志级别
 */
log_level_t log_get_module_level(log_module_t module);

/**
 * @brief 获取模块是否启用
 * 
 * @param module 日志模块
 * @return 是否启用
 */
bool log_get_module_enable(log_module_t module);

/**
 * @brief 检查指定级别的日志是否会被记录
 * 
 * @param module 日志模块
 * @param level 日志级别
 * @return 如果会被记录返回true，否则返回false
 */
bool log_is_level_enabled(log_module_t module, log_level_t level);

/**
 * @brief 设置日志格式选项
 * 
 * @param options 格式选项
 */
void log_set_format_options(const log_format_options_t *options);

/**
 * @brief 获取日志格式选项
 * 
 * @param options 用于存储格式选项的结构体指针
 */
void log_get_format_options(log_format_options_t *options);

/**
 * @brief 设置日志轮转配置
 * 
 * @param config 轮转配置
 */
void log_set_rotation_config(const log_rotation_config_t *config);

/**
 * @brief 获取日志轮转配置
 * 
 * @param config 用于存储轮转配置的结构体指针
 */
void log_get_rotation_config(log_rotation_config_t *config);

/**
 * @brief 立即执行日志轮转
 * 
 * @return 成功返回0，失败返回负数错误码
 */
int log_rotate_now(void);

/**
 * @brief 注册日志回调函数
 * 
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 成功返回0，失败返回负数错误码
 */
int log_register_callback(log_callback_t callback, void *user_data);

/**
 * @brief 注销日志回调函数
 * 
 * @param callback 回调函数
 * @return 成功返回0，失败返回负数错误码
 */
int log_unregister_callback(log_callback_t callback);

/*******************************************************************************
 * 日志上下文管理接口
 *******************************************************************************/

/**
 * @brief 设置日志上下文
 * 
 * @param context 上下文信息
 */
void log_set_context(const log_context_t *context);

/**
 * @brief 清除日志上下文
 */
void log_clear_context(void);

/**
 * @brief 获取当前线程的日志上下文
 * 
 * @return 日志上下文
 */
log_context_t log_get_thread_context(void);

/**
 * @brief 带上下文的日志写入函数
 * 
 * @param level 日志级别
 * @param module 日志模块
 * @param context 上下文信息
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void log_write_with_context(log_level_t level, log_module_t module, const log_context_t *context,
                           const char *file, int line, const char *func, const char *fmt, ...);

/*******************************************************************************
 * 日志宏
 *******************************************************************************/

/**
 * @brief 基本日志宏
 * 
 * 这些宏用于记录不同级别的日志，自动包含文件名、行号和函数名。
 */
#define LOG_FATAL(module, fmt, ...) \
    log_write(LOG_LEVEL_FATAL, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(module, fmt, ...) \
    log_write(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(module, fmt, ...) \
    log_write(LOG_LEVEL_WARN, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(module, fmt, ...) \
    log_write(LOG_LEVEL_INFO, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(module, fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_TRACE(module, fmt, ...) \
    log_write(LOG_LEVEL_TRACE, module, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

/**
 * @brief 条件日志宏
 * 
 * 这些宏仅在条件为真时记录日志，避免不必要的字符串格式化开销。
 */
#define LOG_FATAL_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_FATAL(module, fmt, ##__VA_ARGS__); } while(0)

#define LOG_ERROR_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_ERROR(module, fmt, ##__VA_ARGS__); } while(0)

#define LOG_WARN_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_WARN(module, fmt, ##__VA_ARGS__); } while(0)

#define LOG_INFO_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_INFO(module, fmt, ##__VA_ARGS__); } while(0)

#define LOG_DEBUG_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_DEBUG(module, fmt, ##__VA_ARGS__); } while(0)

#define LOG_TRACE_IF(cond, module, fmt, ...) \
    do { if (cond) LOG_TRACE(module, fmt, ##__VA_ARGS__); } while(0)

/**
 * @brief 带退出的致命错误日志宏
 * 
 * 记录致命错误日志并退出程序，通常用于无法恢复的错误。
 */
#define LOG_FATAL_EXIT(module, exit_code, fmt, ...) \
    do { LOG_FATAL(module, fmt, ##__VA_ARGS__); exit(exit_code); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* CROLINKIT_LOG_H */
