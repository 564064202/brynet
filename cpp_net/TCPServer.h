#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include <vector>
#include <assert.h>

#include "typeids.h"

class EventLoop;
class DataSocket;

class TcpServer
{
    typedef std::function<void (EventLoop&)> FRAME_CALLBACK;
    typedef std::function<void(int64_t)>    CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>    DISCONNECT_PROC;
    typedef std::function<void(int64_t, const char* buffer, int len)>    DATA_PROC;

public:
    TcpServer(int port, int threadNum, FRAME_CALLBACK callback = nullptr);  /*callbackΪIO�߳�ÿ��loopѭ������ִ�еĻص�����������Ϊnull*/
    ~TcpServer();

    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_PROC handle);
    void                                setMsgHandle(TcpServer::DATA_PROC handle);

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*�����Ͽ���id���ӣ�����Ȼ�����յ���id�ĶϿ��ص�����Ҫ�ϲ��߼��Լ��������"����"*/
    void                                disConnect(int64_t id);

private:
    void                                RunListen(int port);
    int64_t                             MakeID(int loopIndex);

    void                                _procDataSocketClose(DataSocket*);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    std::thread*                        mListenThread;

    TypeIDS<DataSocket>*                mIds;
    int*                                mIncIds;

    TcpServer::CONNECTION_ENTER_HANDLE  mEnterHandle;
    TcpServer::DISCONNECT_PROC          mDisConnectHandle;
    TcpServer::DATA_PROC                mDataProc;

    union SessionId
    {
        struct
        {
            int8_t loopIndex;
            int32_t index:24;
            int32_t iid;
        }data;  /*TODO::so,���������֧��0x7f��io loop�̣߳�ÿһ��io loop���֧��0x7fffff�����ӡ�*/

        int64_t id;
    };
};

#endif