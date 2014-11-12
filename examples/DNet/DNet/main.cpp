#include <functional>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "eventloop.h"
#include "datasocket.h"

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
            if (EINTR == GetLastError())
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

int main()
{
    double total_recv_len = 0;
    double  packet_num = 0;

    WSADATA g_WSAData;
    WSAStartup(MAKEWORD(2, 2), &g_WSAData);

    Rwlist<NetMsg*>  msgList;
    EventLoop       mainLoop;

    /*  TODO::��ô�Ż���Ϣ���У������û�в��ϵ�flush��Ϣ���У�Ȼ���ֲ�����ÿ����Ϣ���Ͷ�ǿ��flush����Ȼ�߼���Ҳ���Բ�ȡ�������ķ�ʽ��ȥǿ��������   */
    /*  TODO::��(ÿ��IO�߳�һ����Ϣ����)��Ϣ�������ص�TCPServer�С� ���û����ɼ�    */
    /*�߼��̶߳�DataSocket���ɼ������ǲ���idͨ��(ҲҪȷ��������)���߼��̻߳᣺1�������ݡ�2���Ͽ����ӡ�  ����TCPServer��loopҪÿ��ѭ��������Ϣ����*/
    TcpServer t(8888, 1);
    t.setEnterHandle([&](Channel* c){
        printf("enter client \n");
        /*
                NetMsg* msg = new NetMsg(1, c);
                msgList.Push(msg);
                mainLoop.wakeup();
                */
    });

    t.setDisconnectHandle([](DataSocket* c){
        printf("client dis connect \n");
        delete c;

    });

    t.setMsgHandle([&](DataSocket* d, const char* buffer, int len){
        //printf("recv data \n");
        /*
        NetMsg* msg = new NetMsg(2, d);
        msg->setData(buffer, len);
        msgList.Push(msg);
        mainLoop.wakeup();
        */
        d->send(buffer, len);
        total_recv_len += len;
        packet_num++;
    });

    /*  �ͻ���IO�߳�   */
    for (int i = 0; i < 1; ++i)
    {
        new std::thread([&mainLoop]{
            /*  �ͻ���eventloop*/
            EventLoop clientEventLoop;

            /*  ��Ϣ����С���� */
#define PACKET_LEN (1)
#define CLIENT_NUM (1000)

            const char* senddata = (const char*)malloc(PACKET_LEN);

            for (int i = 0; i < CLIENT_NUM; i++)
            {
                int client = ox_socket_connect("127.0.0.1", 8888);
                int flag = 1;
                int setret = setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                cout << setret << endl;

                clientEventLoop.addConnection(client, new DataSocket(client), [&](Channel* arg){
                    DataSocket* ds = static_cast<DataSocket*>(arg);
                    ds->setEventLoop(&clientEventLoop);

                    /*  ������Ϣ    */
                    ds->send(senddata, PACKET_LEN);

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
    DWORD lasttime = GetTickCount();
    int total_fps = 0;

    while (true)
    {
        mainLoop.loop(10);
        total_fps++;
        msgList.ForceSyncWrite();
        msgList.SyncRead(0);

        NetMsg* msg = NULL;
        while (msgList.ReadListSize() > 0)
        {
            msg = msgList.PopBack();
            delete msg;
        }

        DWORD now = GetTickCount();
        if ((now - lasttime) >= 1000)
        {
            cout << "recv :" << (total_recv_len / 1024) / 1024 << " M/s, " << "packet num: " << packet_num  << endl;
            cout << "fps:" << total_fps << endl;
            total_fps = 0;
            lasttime = now;
            total_recv_len = 0;
            packet_num = 0;
        }
    }
}