#include<sys/socket.h>
#include<arpa/inet.h>
#include<iostream>
#pragma once
class Socket
{
public:
	Socket();
	~Socket();
	bool Init();
	int Server_Socket();
	int Server_Bind();
	int Server_Listen();
	int Server_Accept();
	int GetS_Sock();
	int GetC_Sock();
	int GetClients_fd(int i);
private:
	int* clients;
	int Client_Count;
	int Server_Sock, Client_Sock;
	uint16_t port;
	socklen_t client_len;
	sockaddr_in server_addr, client_addr;
};

