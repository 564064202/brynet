#include <time.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <thread>
#include <iostream>
#include <exception>

#include "platform.h"
#include "socketlibtypes.h"
#include "TCPServer.h"

int
ox_socket_listen(int port, int back_num)
{
    int socketfd = SOCKET_ERROR;
    struct  sockaddr_in server_addr;
    int reuseaddr_value = 1;

    socketfd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr_value, sizeof(int));

        if (::bind(socketfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR ||
            listen(socketfd, back_num) == SOCKET_ERROR)
        {
#ifdef PLATFORM_WINDOWS
            closesocket(socketfd);
#else
            close(socketfd);
#endif
            socketfd = SOCKET_ERROR;
        }
    }

    return socketfd;
}

TcpServer::TcpServer(int port, int threadNum, FRAME_CALLBACK callback)
{
    mLoops = new EventLoop[threadNum];
    mIOThreads = new std::thread*[threadNum];
    mLoopNum = threadNum;
    mIds = new IdTypes<DataSocket>[threadNum];
    mIncIds = new int[threadNum];
    memset(mIncIds, 0, sizeof(mIncIds[0])*threadNum);

    for (int i = 0; i < mLoopNum; ++i)
    {
        EventLoop& l = mLoops[i];
        mIOThreads[i] = new thread([&l, callback](){
            l.restoreThreadID();
            while (true)
            {
                l.loop(-1);
                if (callback != nullptr)
                {
                    callback(l);
                }
            }
        });
    }
    
    mListenThread = new std::thread([this, port](){
        RunListen(port);
    });
}

TcpServer::~TcpServer()
{
    for (int i = 0; i < mLoopNum; ++i)
    {
        mIOThreads[i]->join();
        delete mIOThreads[i];
    }

    delete[] mIOThreads;

    mListenThread->join();
    delete mListenThread;
    mListenThread = NULL;

    delete[] mLoops;
    mLoops = NULL;
}

void TcpServer::setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle)
{
    mEnterHandle = handle;
}

void TcpServer::setDisconnectHandle(TcpServer::DISCONNECT_PROC handle)
{
    mDisConnectHandle = handle;
}

void TcpServer::setMsgHandle(TcpServer::DATA_PROC handle)
{
    mDataProc = handle;
}

void TcpServer::send(int64_t id, DataSocket::PACKET_PTR&& packet)
{
    send(id, packet);
}

void TcpServer::send(int64_t id, DataSocket::PACKET_PTR& packet)
{
    union  SessionId sid;
    sid.id = id;

    if (mIOThreads[sid.data.loopIndex]->get_id() != std::this_thread::get_id())
    {
        /*TODO::pushAsyncProc��Ч�����⡣δ֪ */
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid, packet](){

            DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
            if (ds != nullptr && ds->getUserData() == sid.id)
            {
                ds->sendPacket(packet);
            }

        });
    }
    else
    {
        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->sendPacket(packet);
        }
    }
}

void TcpServer::disConnect(int64_t id)
{
    union  SessionId sid;
    sid.id = id;

    if (mIOThreads[sid.data.loopIndex]->get_id() != std::this_thread::get_id())
    {
        /*TODO::pushAsyncProc��Ч�����⡣δ֪ */
        mLoops[sid.data.loopIndex].pushAsyncProc([this, sid](){

            DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
            if (ds != nullptr && ds->getUserData() == sid.id)
            {
                ds->disConnect();
                _procDataSocketClose(ds);
            }

        });
    }
    else
    {
        DataSocket* ds = mIds[sid.data.loopIndex].get(sid.data.index);
        if (ds != nullptr && ds->getUserData() == sid.id)
        {
            ds->disConnect();
            _procDataSocketClose(ds);
        }
    }
}
int64_t TcpServer::MakeID(int loopIndex)
{
    union SessionId sid;
    sid.data.loopIndex = loopIndex;
    sid.data.index = mIds[loopIndex].claimID();
    sid.data.iid = mIncIds[loopIndex]++;

    return sid.id;
}

void TcpServer::_procDataSocketClose(DataSocket* ds)
{
    int64_t id = ds->getUserData();
    union SessionId sid;
    sid.id = id;

    mIds[sid.data.loopIndex].set(nullptr, sid.data.index);
    mIds[sid.data.loopIndex].reclaimID(sid.data.index);
    delete ds;
}

void TcpServer::RunListen(int port)
{
    while (true)
    {
        int client_fd = SOCKET_ERROR;
        struct sockaddr_in socketaddress;
        socklen_t size = sizeof(struct sockaddr);

        int listen_fd = ox_socket_listen(port, 25);

        if (SOCKET_ERROR != listen_fd)
        {
            printf("listen : %d \n", port);
            for (;;)
            {
                while ((client_fd = accept(listen_fd, (struct sockaddr*)&socketaddress, &size)) < 0)
                {
                    if (EINTR == sErrno)
                    {
                        continue;
                    }
                }

                printf("accept fd : %d \n", client_fd);

                if (SOCKET_ERROR != client_fd)
                {
                    int flag = 1;
                    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
                    {
                        int sd_size = 32 * 1024;
                        int op_size = sizeof(sd_size);
                        setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, (const char*)&sd_size, op_size);
                    }

                    Channel* channel = new DataSocket(client_fd);
                    int loopIndex = rand() % mLoopNum;
                    EventLoop& loop = mLoops[loopIndex];
                    loop.addConnection(client_fd, channel, [this, loopIndex](Channel* arg){
                        DataSocket* ds = static_cast<DataSocket*>(arg);

                        int64_t id = MakeID(loopIndex);
                        union SessionId sid;
                        sid.id = id;
                        mIds[loopIndex].set(ds, sid.data.index);
                        ds->setUserData(id);
                        ds->setDataHandle([this](DataSocket* ds, const char* buffer, int len){
                            mDataProc(ds->getUserData(), buffer, len);
                        });

                        ds->setDisConnectHandle([this](DataSocket* arg){
                            /*TODO::����Ҳ��Ҫһ��ʼ�ͱ���thisָ�룬 delete datasocket֮�󣬴�lambda�������󶨱���ʧЧ����������������this�ͻ�崻�*/
                            TcpServer* p = this;
                            int64_t id = arg->getUserData();
                            p->_procDataSocketClose(arg);
                            p->mDisConnectHandle(id);
                        });

                        mEnterHandle(id);
                    });
                }
            }

#ifdef PLATFORM_WINDOWS
            closesocket(listen_fd);
#else
            close(listen_fd);
#endif
            listen_fd = SOCKET_ERROR;
        }
        else
        {
            printf("listen failed, error:%d \n", sErrno);
            return;
        }
    }
}