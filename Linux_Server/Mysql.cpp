#include "Mysql.h"

Mysql::~Mysql()
{
	mysql_free_result(this->res);
	if (!mysql_ping(this->mysql))
	{
		mysql_close(this->mysql);
	}
}

bool Mysql::Init()
{
	if (Mysql_Create() == nullptr || Mysql_Connect() == nullptr)
	{
		return false;
	}
	return true;
}

MYSQL* Mysql::Mysql_Create()
{
	this->mysql = mysql_init(nullptr);
	return this->mysql;
}

MYSQL* Mysql::Mysql_Connect()
{
	return mysql_real_connect(this->mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, nullptr, 0);
}

int Mysql::Mysql_Query(std::string sql)
{
	int return_value = mysql_query(this->mysql, sql.c_str());
	this->res = mysql_store_result(this->mysql);
	return return_value;
}

MYSQL_ROW Mysql::GetR()
{
	return this->r;
}

MYSQL_ROW Mysql::GetRow()
{
	this->r = mysql_fetch_row(this->res);
	return this->r;
}

MYSQL* Mysql::GetMysql()
{
	return this->mysql;
}

unsigned int Mysql::GetErrno()
{
	return mysql_errno(this->mysql);
}

const char* Mysql::GetError()
{
	return mysql_error(this->mysql);
}
