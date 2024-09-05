#include "Socket.h"

bool Socket::Init()
{
	if (Server_Socket() == -1)
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

int Socket::Server_Socket()
{
	this->Server_Sock = socket(PF_INET, SOCK_STREAM, 0);;
	return this->Server_Sock;
}

int Socket::Server_Bind()
{
	uint16_t port;
	cout << "请输入端口:" << endl;
	cin >> port;
	this->server_addr.sin_family = AF_INET;
	this->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	this->server_addr.sin_port = htons(port);
	return bind(this->Server_Sock, (struct sockaddr*)&this->server_addr, sizeof(this->server_addr));
}

int Socket::Server_Listen()
{
	return listen(this->Server_Sock, 5);
}

int Socket::GetSock()
{
	return this->Server_Sock;
}
