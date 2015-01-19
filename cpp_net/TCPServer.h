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
    typedef std::function<void(int64_t)>    DISCONNECT_HANDLE;
    typedef std::function<void(int64_t, const char* buffer, int len)>    DATA_HANDLE;

public:
    TcpServer(int port, int threadNum, FRAME_CALLBACK callback = nullptr);  /*callbackΪIO�߳�ÿ��loopѭ������ִ�еĻص�����������Ϊnull*/
    ~TcpServer();

    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_HANDLE handle);
    void                                setMsgHandle(TcpServer::DATA_HANDLE handle);

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

    /*  ���������ص��������ڶ��߳��е���(ÿ���̼߳�һ��eventloop������io loop)(����RunListen�е�ʹ��)   */
    TcpServer::CONNECTION_ENTER_HANDLE  mEnterHandle;
    TcpServer::DISCONNECT_HANDLE        mDisConnectHandle;
    TcpServer::DATA_HANDLE              mDataHandle;

    /*  �˽ṹ���ڱ�ʾһ���ػ����߼��̺߳������߳�ͨ����ͨ���˽ṹ�Իػ�������ز���(������ֱ�Ӵ���Channel/DataSocketָ��)  */
    
    union SessionId
    {
        struct
        {
            uint16_t    loopIndex;      /*  �Ự������eventloop��(��mLoops�е�)����  */
            uint16_t    index;          /*  �Ự��mIds[loopIndex]�е�����ֵ */
            uint32_t    iid;            /*  ����������   */
        }data;  /*  warn::so,���������֧��0xFFFF(65536)��io loop�̣߳�ÿһ��io loop���֧��0xFFFF(65536)�����ӡ�*/

        int64_t id;
    };
    
};

#endif