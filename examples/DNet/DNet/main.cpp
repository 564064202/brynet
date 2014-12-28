#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "eventloop.h"
#include "datasocket.h"
#include "platform.h"
#include "socketlibtypes.h"
#include "systemlib.h"

int
ox_socket_connect(const char* server_ip, int port)
{
    struct sockaddr_in server_addr;
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if (clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_ip);
        server_addr.sin_port = htons(port);

        while (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0)
        {
            if (EINTR == sErrno)
            {
                continue;
            }
        }
    }

    return clientfd;
}

#include "TCPServer.h"
#include "rwlist.h"

struct NetMsg
{
    NetMsg(int t, Channel* c) : mType(t), mChannel(c)
    {
    }

    void        setData(const char* data, int len)
    {
        mData = string(data, len);
    }

    int         mType;
    Channel*    mChannel;
    string      mData;
};
#include <chrono>
#include <thread>
int main()
{
    double total_recv_len = 0;
    double  packet_num = 0;

    int client_num;
    int thread_num;
    int packet_len;

    cout << "enter client num:";
    cin >> client_num;

    cout << "enter thread num:";
    cin >> thread_num;

    cout << "enter packet len:";
    cin >> packet_len;
#ifdef PLATFORM_WINDOWS
    WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2, 2), &g_WSAData);
#else
#endif

    std::mutex                      mFlagMutex;
    std::unique_lock<std::mutex>    mFlagLock(mFlagMutex, std::defer_lock);
    int total_client_num = 0;

    Rwlist<NetMsg*>  msgList;
    EventLoop       mainLoop;

    /*  TODO::��ô�Ż���Ϣ���У������û�в��ϵ�flush��Ϣ���У�Ȼ���ֲ�����ÿ����Ϣ���Ͷ�ǿ��flush����Ȼ�߼���Ҳ���Բ�ȡ�������ķ�ʽ��ȥǿ��������   */
    /*  TODO::��(ÿ��IO�߳�һ����Ϣ����)��Ϣ�������ص�TCPServer�С� ���û����ɼ�    */
    /*�߼��̶߳�DataSocket���ɼ������ǲ���idͨ��(ҲҪȷ��������)���߼��̻߳᣺1�������ݡ�2���Ͽ����ӡ�  ����TCPServer��loopҪÿ��ѭ��������Ϣ����*/
    TcpServer t(5999, thread_num, [](EventLoop& l){
    });
    t.setEnterHandle([&](int64_t id){
        printf("%lld enter client \n", id);
        mFlagMutex.lock();
        total_client_num++;
        mFlagMutex.unlock();
    });

    t.setDisconnectHandle([&](int64_t id){
        printf("client %lld dis connect \n", id);
    });

    t.setMsgHandle([&](int64_t id, const char* buffer, int len){
        t.send(id, DataSocket::makePacket(buffer, len));
        total_recv_len += len;
        packet_num++;
    });

    /*  �ͻ���IO�߳�   */
    for (int i = 0; i < thread_num; ++i)
    {
        new std::thread([&mainLoop, &msgList, &client_num, &packet_len]{
            printf("start one client thread \n");
            /*  �ͻ���eventloop*/
            EventLoop clientEventLoop;

            /*  ��Ϣ����С���� */
            const char* senddata = (const char*)malloc(packet_len);

            for (int i = 0; i < client_num; i++)
            {
                int client = ox_socket_connect("127.0.0.1", 5999);
                if (client == SOCKET_ERROR)
                {
                    printf("error : %d \n", sErrno);
                }
                printf("connect fd: %d\n", client);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                cout << setret << endl;

                clientEventLoop.addConnection(client, new DataSocket(client), [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    /*  ������Ϣ    */
                    ds->send(senddata, packet_len);

                    /*  ���Է�����Ϣ���У�Ȼ���������̵߳�eventloop��Ȼ�����߳�ͨ����Ϣ����ȥ��ȡ*/
                    ds->setDataHandle([](DataSocket* ds, const char* buffer, int len){
                        ds->send(buffer, len);
                    });
                });
            }

            while (true)
            {
                clientEventLoop.loop(-1);
            }
        });
    }

    /*  ���̴߳����������IO*/
    int64_t lasttime = ox_getnowtime();

    while (true)
    {
        mainLoop.loop(10);

        int64_t now = ox_getnowtime();
        if ((now - lasttime) >= 1000)
        {
            cout << "recv by clientnum:" << total_client_num << " of :" << (total_recv_len / 1024) / 1024 << " M / s, " << "packet num : " << packet_num  << endl;
            lasttime = now;
            total_recv_len = 0;
            packet_num = 0;
        }
    }
}