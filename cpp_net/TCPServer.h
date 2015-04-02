#ifndef _TCP_SERVER_H
#define _TCP_SERVER_H

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <assert.h>
#include <stdint.h>

#include "datasocket.h"
#include "typeids.h"

class EventLoop;
class DataSocket;

class ListenThread
{
public:
    typedef std::function<void(int fd)> ACCEPT_CALLBACK;
    ListenThread();
    ~ListenThread();

    /*  ���������߳�  */
    void                                startListen(int port, const char *certificate, const char *privatekey, ACCEPT_CALLBACK callback);
    void                                closeListenThread();
#ifdef USE_OPENSSL
    SSL_CTX*                            getOpenSSLCTX();
#endif
private:
    void                                RunListen();
    void                                initSSL();
    void                                destroySSL();
private:
    ACCEPT_CALLBACK                     mAcceptCallback;
    int                                 mPort;
    bool                                mRunListen;
    std::thread*                        mListenThread;
    std::string                         mCertificate;
    std::string                         mPrivatekey;
#ifdef USE_OPENSSL
    SSL_CTX*                            mOpenSSLCTX;
#endif
};

class TcpServer
{
    typedef std::function<void (EventLoop&)>                            FRAME_CALLBACK;
    typedef std::function<void(int64_t, std::string)>                   CONNECTION_ENTER_HANDLE;
    typedef std::function<void(int64_t)>                                DISCONNECT_HANDLE;
    typedef std::function<int (int64_t, const char* buffer, int len)>   DATA_HANDLE;

public:
    TcpServer();
    ~TcpServer();

    /*  ����Ĭ���¼��ص�    */
    void                                setEnterHandle(TcpServer::CONNECTION_ENTER_HANDLE handle);
    void                                setDisconnectHandle(TcpServer::DISCONNECT_HANDLE handle);
    void                                setMsgHandle(TcpServer::DATA_HANDLE handle);

    TcpServer::CONNECTION_ENTER_HANDLE  getEnterHandle();
    TcpServer::DISCONNECT_HANDLE        getDisConnectHandle();
    TcpServer::DATA_HANDLE              getDataHandle();

    void                                send(int64_t id, DataSocket::PACKET_PTR&& packet);
    void                                send(int64_t id, DataSocket::PACKET_PTR& packet);

    /*�����Ͽ���id���ӣ�����Ȼ���յ���id�ĶϿ��ص�����Ҫ�ϲ��߼��Լ��������"����"(����ͳһ�ڶϿ��ص�������������ȹ���) */
    void                                disConnect(int64_t id);

    /*  �������һ�����ӵ�����IOLoop��ָ���¼��ص�, TODO::DataSocket* channel���������ڹ���   */
    void                                addDataSocket(int fd, DataSocket* channel, 
                                                        TcpServer::CONNECTION_ENTER_HANDLE enterHandle,
                                                        TcpServer::DISCONNECT_HANDLE disHandle,
                                                        TcpServer::DATA_HANDLE msgHandle);

    /*  ���������߳�  */
    void                                startListen(int port, const char *certificate = nullptr, const char *privatekey = nullptr);
    /*  ����IO�����߳�    */
    void                                startWorkerThread(int threadNum, FRAME_CALLBACK callback = nullptr);

    /*  �رշ���    */
    void                                closeService();
    void                                closeListenThread();
    void                                closeWorkerThread();

    /*wakeupĳid���ڵ����繤���߳�*/
    void                                wakeup(int64_t id);
    /*wakeup ���е����繤���߳�*/
    void                                wakeupAll();

    EventLoop*                          getRandomEventLoop();
    
private:
    int64_t                             MakeID(int loopIndex);

    void                                _procDataSocketClose(DataSocket*);

private:
    EventLoop*                          mLoops;
    std::thread**                       mIOThreads;
    int                                 mLoopNum;
    bool                                mRunIOLoop;

    ListenThread                        mListenThread;

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