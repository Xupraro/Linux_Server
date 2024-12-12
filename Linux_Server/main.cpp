#include<cstdio>
#include<stdlib.h>
#include<string.h>
#include<json/json.h>
#include<iostream>
#include<errno.h>
#include<thread>
#include<vector>
#include<mutex>
#include<netinet/tcp.h>
#include<memory>
#include"ThreadPool.h"
#include"Socket.h"
#include"Epoll.h"
#include"Mysql.h"
using namespace std;

mutex mtx;

void thread_init()
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    if (mysql->Mysql_Query("update usermsg set epoll_id = -1 where epoll_id != -1;") != 0)
    {
        cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    cout << "init线程结束！" << endl;
}

void thread_userdata(vector<int>& clients)
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    if (mysql->Mysql_Query("select name,epoll_id from usermsg where epoll_id != -1;") != 0)
    {
        cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    Json::Value user;
    Json::Value users(Json::arrayValue);
    while (mysql->GetRow())
    {
        user["name"] = mysql->GetR()[0];
        user["epoll_id"] = mysql->GetR()[1];
        users.append(user);
    }
    Json::Value send_root;
    send_root["type"] = "USER";
    send_root["data"] = users;
    size_t len = send_root.toStyledString().size();
    string data = send_root.toStyledString();
    for (auto it : clients)
    {
        {
            shared_ptr<char> buf(new char[len], [](char* p) {delete[]p;});
            strcpy(buf.get(), data.data());
            unique_lock<mutex> lock(mtx);
            if (write(it, buf.get(), len) == -1)
            {
                cout << "register Write error:" << strerror(errno) << endl;
                return;
            }
        }
    }
}

void thread_login(Json::Value root, int client_fd, shared_ptr<ThreadPool>& pool, vector<int>& clients)
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    if (mysql->Mysql_Query("select password,name from usermsg where user = " + root["user"].asString() + ";") != 0)
    {
        cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    Json::Value send_root;
    send_root["type"] = "LOGIN_TOKEN";
    mysql->GetRow();
    if (mysql->GetR() != NULL && mysql->GetR()[0] == root["password"].asString())
    {
        send_root["token"] = "TRUE";
        send_root["name"] = mysql->GetR()[1];
        if (mysql->Mysql_Query("update usermsg set epoll_id = " + to_string(client_fd) + " where user = " + root["user"].asString() + ";") != 0)
        {
            cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
            return;
        }
        pool->enqueue(thread_userdata, clients);
    }
    else
    {
        send_root["token"] = "FALSE";
    }
    size_t len = send_root.toStyledString().size();
    string data = send_root.toStyledString();
    shared_ptr<char> buf(new char[len], [](char* p) {delete[]p;});
    strcpy(buf.get(), data.data());
    unique_lock<mutex> lock(mtx);
    if (write(client_fd, buf.get(), len) == -1)
    {
        cout << "Login Write error:" << strerror(errno) << endl;
        return;
    }
}

void thread_register(Json::Value root, int client_fd)
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    Json::Value send_root;
    send_root["type"] = "REGISTER_TOKEN";
    cout << "insert into usermsg values(" + root["user"].asString() + ",'" + root["password"].asString() + "','" + root["name"].asString() + "',-1);" << endl;
    if (mysql->Mysql_Query("insert into usermsg values(" + root["user"].asString() + ",'" + root["password"].asString() + "','" + root["name"].asString() + "',-1);") != 0)
    {
        if (mysql->GetErrno() == 1062)
        {
            send_root["token"] = "FALSE";
        }
        else
        {
            cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
            return;
        }
    }
    else
    {
        send_root["token"] = "TRUE";
    }
    shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
    strcpy(buf.get(), send_root.toStyledString().data());
    unique_lock<mutex> lock(mtx);
    if (write(client_fd, buf.get(), send_root.toStyledString().size()) == -1)
    {
        cout << "register Write error:" << strerror(errno) << endl;
        return;
    }
}

void thread_disconnect(int Client_fd, vector<int>& clients)
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    if (mysql->Mysql_Query("update usermsg set epoll_id = -1 where epoll_id = " + to_string(Client_fd) + ";") != 0)
    {
        cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    Json::Value send_root;
    send_root["type"] = "DELETE";
    send_root["epoll_id"] = to_string(Client_fd);
    size_t len = send_root.toStyledString().size();
    string data = send_root.toStyledString();
    for (auto it : clients)
    {
        {
            shared_ptr<char> buf(new char[len], [](char* p) {delete[]p;});
            strcpy(buf.get(), data.data());
            unique_lock<mutex> lock(mtx);
            if (write(it, buf.get(), len) == -1)
            {
                cout << "disconnect Write error:" << strerror(errno) << endl;
                return;
            }
        }
    }
}

void thread_chat(Json::Value root, int client_fd)
{
    Json::Value send_root;
    send_root["type"] = "SAYFROM";
    send_root["epoll_id"] = to_string(client_fd);
    send_root["data"] = root["data"];
    shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
    strcpy(buf.get(), send_root.toStyledString().data());
    unique_lock<mutex> lock(mtx);
    if (write(stoi(root["epoll_id"].asString()), buf.get(), send_root.toStyledString().size()) == -1)
    {
        cout << "chat Write error:" << strerror(errno) << endl;
        return;
    }
}

