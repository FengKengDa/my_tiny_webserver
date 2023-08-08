#include "config.h"
#include "webserver.h"
#include <iostream>

int main()
{
    string user = "root";
    string passwd = "Feng_kd1376352437";
    string databasename = "myserver";

    Config config;
    webserver m_webserver;
    m_webserver.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
        config.OPT_LINGER, config.TRIGMode, config.sql_num, config.thread_num,
        config.close_log, config.actor_model);
    m_webserver.log_write();
    m_webserver.sql_pool();
    m_webserver.thread_pool();
    m_webserver.trig_mode();
    m_webserver.event_listen();
    std::cout<<"enter loop"<<std::endl;
    m_webserver.event_loop();
    return 0;
}