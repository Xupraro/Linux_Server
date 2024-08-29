#include <cstdio>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
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

void* thread_init(void* args)
{
    MYSQL mysql_conn;
    MYSQL* mysql = mysql_init(&mysql_conn);
    if (mysql == NULL)
    {
        printf("mysql init err\n");
        pthread_exit((void*)111);
    }
    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
    {
        printf("connect err\n");
        pthread_exit((void*)111);
    }
    string sql = "update usermsg set epoll_id = -1 where epoll_id != -1;";
    //cout << "SQL:" << sql.c_str() << endl;
    if (mysql_query(mysql, sql.c_str()) == 0);
        //cout << "update sceeuss！" << endl;
    cout << "init线程结束！" << endl;
}

void* thread_userdata(void* args)
{
    pthread_detach(pthread_self());
    thread_msg* tm = (thread_msg*)args;
    int cn = tm->client_num;
    MYSQL mysql_conn;
    MYSQL* mysql = mysql_init(&mysql_conn);
    if (mysql == NULL)
    {
        printf("mysql init err\n");
        pthread_exit((void*)111);
    }
    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
    {
        printf("connect err\n");
        pthread_exit((void*)111);
    }
    string sql = "select name,epoll_id from usermsg where epoll_id != -1;";
    if (mysql_query(mysql, sql.c_str()) == 0);
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res == NULL)
    {
        printf("get err！\n");
        mysql_close(mysql);
        pthread_exit((void*)111);
    }
    int hang = mysql_num_rows(res);
    int lie = mysql_num_fields(res);
    for (int i = 0;i < hang;i++)
    {
        MYSQL_ROW r = mysql_fetch_row(res);
        string sendmsg = "USER:";
        for (int j = 0;j < lie;j++)
        {
            sendmsg += "##";
            sendmsg += r[j];
        }
        char buf[sendmsg.size()];
        strcpy(buf, sendmsg.data());
        for (int j = 0;j < cn;j++)
        {
            if (tm->client[j] != -1)
            {
                if (r[1] != to_string(tm->client[j]))
                {
                    mtx.lock();
                    ssize_t wlen = write(tm->client[j], &buf, sendmsg.size());
                    mtx.unlock();
                    if (wlen == -1)
                        cout << "write error:" << strerror(errno) << endl;
                    struct timeval tv;
                    tv.tv_sec = 0;
                    tv.tv_usec = 201 * 1000;
                    select(0, NULL, NULL, NULL, &tv);
                }
            }
            else if (tm->client[j] == -1)
                cn++;
        }
    }
    mysql_free_result(res);
    cout << "userdata线程结束！" << endl;
}

void* thread_disconnected(void* args)
{
    pthread_detach(pthread_self());
    delete_msg* dm = (delete_msg*)args;
    int cn = dm->client_num;
    MYSQL mysql_conn;
    MYSQL* mysql = mysql_init(&mysql_conn);
    if (mysql == NULL)
    {
        printf("mysql init err\n");
        pthread_exit((void*)111);
    }
    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
    {
        printf("connect err\n");
        pthread_exit((void*)111);
    }
    string sql = "update usermsg set epoll_id = -1 where epoll_id = " + to_string(dm->client_sock) + ";";
    if (mysql_query(mysql, sql.c_str()) == 0);
    mysql_close(mysql);
    string sendmsg = "DELETE:##" + to_string(dm->client_sock);
    char buf[sendmsg.size()];
    strcpy(buf, sendmsg.data());
    for (int i = 0;i < cn;i++)
    {
        if (dm->client[i] != -1)
        {
            mtx.lock();
            ssize_t wlen = write(dm->client[i], &buf, sendmsg.size());
            mtx.unlock();
            if (wlen == -1)
                cout << "write error:" << strerror(errno) << endl;
        }
        else if (dm->client[i] == -1)
            cn++;
    }

    cout << "disconnected线程结束!" << endl;
}

