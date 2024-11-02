#include "Epoll.h"

Epoll::Epoll()
{
	this->events = new epoll_event[1024]{ 0 };
}

Epoll::~Epoll()
{
	delete[]this->events;
}

bool Epoll::Init(int Socket_fd, EPOLL_EVENTS type)
{
	if (Epoll_Create() == -1)
	{
		return false;
	}
	if (Event_Add(Socket_fd, type) == -1)
	{
		return false;
	}
	return true;
}

int Epoll::Epoll_Create()
{
	this->epoll_fd = epoll_create(1);
	return this->epoll_fd;
}

int Epoll::Epoll_Wait()
{
	this->wait_count = epoll_wait(this->epoll_fd, this->events, 1024, -1);
	return this->wait_count;
}

int Epoll::Event_Add(int Socket_fd, EPOLL_EVENTS type)
{
	this->event.events = type;
	this->event.data.fd = Socket_fd;
	return epoll_ctl(this->epoll_fd, EPOLL_CTL_ADD, Socket_fd, &this->event);
}

int Epoll::Event_Del(int Socket_fd)
{
	return epoll_ctl(this->epoll_fd, EPOLL_CTL_DEL, Socket_fd, nullptr);
}

int Epoll::GetEvents_fd(int i)
{
	return this->events[i].data.fd;
}

int Epoll::GetEpoll_fd()
{
	return this->epoll_fd;
}

int Epoll::GetWait_Count()
{
	return this->wait_count;
}

epoll_event* Epoll::GetEvents()
{
	return this->events;
}
