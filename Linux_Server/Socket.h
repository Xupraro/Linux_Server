#include<sys/socket.h>
#include<arpa/inet.h>
#include<iostream>
#include<string.h>
#include<errno.h>
#pragma once
using namespace std;
class Socket
{
public:
	bool Init();
	int Server_Socket();
	int Server_Bind();
	int Server_Listen();
	int GetSock();
private:
	int Server_Sock;
	sockaddr_in server_addr;
};