void thread_video(Json::Value root, int client_fd, string ip)
{
    Json::Value send_root;
    send_root["type"] = "VIDEOFROM";
    send_root["epoll_id"] = to_string(client_fd);
    send_root["ip"] = ip;
    shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
    strcpy(buf.get(), send_root.toStyledString().data());
    unique_lock<mutex> lock(mtx);
    if (write(stoi(root["epoll_id"].asString()), buf.get(), send_root.toStyledString().size()) == -1)
    {
        cout << "video Write error:" << strerror(errno) << endl;
        return;
    }
}

void thread_res(Json::Value root, int client_fd, string ip)
{
    Json::Value send_root;
    send_root["type"] = "RES";
    send_root["epoll_id"] = to_string(client_fd);
    send_root["res"] = root["res"];
    send_root["ip"] = ip;
    shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
    strcpy(buf.get(), send_root.toStyledString().data());
    unique_lock<mutex> lock(mtx);
    if (write(stoi(root["epoll_id"].asString()), buf.get(), send_root.toStyledString().size()) == -1)
    {
        cout << "res Write error:" << strerror(errno) << endl;
        return;
    }
}

void thread_text(Json::Value root, vector<int>& clients)
{
    for (auto it : clients)
    {
        {
            shared_ptr<char> buf(new char[root.toStyledString().size()], [](char* p) {delete[]p;});
            strcpy(buf.get(), root.toStyledString().data());
            unique_lock<mutex> lock(mtx);
            if (write(it, buf.get(), root.toStyledString().size()) == -1)
            {
                cout << "text Write error:" << strerror(errno) << endl;
                return;
            }
        }
    }
}

void Server(char* arg)
{
    shared_ptr<ThreadPool> pool = make_shared<ThreadPool>(8);
    shared_ptr<Socket> Socket_sock = make_shared<Socket>();
    if (!Socket_sock->Init())
    {
        cout << "Socket_Init error:" << strerror(errno) << endl;
        return;
    }

    shared_ptr<Epoll> epoll = make_shared<Epoll>();
    if (!epoll->Init(Socket_sock->GetS_Sock(), EPOLLIN))
    {
        cout << "Epoll_Init error:" << strerror(errno) << endl;
        return;
    }

    cout << "服务端已启动......" << endl;

    while (true)
    {
        epoll->Epoll_Wait();
        if (epoll->GetWait_Count() == -1)
        {
            cout << "Epoll_Wait error:" << strerror(errno) << endl;
            return;
        }
        else if (epoll->GetWait_Count() == 0)
        {
            continue;
        }
        else if (epoll->GetWait_Count() > 0)
        {
            for (int i = 0;i < epoll->GetWait_Count();i++)
            {
                if (epoll->GetEvents_fd(i) == Socket_sock->GetS_Sock())
                {
                    if (Socket_sock->Server_Accept() == -1)
                    {
                        cout << "accept error:" << strerror(errno) << endl;
                        break;
                    }
                    cout << "客户端" << Socket_sock->GetC_Sock() << "已连接！" << endl;
                    Socket_sock->Client_Add();
                    epoll->Event_Add(Socket_sock->GetC_Sock(), EPOLLIN);
                }
                else
                {
                    string msg;
                    char buf[10 * 1024] = { 0 };
                    if (read(epoll->GetEvents_fd(i), &buf, sizeof(buf)) == -1)
                    {
                        cout << "read error:" << strerror(errno) << endl;
                    }
                    msg += buf;
                    Json::Reader reader;
                    Json::Value root;
                    reader.parse(msg, root);
                    cout << root.toStyledString() << endl;
                    if (msg.length() == 0 || msg == "0")
                    {
                        epoll->Event_Del(epoll->GetEvents_fd(i));
                        Socket_sock->Client_Del(epoll->GetEvents_fd(i));
                        pool->enqueue(thread_disconnect, epoll->GetEvents_fd(i), Socket_sock->GetClients());
                        cout << "客户端" << epoll->GetEvents_fd(i) << "断开连接！" << endl;
                    }
                    else if (root["type"].asString() == "LOGIN")
                    {
                        pool->enqueue(thread_login, root, epoll->GetEvents_fd(i), pool, Socket_sock->GetClients());
                    }
                    else if (root["type"].asString() == "REGISTER")
                    {
                        pool->enqueue(thread_register, root, epoll->GetEvents_fd(i));
                    }
                    else if (root["type"].asString() == "TEXT")
                    {
                        pool->enqueue(thread_text, root, Socket_sock->GetClients());
                    }
                    else if (root["type"].asString() == "SAYTO")
                    {
                        pool->enqueue(thread_chat, root, epoll->GetEvents_fd(i));
                    }
                    else if (root["type"].asString() == "VIDEOTO")
                    {
                        pool->enqueue(thread_video, root, epoll->GetEvents_fd(i), Socket_sock->GetClient_ip(epoll->GetEvents_fd(i)));
                    }
                    else if (root["type"].asString() == "VIDEORES")
                    {
                        pool->enqueue(thread_res, root, epoll->GetEvents_fd(i), Socket_sock->GetClient_ip(epoll->GetEvents_fd(i)));
                    }
                }
            }
        }
    }
    return;
}

int main(int argc, char* argv[])
{
    thread Init(thread_init);
    if (Init.joinable())
    {
        Init.join();
    }
    Server(argv[1]);
    return 0;
}