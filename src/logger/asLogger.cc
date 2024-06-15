#include <iostream>
#include <time.h> 

#include "asLogger.h"
#include "Timestamp.h"


// 获取日志唯一实例对象
Logger& Logger::instance() 
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

// 写日志
void Logger::log(std::string msg)
{
    m_lckQue.Push(msg);

    std::string loglevel;
    if (logLevel_ == INFO) loglevel = "[INFO]";
    else if (logLevel_ == ERROR) loglevel = "[ERROR]";
    else if ((logLevel_ == FATAL)) loglevel = "[FATAL]";
    else loglevel = "[DEBUG]";

    // 打印时间
    std::cout << loglevel << Timestamp::now().toString() << " : " << msg << std::endl;
}


// 构造函数，启动子线程单独处理日志的写入
Logger::Logger()
{
    std::thread writeLogTask([&]() {
        // 获取当前日期   取出缓冲队列中的日志信息    拼接好后写入对应日志文件中
        for (;;)
        {
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);

            char file_name[128];
            sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

            // 打开文件
            FILE *pf = fopen(file_name, "a+");
            if (nullptr == pf)
            {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }
            
            std::string msg = m_lckQue.Pop();

            std::string loglevel;
            if (logLevel_ == INFO) loglevel = "info";
            else if (logLevel_ == ERROR) loglevel = "error";
            else if ((logLevel_ == FATAL)) loglevel = "fatal";
            else loglevel = "debug";


            // 加入具体时间信息
            char time_buf[128] = {0};
            sprintf(time_buf, "%s => [%s]",
                    Timestamp::now().toString().c_str(),
                    loglevel.c_str());
            msg.insert(0, time_buf);
            msg.append("\n");

            // 写入数据
            fputs(msg.c_str(), pf);

            // 关闭文件
            fclose(pf);
        }
    });

    // 分离线程，该线程为 守护线程 专门在后台进行写入 log.txt 的操作
    writeLogTask.detach();
}
