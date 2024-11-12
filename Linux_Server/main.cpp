#include<cstdio>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<json/json.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<iostream>
#include<errno.h>
#include<sys/wait.h>
#include<sys/epoll.h>
#include<sys/time.h>
#include<fstream>
#include<thread>
#include<vector>
#include<mysql/mysql.h>
#include<mutex>
#include<netinet/tcp.h>
#include<memory>
#include"ThreadPool.h"
#include"Socket.h"
#include"Epoll.h"
#include"Mysql.h"
using namespace std;

mutex mtx;
int thread_i = 0;

struct thread_msg
{
    char msg[1024];
    int client_sock;
    int* client;
    int client_num;
};
struct delete_msg
{
    int* client;
    int client_num;
    int client_sock;
};
struct siliao_msg
{
    char msg[1024];
    int to_sock;
    int from_sock;
};

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
    for (auto it : clients)
    {
        {
            shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
            strcpy(buf.get(), send_root.toStyledString().data());
            unique_lock<mutex> lock(mtx);
            if (write(it, buf.get(), send_root.toStyledString().size()) == -1)
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
    shared_ptr<char> buf(new char[send_root.toStyledString().size()], [](char* p) {delete[]p;});
    strcpy(buf.get(), send_root.toStyledString().data());
    unique_lock<mutex> lock(mtx);
    if (write(client_fd, buf.get(), send_root.toStyledString().size()) == -1)
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

void thread_disconnect(int client_fd, vector<int>& clients)
{
    shared_ptr<Mysql> mysql = make_shared<Mysql>();
    if (!mysql->Init())
    {
        cout << "Mysql Init error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    if (mysql->Mysql_Query("update usermsg set epoll_id = -1 where epoll_id = " + to_string(client_fd) + ";") != 0)
    {
        cout << "Mysql Query error:" << mysql->GetErrno() << mysql->GetError() << endl;
        return;
    }
    Json::Value send_root;
    send_root["type"] = "DELETE";
    send_root["epoll_id"] = to_string(client_fd);
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

//void* thread_userdata(void* args)
//{
//    pthread_detach(pthread_self());
//    thread_msg* tm = (thread_msg*)args;
//    int cn = tm->client_num;
//    MYSQL mysql_conn;
//    MYSQL* mysql = mysql_init(&mysql_conn);
//    if (mysql == NULL)
//    {
//        printf("mysql init err\n");
//        pthread_exit((void*)111);
//    }
//    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
//    {
//        printf("connect err\n");
//        pthread_exit((void*)111);
//    }
//    string sql = "select name,epoll_id from usermsg where epoll_id != -1;";
//    if (mysql_query(mysql, sql.c_str()) == 0);
//    MYSQL_RES* res = mysql_store_result(mysql);
//    if (res == NULL)
//    {
//        printf("get err！\n");
//        mysql_close(mysql);
//        pthread_exit((void*)111);
//    }
//    int hang = mysql_num_rows(res);
//    int lie = mysql_num_fields(res);
//    for (int i = 0;i < hang;i++)
//    {
//        MYSQL_ROW r = mysql_fetch_row(res);
//        string sendmsg = "USER:";
//        for (int j = 0;j < lie;j++)
//        {
//            sendmsg += "##";
//            sendmsg += r[j];
//        }
//        char buf[sendmsg.size()];
//        strcpy(buf, sendmsg.data());
//        for (int j = 0;j < cn;j++)
//        {
//            if (tm->client[j] != -1)
//            {
//                if (r[1] != to_string(tm->client[j]))
//                {
//                    mtx.lock();
//                    ssize_t wlen = write(tm->client[j], &buf, sendmsg.size());
//                    mtx.unlock();
//                    if (wlen == -1)
//                        cout << "write error:" << strerror(errno) << endl;
//                    struct timeval tv;
//                    tv.tv_sec = 0;
//                    tv.tv_usec = 201 * 1000;
//                    select(0, NULL, NULL, NULL, &tv);
//                }
//            }
//            else if (tm->client[j] == -1)
//                cn++;
//        }
//    }
//    mysql_free_result(res);
//    cout << "userdata线程结束！" << endl;
//}

//void* thread_disconnected(void* args)
//{
//    pthread_detach(pthread_self());
//    delete_msg* dm = (delete_msg*)args;
//    int cn = dm->client_num;
//    MYSQL mysql_conn;
//    MYSQL* mysql = mysql_init(&mysql_conn);
//    if (mysql == NULL)
//    {
//        printf("mysql init err\n");
//        pthread_exit((void*)111);
//    }
//    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
//    {
//        printf("connect err\n");
//        pthread_exit((void*)111);
//    }
//    string sql = "update usermsg set epoll_id = -1 where epoll_id = " + to_string(dm->client_sock) + ";";
//    if (mysql_query(mysql, sql.c_str()) == 0);
//    mysql_close(mysql);
//    string sendmsg = "DELETE:##" + to_string(dm->client_sock);
//    char buf[sendmsg.size()];
//    strcpy(buf, sendmsg.data());
//    for (int i = 0;i < cn;i++)
//    {
//        if (dm->client[i] != -1)
//        {
//            mtx.lock();
//            ssize_t wlen = write(dm->client[i], &buf, sendmsg.size());
//            mtx.unlock();
//            if (wlen == -1)
//                cout << "write error:" << strerror(errno) << endl;
//        }
//        else if (dm->client[i] == -1)
//            cn++;
//    }
//
//    cout << "disconnected线程结束!" << endl;
//}

//void* thread_yanzheng(void* args)
//{
//    pthread_detach(pthread_self());
//    string sql;
//    string name;
//    string sendmsg;
//    thread_msg* tm = (thread_msg*)args;
//    string msg = tm->msg;
//    if (msg.substr(0, 5) == "DLZC:")
//    {
//        name = msg.substr(msg.find("$$") + 2);
//    }
//    string user = msg.substr(msg.find("##") + 2, msg.find("@@") - msg.find("##") - 2);
//    string password = msg.substr(msg.find("@@") + 2, msg.find("$$") - msg.find("@@") - 2);
//
//    MYSQL mysql_conn;
//    MYSQL* mysql = mysql_init(&mysql_conn);
//    if (mysql == NULL)
//    {
//        printf("mysql init err\n");
//        pthread_exit((void*)111);
//    }
//    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
//    {
//        printf("connect err\n");
//        pthread_exit((void*)111);
//    }
//    if (mysql_query(mysql, "create table if not exists usermsg(user int primary key,password varchar(20) not null,name varchar(20) not null,epoll_id int not null);") == 0)
//    {
//        //cout << "表usermsg创建成功！" << endl;
//    }
//    else
//    {
//        printf("create err\n");
//        mysql_close(mysql);
//        pthread_exit((void*)111);
//    }
//    sql = "select password from usermsg where user = " + user + ";";
//    if (mysql_query(mysql, sql.c_str()) != 0)
//    {
//        cout << "select err！" << endl;
//        mysql_close(mysql);
//        pthread_exit((void*)111);
//    }
//    MYSQL_RES* res = mysql_store_result(mysql);
//    if (res == NULL)
//    {
//        printf("get err！\n");
//        mysql_close(mysql);
//        pthread_exit((void*)111);
//    }
//    MYSQL_ROW r = mysql_fetch_row(res);
//    
//    if (r != NULL)
//    {
//        if (msg.substr(0, 5) == "DLZC:")
//            sendmsg = "TOKEN:##CUNZAI";
//        else if (msg.substr(0, 5) == "DLYZ:")
//        {
//            if (password == r[0])
//            {
//                sendmsg = "TOKEN:##TRUE";
//                sql = "update usermsg set epoll_id = " + to_string(tm->client_sock) + " where user = " + user + ";";
//                if (mysql_query(mysql, sql.c_str()) == 0);
//                sql = "select name from usermsg where user = " + user + ";";
//                if (mysql_query(mysql, sql.c_str()) == 0);
//                res = mysql_store_result(mysql);
//                MYSQL_ROW n = mysql_fetch_row(res);
//                sendmsg += "##";
//                sendmsg += n[0];
//                pthread_t PID = thread_i;
//                thread_i++;
//                pthread_create(&PID, NULL, thread_userdata, (void*)tm);
//            }
//            else
//                sendmsg = "TOKEN:##FALSE";
//        }
//    }
//    else
//    {
//        if(msg.substr(0,5)=="DLYZ:")
//            sendmsg = "TOKEN:##NULL";
//        else if (msg.substr(0, 5) == "DLZC:")
//        {
//            sql = "insert into usermsg values(" + user + ",'" + password + "','" + name + "',-1);";
//            if (mysql_query(mysql, sql.c_str()) == 0);
//            sendmsg = "TOKEN:##CHENGGONG";
//        }
//    }
//    mysql_free_result(res);
//    mysql_close(mysql);
//
//    char buf[sendmsg.size()];
//    strcpy(buf, sendmsg.data());
//    mtx.lock();
//    ssize_t wlen = write(tm->client_sock, &buf, sendmsg.size());
//    mtx.unlock();
//    if (wlen == -1)
//        cout << "write error:" << strerror(errno) << endl;
//    
//    cout << "yanzheng线程结束！" << endl;
//}

void* thread_siliao(void* args)
{
    pthread_detach(pthread_self());
    siliao_msg* sm = (siliao_msg*)args;
    string msg = sm->msg;
    sm->to_sock = stoi(msg.substr(msg.find("##") + 2, msg.find("@@") - msg.find("##") - 2));
    string sendmsg = "FROM:##";
    sendmsg += to_string(sm->from_sock) + "##" + msg.substr(msg.find("@@") + 2);
    char buf[sendmsg.size()];
    strcpy(buf, sendmsg.data());
    mtx.lock();
    ssize_t wlen = write(sm->to_sock, &buf, sendmsg.size());
    mtx.unlock();
    if (wlen == -1)
        cout << "write error:" << strerror(errno) << endl;
}

void Server(char* arg)
{
    //ThreadPool pool(8);
    shared_ptr<ThreadPool> pool = make_shared<ThreadPool>(8);
    //int client_sock, client_num = 0;
    //int* client = new int[1024];
    //for (int i = 0;i < 1024;i++)
    //    client[i] = -1;
    //sockaddr_in /*server_addr, */client_addr;

    /*Socket_sock->GetS_Sock() = socket(PF_INET, SOCK_STREAM, 0);
    if (Socket_sock->GetS_Sock() == -1)
    {
        cout << "socket error:" << strerror(errno) << endl;
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    uint16_t port;
    cout << "请输入端口:" << endl;
    cin >> port;
    server_addr.sin_port = htons(port);

    int ret = bind(Socket_sock->GetS_Sock(), (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        cout << "bind error:" << strerror(errno) << endl;
        close(Socket_sock->GetS_Sock());
        return;
    }

    ret = listen(Socket_sock->GetS_Sock(), 5);
    if (ret == -1)
    {
        cout << "listen error:" << strerror(errno) << endl;
        close(Socket_sock->GetS_Sock());
        return;
    }*/
    shared_ptr<Socket> Socket_sock = make_shared<Socket>();
    if (!Socket_sock->Init())
    {
        cout << "Socket_Init error:" << strerror(errno) << endl;
        close(Socket_sock->GetS_Sock());
        return;
    }

    /*epoll_event event;
    int event_fd, event_cnt;
    epoll_event* event_num = new epoll_event[10];
    event_fd = epoll_create(1);
    if (event_fd == -1)
    {
        cout << "epoll_create error:" << strerror(errno) << endl;
        close(Socket_sock->GetS_Sock());
        return;
    }

    event.events = EPOLLIN;
    event.data.fd = Socket_sock->GetS_Sock();
    epoll_ctl(event_fd, EPOLL_CTL_ADD, Socket_sock->GetS_Sock(), &event);*/
    shared_ptr<Epoll> epoll = make_shared<Epoll>();
    if (!epoll->Init(Socket_sock->GetS_Sock(), EPOLLIN))
    {
        cout << "Epoll_Init error:" << strerror(errno) << endl;
        close(Socket_sock->GetS_Sock());
        close(epoll->GetEpoll_fd());
        return;
    }

    cout << "服务端已启动......" << endl;

    while (true)
    {
        //event_cnt = epoll_wait(event_fd, event_num, 10, 1000);
        epoll->Epoll_Wait();
        if (epoll->GetWait_Count() == -1)
        {
            cout << "Epoll_Wait error:" << strerror(errno) << endl;
            close(Socket_sock->GetS_Sock());
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
                    //socklen_t client_len = sizeof(client_addr);
                    //client_sock = accept(Socket_sock->GetS_Sock(), (struct sockaddr*)&client_addr, &client_len);
                    if (/*client_sock*/Socket_sock->Server_Accept() == -1)
                    {
                        cout << "accept error:" << strerror(errno) << endl;
                        break;
                    }

                    cout << "客户端" << /*client_sock*/Socket_sock->GetC_Sock() << "已连接！" << endl;
                    /*event.events = EPOLLIN;
                    event.data.fd = client_sock;
                    epoll_ctl(event_fd, EPOLL_CTL_ADD, client_sock, &event);*/
                    Socket_sock->Client_Add();
                    epoll->Event_Add(/*client_sock*/Socket_sock->GetC_Sock(), EPOLLIN);
                    /*client_num++;

                    for (int j = 0;j < client_num;j++)
                    {
                        if (client[j] == -1)
                        {
                            client[j] = client_sock;
                            break;
                        }
                    }*/
                }
                else
                {
                    string msg;
                    char buf[10 * 1024] = { 0 };
                    size_t rlen = read(epoll->GetEvents_fd(i), &buf, sizeof(buf));
                    if (rlen == -1)
                    {
                        cout << "read error:" << strerror(errno) << endl;
                        break;
                    }
                    msg += buf;

                    Json::Reader reader;
                    Json::Value root;
                    reader.parse(msg, root);

                    if (msg == "0" || msg.length() == 0)
                    {
                        Socket_sock->Client_Del(epoll->GetEvents_fd(i));
                        /*for (int j = 0;j < client_num;j++)
                        {
                            if (epoll->GetEvents_fd(i)==client[j])
                            {
                                client[j] = -1;
                                break;
                            }
                        }*/
                        //epoll_ctl(event_fd, EPOLL_CTL_DEL, epoll->GetEvents_fd(i), NULL);
                        epoll->Event_Del(epoll->GetEvents_fd(i));
                        close(epoll->GetEvents_fd(i));
                        //client_num--;
                        pool->enqueue(thread_disconnect, epoll->GetEvents_fd(i), Socket_sock->GetClients());
                        /*pthread_t PID = thread_i;
                        thread_i++;
                        delete_msg dm;
                        dm.client = client;
                        dm.client_num = client_num;
                        dm.client_sock = epoll->GetEvents_fd(i);
                        pthread_create(&PID, NULL, thread_disconnected, (void*)&dm);*/

                        cout << "客户端" << epoll->GetEvents_fd(i) << "断开连接！" << endl;
                        continue;
                    }
                    if (root["type"].asString() == "LOGIN")
                    {
                        pool->enqueue(thread_login, root, epoll->GetEvents_fd(i), pool, Socket_sock->GetClients());
                    }
                    else if (root["type"].asString() == "REGISTER")
                    {
                        pool->enqueue(thread_register, root, epoll->GetEvents_fd(i));
                    }
                    else if (root["type"].asString() == "TEXT")
                    {
                        cout << "客户端" << epoll->GetEvents_fd(i) << ":" << msg << endl;
                        for (int j = 0;j < /*client_num*/Socket_sock->GetClient_Count();j++)
                        {
                            /*Json::Value root;
                            root["type"] = "TEXT";
                            root["lenght"] = 100;
                            root["data"] = "你好呀，我是花火！";
                            msg += root.toStyledString();*/
                            //msg += "{\"name\":\"shuiyixin\",\"age\":21,\"sex\":\"man\"}";
                            /*int filesize = stoi(msg.substr(msg.find("##") + 2, msg.find("##", 7) - msg.find("##") - 2));
                            int x = 0;
                            x = filesize > msg.size() ? filesize : msg.size();
                            char buf[x];
                            cout << filesize << "--" << sizeof(buf) << "--" << msg.size() << endl;
                            strcpy(buf, msg.data());*/
                            mtx.lock();
                            ssize_t wlen = write(/*client[j]*/Socket_sock->GetClients_fd(j), &buf, msg.size());
                            mtx.unlock();
                            if (wlen == -1)
                            {
                                cout << "write error:" << strerror(errno) << endl;
                                close(epoll->GetEvents_fd(i));
                                break;
                            }
                            
                            cout << wlen << endl;
                            cout << "发给客户端" << /*client[j]*/Socket_sock->GetClients_fd(j) << ":";
                            for (int k = 0;k < msg.size();k++)
                            {
                                cout << buf[k];
                            }
                            cout << endl;
                        }
                    }
                    /*else if (msg.substr(0, 5) == "FILE:")
                    {
                        int filesize = stoi(msg.substr(msg.find("##") + 2, msg.find("##", 7) - msg.find("##") - 2));
                        string filename = msg.substr(msg.find(to_string(filesize)) + to_string(filesize).length() + 2, msg.find("##", msg.find(to_string(filesize)) + to_string(filesize).length() + 2) - msg.find(to_string(filesize)) - to_string(filesize).length() - 2);
                        cout << filename << endl;
                        ifstream ifs;
                        string mulu;
                        ifs.open("/home/newftp/file/mulu.txt", ios::in);
                        ifs >> mulu;
                        cout << mulu << endl;
                        ifs.close();
                        if (mulu.find(filename) == -1)
                        {
                            ofstream ofs;
                            ofs.open("/home/newftp/file/mulu.txt", ios::app);
                            ofs << filename+"#@#@";`
                            ofs.close();
                        }
                    }*/
                    /*else if (msg.substr(0, 5) == "DLYZ:" || msg.substr(0, 5) == "DLZC:")
                    {
                        pthread_t PID = thread_i;
                        thread_i++;
                        thread_msg tm;
                        strcpy(tm.msg, msg.data());
                        tm.client_sock = epoll->GetEvents_fd(i);
                        tm.client = client;
                        tm.client_num = client_num;
                        pthread_create(&PID, NULL, thread_yanzheng, (void*)&tm);
                    }*/
                    /*else if (msg.substr(0, 3) == "TO:")
                    {
                        pthread_t PID = thread_i;
                        thread_i++;
                        siliao_msg sm;
                        strcpy(sm.msg, msg.data());
                        sm.from_sock = epoll->GetEvents_fd(i);
                        pthread_create(&PID, NULL, thread_siliao, (void*)&sm);
                    }
                    else if (msg.substr(0, 6) == "VIDEO:")
                    {
                        sockaddr_in addr;
                        socklen_t addr_size = sizeof(struct sockaddr_in);
                        getpeername(epoll->GetEvents_fd(i), (struct sockaddr*)&addr, &addr_size);
                        string sendmsg = "TOVIDEO:##" + to_string(epoll->GetEvents_fd(i)) + "##" + inet_ntoa(addr.sin_addr);
                        cout << sendmsg << endl;
                        char buf[sendmsg.size()];
                        strcpy(buf, sendmsg.data());
                        mtx.lock();
                        ssize_t wlen = write(stoi(msg.substr(msg.find("##") + 2)), buf, sendmsg.size());
                        mtx.unlock();
                        if (wlen == -1)
                            cout << "write error:" << strerror(errno) << endl;
                    }
                    else if (msg.substr(0, 6) == "YESNO:")
                    {
                        string sendmsg = "V_STATE:##";
                        if (msg.substr(msg.find("@@") + 2) == "yes")
                        {
                            sockaddr_in addr;
                            socklen_t addr_size = sizeof(struct sockaddr_in);
                            getpeername(epoll->GetEvents_fd(i), (struct sockaddr*)&addr, &addr_size);
                            sendmsg += "yes##";
                            sendmsg += inet_ntoa(addr.sin_addr);
                        }
                        else if (msg.substr(msg.find("@@") + 2) == "no")
                        {
                            sendmsg += "no";
                        }
                        cout << sendmsg << endl;
                        char buf[sendmsg.size()];
                        strcpy(buf, sendmsg.data());
                        mtx.lock();
                        ssize_t wlen = write(stoi(msg.substr(msg.find("##") + 2, msg.find("@@") - msg.find("##") - 2)), buf, sendmsg.size());
                        mtx.unlock();
                        if (wlen == -1)
                            cout << "write error:" << strerror(errno) << endl;
                    }*/
                }
            }
        }
    }
    /*close(Socket_sock->GetS_Sock());
    close(epoll->GetEpoll_fd());*/
    //delete[]event_num;
    return;
}

int main(int argc, char* argv[])
{
    /*pthread_t PID = thread_i;
    thread_i++;
    pthread_create(&PID, NULL, thread_init, NULL);
    pthread_join(PID, NULL);*/
    thread Init(thread_init);
    if (Init.joinable())
    {
        Init.join();
    }
    Server(argv[1]);
    return 0;
}