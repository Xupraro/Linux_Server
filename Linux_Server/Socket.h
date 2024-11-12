#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<algorithm>
#include<iostream>
#include<vector>
#pragma once
class Socket
{
public:
	Socket();
	~Socket();
	bool Init();
	void Client_Add();
	void Client_Del(int Client_fd);
	int Server_Socket();
	int Server_Set();
	int Server_Bind();
	int Server_Listen();
	int Server_Accept();
	int GetS_Sock();
	int GetC_Sock();
	int GetClient_Count();
	int GetClients_fd(int i);
	std::vector<int>& GetClients();
private:
	/*int* clients;
	int Client_Count;*/
	int Server_Sock, Client_Sock, opt;
	uint16_t port;
	socklen_t client_len;
	sockaddr_in server_addr, client_addr;
	std::vector<int> clients;
};

