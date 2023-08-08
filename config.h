#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};
    //端口号
    int PORT;
    //日志写入方式
    int LOGWrite;
    //触发组合模式
    int TRIGMode;
    //listenfd触发模式
    int LISTENTrigmode;
    //connfd触发模式
    int CONNTrigmode;
    //优雅关闭链接
    int OPT_LINGER;
    //数据库连接池数量
    int sql_num;
    //线程池内的线程数量
    int thread_num;
    //是否关闭日志
    int close_log;
    //并发模型选择
    int actor_model;
};

Config::Config()
{
        
    PORT = 7456;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT
    TRIGMode = 0;

    //listenfd触发模式，默认LT
    LISTENTrigmode = 0;

    //connfd触发模式，默认LT
    CONNTrigmode = 0;

    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是proactor
    actor_model = 0;
}

#endif