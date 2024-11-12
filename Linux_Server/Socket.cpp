#include "Socket.h"

Socket::Socket()
{
	/*this->Client_Count = 0;
	this->clients = new int[1024] {0};*/
	opt = 1;
	this->client_len = sizeof(this->Client_Sock);
}

Socket::~Socket()
{
	close(this->Server_Sock);
	std::cout << "close server:" << this->Server_Sock << std::endl;
	for (int i = 0;i < this->clients.size();++i)
	{
		close(this->clients[i]);
		std::cout << "close client:" << this->clients[i] << std::endl;
	}
	this->clients.clear();
}

bool Socket::Init()
{
	if (Server_Socket() == -1)
	{
		return false;
	}
	if (Server_Set() == -1)
	{
		return false;
	}
	if (Server_Bind() == -1)
	{
		return false;
	}
	if (Server_Listen() == -1)
	{
		return false;
	}
	return true;
}

void Socket::Client_Add()
{
	this->clients.push_back(this->Client_Sock);
}

void Socket::Client_Del(int Client_fd)
{
	auto it = std::find(this->clients.begin(), this->clients.end(), Client_fd);
	this->clients.erase(it);
}

int Socket::Server_Socket()
{
	this->Server_Sock = socket(PF_INET, SOCK_STREAM, 0);
	return this->Server_Sock;
}

int Socket::Server_Set()
{
	return setsockopt(this->Server_Sock, SOL_SOCKET, SO_REUSEADDR, &this->opt, sizeof(this->opt));
}

int Socket::Server_Bind()
{
	std::cout << "请输入端口:";
	std::cin >> this->port;
	this->server_addr.sin_family = AF_INET;
	this->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	this->server_addr.sin_port = htons(this->port);
	return bind(this->Server_Sock, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr));
}

int Socket::Server_Listen()
{
	return listen(this->Server_Sock, 5);
}

int Socket::Server_Accept()
{
	this->Client_Sock = accept(this->Server_Sock, (struct sockaddr*)&this->client_addr, &this->client_len);
	return this->Client_Sock;
}

int Socket::GetS_Sock()
{
	return this->Server_Sock;
}

int Socket::GetC_Sock()
{
	return this->Client_Sock;
}

int Socket::GetClient_Count()
{
	return this->clients.size();
}

int Socket::GetClients_fd(int i)
{
	return this->clients[i];
}

std::vector<int>& Socket::GetClients()
{
	return this->clients;
}
