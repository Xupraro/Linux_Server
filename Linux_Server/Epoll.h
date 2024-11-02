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
	int GetWait_Count();
	epoll_event* GetEvents();
private:
	int epoll_fd, wait_count;
	epoll_event event;
	epoll_event* events;
};

