#pragma once
#include<mysql/mysql.h>
#include <string>
class Mysql
{
public:
	~Mysql();
	bool Init();
	MYSQL* Mysql_Create();
	MYSQL* Mysql_Connect();
	int Mysql_Query(std::string sql);
	MYSQL_ROW GetR();
	MYSQL_ROW GetRow();
	MYSQL* GetMysql();
	unsigned int GetErrno();
	const char* GetError();
private:
	MYSQL* mysql;
	MYSQL_RES* res;
	MYSQL_ROW r;
};

