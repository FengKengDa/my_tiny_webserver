#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include<mysql/mysql.h>
#include<list>
#include<string>
#include"../lock/locker.h"

using std::string;
using std::list;

class connection_pool
{
public:
    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL* conn);
    int GetFreeConn();
    void DestoryPool();

    static connection_pool *GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);

    connection_pool();
    ~connection_pool();

private:
	unsigned int MaxConn;
	unsigned int CurConn;
	unsigned int FreeConn;

    locker lock;
    sem reserve;
    
    list<MYSQL*> connList;

    string url;
    string port;
    string user;
    string password;
    string database_name;
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **conn, connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL* conRAII;
    connection_pool *poolRAII;

};



#endif