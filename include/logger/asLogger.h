#pragma once

#include <string>
#include <atomic>

#include "noncopyable.h"
#include "lockqueue.h"


/*
* @         INFO : 普通信息
* @         ERROR: 错误信息
* @         FATAL: core信息
* @         DEBUG: 调试信息
*/


enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// 异步日志系统  (单例模式)
class Logger
{
public:
    
    // 获取日志唯一实例对象
    static Logger& instance();

    // 设置日志级别
    void setLogLevel(int level);

    // 写日志
    void log(std::string msg);

private:
    int logLevel_;                          // 记录日志级别
    LockQueue<std::string> m_lckQue;        // 日志缓冲队列

    Logger();
};


/*
    使用方法：
    LOG_INFO("%s, %d", arg1, arg2);
*/
#define LOG_INFO(logmsgFormat, ...)                             \
    do                                                          \
    {                                                           \
        Logger &logger = Logger::instance();                    \
        logger.setLogLevel(INFO);                               \
        char buf[1024] = {0};                                   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);       \
        logger.log(buf);                                        \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                            \
    do                                                          \
    {                                                           \
        Logger &logger = Logger::instance();                    \
        logger.setLogLevel(ERROR);                              \
        char buf[1024] = {0};                                   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);       \
        logger.log(buf);                                        \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                            \
    do                                                          \
    {                                                           \
        Logger &logger = Logger::instance();                    \
        logger.setLogLevel(FATAL);                              \
        char buf[1024] = {0};                                   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);       \
        logger.log(buf);                                        \
        exit(-1);                                               \
    } while (0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                            \
    do                                                          \
    {                                                           \
        Logger &logger = Logger::instance();                    \
        logger.setLogLevel(DEBUG);                              \
        char buf[1024] = {0};                                   \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__);       \
        logger.log(buf);                                        \
    } while (0)
#else
    #define LOG_DEBUG(logmsgFormat, ...) 
#endif



