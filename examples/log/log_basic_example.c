/**
 * @file log_basic_example.c
 * @brief 日志模块基本使用示例
 */

#include <stdio.h>
#include "log.h"

int main(void)
{
    // 初始化日志系统，设置日志文件路径和默认日志级别
    int ret = log_init("log_basic_example.log", LOG_LEVEL_DEBUG);
    if (ret != 0) {
        fprintf(stderr, "日志系统初始化失败\n");
        return -1;
    }

    // 使用不同级别的日志
    LOG_FATAL(LOG_MODULE_CORE, "这是一条致命错误日志");
    LOG_ERROR(LOG_MODULE_CORE, "这是一条错误日志");
    LOG_WARN(LOG_MODULE_CORE, "这是一条警告日志");
    LOG_INFO(LOG_MODULE_CORE, "这是一条信息日志");
    LOG_DEBUG(LOG_MODULE_CORE, "这是一条调试日志");
    LOG_TRACE(LOG_MODULE_CORE, "这是一条跟踪日志，默认级别下不会显示");

    // 修改日志级别
    log_set_module_level(LOG_MODULE_CORE, LOG_LEVEL_TRACE);
    LOG_TRACE(LOG_MODULE_CORE, "修改日志级别后，跟踪日志可以显示了");

    // 使用不同的模块
    LOG_INFO(LOG_MODULE_THREAD, "线程模块的日志");
    LOG_INFO(LOG_MODULE_LOG, "日志模块的日志");

    // 条件日志
    int error_code = 404;
    LOG_ERROR_IF(error_code == 404, LOG_MODULE_CORE, "发生404错误: %d", error_code);

    // 格式化日志
    LOG_INFO(LOG_MODULE_CORE, "支持格式化: %d, %s, %.2f", 100, "字符串", 3.14159);

    // 关闭日志系统
    log_deinit();
    printf("日志示例完成，请查看 log_basic_example.log 文件\n");

    return 0;
}
