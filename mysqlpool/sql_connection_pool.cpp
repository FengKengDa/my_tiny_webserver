#include <mysql/mysql.h>
#include <string>
#include <string.h>
#include <list>
#include <pthread.h>
#include "sql_connection_pool.h"

connection_pool::connection_pool()
{
    CurConn = 0;
    FreeConn = 0;
}

connection_pool *connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn)
{
    this->url = url;
    this->port = Port;
    this->user = User;
    this->password = PassWord;
    this->database_name = DBName;

    lock.lock();
    //创建数据库链接
    for(int i=0;i<MaxConn;i++)
    {
        MYSQL* con = mysql_init(NULL);

        if(con == NULL)
        {
            //TODO: log the error

            exit(1);
        }

        con=mysql_real_connect(con, url.c_str(), user.c_str(), password.c_str(),database_name.c_str(), Port, NULL, 0);

        if(con == NULL)
        {
            //TODO: log the error

            exit(1);
        }
        connList.push_back(con);
        FreeConn++;
    }
    reserve = sem(FreeConn);
    MaxConn=FreeConn;
    lock.unlock();
}

MYSQL *connection_pool::GetConnection()
{
    MYSQL* con = NULL;
    if(connList.size()==0)
    {
        return NULL;
    }
    reserve.wait();
    lock.lock();
    //从列表里面取出第一个数据库链接
    con = connList.front();
    connList.pop_front();

    FreeConn--;
    CurConn++;
    lock.unlock();
    return con;
}

bool connection_pool::ReleaseConnection(MYSQL* con)
{
    if(con==NULL)
    {
        return false;
    }
    lock.lock();

    connList.push_back(con);
    FreeConn++;
    CurConn--;
    
    lock.unlock();
    reserve.post();
    return true;
}

void connection_pool::DestoryPool()
{
    lock.lock();
    if(!connList.empty())
    {
        for(std::list<MYSQL*>::iterator it = connList.begin(); it!=connList.end();++it)
        {
            MYSQL* con = *it;
            mysql_close(con);
        }
        CurConn=0;
        FreeConn=0;
        connList.clear();
    }
    lock.unlock();
}

int connection_pool::GetFreeConn()
{
    return FreeConn;
}

connection_pool::~connection_pool()
{
    DestoryPool();
}

connectionRAII::connectionRAII(MYSQL** sql, connection_pool *pool)
{
    *sql = pool->GetConnection();
    conRAII = *sql;
    poolRAII = pool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}