void* thread_yanzheng(void* args)
{
    pthread_detach(pthread_self());
    string sql;
    string name;
    string sendmsg;
    thread_msg* tm = (thread_msg*)args;
    string msg = tm->msg;
    if (msg.substr(0, 5) == "DLZC:")
    {
        name = msg.substr(msg.find("$$") + 2);
    }
    string user = msg.substr(msg.find("##") + 2, msg.find("@@") - msg.find("##") - 2);
    string password = msg.substr(msg.find("@@") + 2, msg.find("$$") - msg.find("@@") - 2);

    MYSQL mysql_conn;
    MYSQL* mysql = mysql_init(&mysql_conn);
    if (mysql == NULL)
    {
        printf("mysql init err\n");
        pthread_exit((void*)111);
    }
    if (mysql_real_connect(mysql, "127.0.0.1", "root", "0", "liaotianshi", 3306, NULL, 0) == NULL)
    {
        printf("connect err\n");
        pthread_exit((void*)111);
    }
    if (mysql_query(mysql, "create table if not exists usermsg(user int primary key,password varchar(20) not null,name varchar(20) not null,epoll_id int not null);") == 0)
    {
        //cout << "表usermsg创建成功！" << endl;
    }
    else
    {
        printf("create err\n");
        mysql_close(mysql);
        pthread_exit((void*)111);
    }
    sql = "select password from usermsg where user = " + user + ";";
    if (mysql_query(mysql, sql.c_str()) != 0)
    {
        cout << "select err！" << endl;
        mysql_close(mysql);
        pthread_exit((void*)111);
    }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res == NULL)
    {
        printf("get err！\n");
        mysql_close(mysql);
        pthread_exit((void*)111);
    }
    MYSQL_ROW r = mysql_fetch_row(res);
    
    if (r != NULL)
    {
        if (msg.substr(0, 5) == "DLZC:")
            sendmsg = "TOKEN:##CUNZAI";
        else if (msg.substr(0, 5) == "DLYZ:")
        {
            if (password == r[0])
            {
                sendmsg = "TOKEN:##TRUE";
                sql = "update usermsg set epoll_id = " + to_string(tm->client_sock) + " where user = " + user + ";";
                if (mysql_query(mysql, sql.c_str()) == 0);
                sql = "select name from usermsg where user = " + user + ";";
                if (mysql_query(mysql, sql.c_str()) == 0);
                res = mysql_store_result(mysql);
                MYSQL_ROW n = mysql_fetch_row(res);
                sendmsg += "##";
                sendmsg += n[0];
                pthread_t PID = thread_i;
                thread_i++;
                pthread_create(&PID, NULL, thread_userdata, (void*)tm);
            }
            else
                sendmsg = "TOKEN:##FALSE";
        }
    }
    else
    {
        if(msg.substr(0,5)=="DLYZ:")
            sendmsg = "TOKEN:##NULL";
        else if (msg.substr(0, 5) == "DLZC:")
        {
            sql = "insert into usermsg values(" + user + ",'" + password + "','" + name + "',-1);";
            if (mysql_query(mysql, sql.c_str()) == 0);
            sendmsg = "TOKEN:##CHENGGONG";
        }
    }
    mysql_free_result(res);
    mysql_close(mysql);

    char buf[sendmsg.size()];
    strcpy(buf, sendmsg.data());
    mtx.lock();
    ssize_t wlen = write(tm->client_sock, &buf, sendmsg.size());
    mtx.unlock();
    if (wlen == -1)
        cout << "write error:" << strerror(errno) << endl;
    
    cout << "yanzheng线程结束！" << endl;
}

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
    int server_sock, client_sock, client_num = 0;
    int* client = new int[1024];
    for (int i = 0;i < 1024;i++)
        client[i] = -1;
    sockaddr_in server_addr, client_addr;

    server_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
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

    int ret = bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        cout << "bind error:" << strerror(errno) << endl;
        close(server_sock);
        return;
    }

    ret = listen(server_sock, 5);
    if (ret == -1)
    {
        cout << "listen error:" << strerror(errno) << endl;
        close(server_sock);
        return;
    }

    epoll_event event;
    int event_fd, event_cnt;
    epoll_event* event_num = new epoll_event[10];
    event_fd = epoll_create(1);
    if (event_fd == -1)
    {
        cout << "epoll_create error:" << strerror(errno) << endl;
        close(server_sock);
        return;
    }

    event.events = EPOLLIN;
    event.data.fd = server_sock;
    epoll_ctl(event_fd, EPOLL_CTL_ADD, server_sock, &event);

    cout << "服务端已启动......" << endl;

    while (true)
    {
        event_cnt = epoll_wait(event_fd, event_num, 10, 1000);
        if (event_cnt == -1)
        {
            cout << "epoll_wait error:" << strerror(errno) << endl;
            close(server_sock);
            return;
        }
        else if (event_cnt == 0)
        {
            continue;
        }
        else if (event_cnt > 0)
        {
            for (int i = 0;i < event_cnt;i++)
            {
                if (event_num[i].data.fd == server_sock)
                {
                    socklen_t client_len = sizeof(client_addr);
                    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
                    if (client_sock == -1)
                    {
                        cout << "accept error:" << strerror(errno) << endl;
                        break;
                    }

                    cout << "客户端" << client_sock << "已连接！" << endl;
                    event.events = EPOLLIN;
                    event.data.fd = client_sock;
                    epoll_ctl(event_fd, EPOLL_CTL_ADD, client_sock, &event);
                    client_num++;

                    for (int j = 0;j < client_num;j++)
                    {
                        if (client[j] == -1)
                        {
                            client[j] = client_sock;
                            break;
                        }
                    }
                }
                else
                {
                    string msg;
                    char buf[10 * 1024] = { 0 };
                    size_t rlen = read(event_num[i].data.fd, &buf, sizeof(buf));
                    if (rlen == -1)
                    {
                        cout << "read error:" << strerror(errno) << endl;
                        break;
                    }
                    msg += buf;

                    if (msg == "0" || msg.length() == 0)
                    {
                        for (int j = 0;j < client_num;j++)
                        {
                            if (event_num[i].data.fd==client[j])
                            {
                                client[j] = -1;
                                break;
                            }
                        }
                        epoll_ctl(event_fd, EPOLL_CTL_DEL, event_num[i].data.fd, NULL);
                        close(event_num[i].data.fd);
                        client_num--;

                        pthread_t PID = thread_i;
                        thread_i++;
                        delete_msg dm;
                        dm.client = client;
                        dm.client_num = client_num;
                        dm.client_sock = event_num[i].data.fd;
                        pthread_create(&PID, NULL, thread_disconnected, (void*)&dm);

                        cout << "客户端" << event_num[i].data.fd << "断开连接！" << endl;
                        continue;
                    }
                    else if (msg.substr(0, 5) == "TEXT:")
                    {
                        cout << "客户端" << event_num[i].data.fd << ":" << msg << endl;
                        for (int j = 0;j < client_num;j++)
                        {
                            int filesize = stoi(msg.substr(msg.find("##") + 2, msg.find("##", 7) - msg.find("##") - 2));
                            int x = 0;
                            x = filesize > msg.size() ? filesize : msg.size();
                            char buf[x];
                            cout << filesize << "--" << sizeof(buf) << "--" << msg.size() << endl;
                            strcpy(buf, msg.data());
                            mtx.lock();
                            ssize_t wlen = write(client[j], &buf, msg.size());
                            mtx.unlock();
                            if (wlen == -1)
                            {
                                cout << "write error:" << strerror(errno) << endl;
                                close(event_num[i].data.fd);
                                break;
                            }
                            
                            cout << wlen << endl;
                            cout << "发给客户端" << client[j] << ":";
                            for (int k = 0;k < msg.size();k++)
                            {
                                cout << buf[k];
                            }
                            cout << endl;
                        }
                    }
                    else if (msg.substr(0, 5) == "FILE:")
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
                            ofs << filename+"#@#@";
                            ofs.close();
                        }
                    }
                    else if (msg.substr(0, 5) == "DLYZ:" || msg.substr(0, 5) == "DLZC:")
                    {
                        pthread_t PID = thread_i;
                        thread_i++;
                        thread_msg tm;
                        strcpy(tm.msg, msg.data());
                        tm.client_sock = event_num[i].data.fd;
                        tm.client = client;
                        tm.client_num = client_num;
                        pthread_create(&PID, NULL, thread_yanzheng, (void*)&tm);
                    }
                    else if (msg.substr(0, 3) == "TO:")
                    {
                        pthread_t PID = thread_i;
                        thread_i++;
                        siliao_msg sm;
                        strcpy(sm.msg, msg.data());
                        sm.from_sock = event_num[i].data.fd;
                        pthread_create(&PID, NULL, thread_siliao, (void*)&sm);
                    }
                    else if (msg.substr(0, 6) == "VIDEO:")
                    {
                        sockaddr_in addr;
                        socklen_t addr_size = sizeof(struct sockaddr_in);
                        getpeername(event_num[i].data.fd, (struct sockaddr*)&addr, &addr_size);
                        string sendmsg = "TOVIDEO:##" + to_string(event_num[i].data.fd) + "##" + inet_ntoa(addr.sin_addr);
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
                            getpeername(event_num[i].data.fd, (struct sockaddr*)&addr, &addr_size);
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
                    }
                }
            }
        }
    }
    close(server_sock);
    close(event_fd);
    delete[]event_num;
    return;
}

int main(int argc, char* argv[])
{
    pthread_t PID = thread_i;
    thread_i++;
    pthread_create(&PID, NULL, thread_init, NULL);
    pthread_join(PID, NULL);
    Server(argv[1]);
    return 0;
}