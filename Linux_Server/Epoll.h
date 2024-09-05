#include<sys/epoll.h>
#pragma once
class Epoll
{
public:
	Epoll();
	~Epoll();
	bool Init(int Socket_fd, EPOLL_EVENTS type);
	int Epoll_Create();
	int Epoll_Wait();
	int Event_Add(int Socket_fd, EPOLL_EVENTS type);
	int Event_Del(int Socket_fd);
	int GetEvents_fd(int i);
	int GetEpoll_fd();
	epoll_event* GetEvents();
private:
	int Timeout;
	int epoll_fd;
	int events_length;
	epoll_event event;
	epoll_event* events;
};

