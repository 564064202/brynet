#ifndef _DATASOCKET_H
#define _DATASOCKET_H

#include <memory>
#include <functional>
#include <deque>

#include "channel.h"

#ifdef USE_OPENSSL

#ifdef  __cplusplus
extern "C" {
#endif
#include "openssl/ssl.h"
#ifdef  __cplusplus
}
#endif

#endif

class EventLoop;
struct buffer_s;

class DataSocket : public Channel
{
public:
    typedef std::function<int (DataSocket*, const char* buffer, int len)>    DATA_HANDLE;
    typedef std::function<void(DataSocket*)>                                 DISCONNECT_HANDLE;
    typedef std::shared_ptr<std::string>                                     PACKET_PTR;

public:
    DataSocket(int fd);
    ~DataSocket();

    void                            send(const char* buffer, int len);

    void                            sendPacket(const PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&);
    void                            sendPacket(PACKET_PTR&&);

    void                            setDataHandle(DATA_HANDLE proc);
    void                            setDisConnectHandle(DISCONNECT_HANDLE proc);

    /*����(����)�Ͽ�����*/
    void                            postDisConnect();

    void                            setUserData(int64_t value);
    int64_t                         getUserData() const;

#ifdef USE_OPENSSL
    void                            setupAcceptSSL(SSL_CTX*);
    void                            setupConnectSSL();
#endif

    static  PACKET_PTR              makePacket(const char* buffer, int len);
private:
    void                            setNoBlock() override;
    void                            setEventLoop(EventLoop* el) override;

    void                            canRecv() override;
    void                            canSend() override;

    bool                            checkRead();
    bool                            checkWrite();

    void                            recv();
    void                            flush();
    void                            normalFlush();
    void                            quickFlush();

    /*�˺�����ҪΪ����windows�³��Ե���onClose*/
    void                            tryOnClose();
    void                            onClose() override;
    void                            _procCloseSocket();

    void                            runAfterFlush();

    void                            freeSendPacketList();
#ifdef PLATFORM_LINUX
    void                            removeCheckWrite();
#endif

private:

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    struct ovl_ext_s
    {
        OVERLAPPED  base;
    };
    struct ovl_ext_s                mOvlRecv;
    struct ovl_ext_s                mOvlSend;
    struct ovl_ext_s                mOvlClose;

    bool                            mPostRecvCheck;     /*  �Ƿ�Ͷ���˿ɶ����   */
    bool                            mPostWriteCheck;    /*  �Ƿ�Ͷ���˿�д���   */
    bool                            mPostClose;         /*  �Ƿ�Ͷ���˶Ͽ�socket��(ģ��)���֪ͨ  */
#endif

    int                             mFD;
    EventLoop*                      mEventLoop;

    buffer_s*                       mRecvBuffer;

    struct pending_packet
    {
        PACKET_PTR  packet;
        size_t      left;
    };

    typedef std::deque<pending_packet>   PACKET_LIST_TYPE;
    PACKET_LIST_TYPE                mSendList;          /*  ������Ϣ�б�  */

    bool                            mCanWrite;          /*  socket�Ƿ��д  */

    DATA_HANDLE                     mDataHandle;
    DISCONNECT_HANDLE               mDisConnectHandle;

    bool                            mIsPostFlush;       /*  �Ƿ��Ѿ�����flush��Ϣ�Ļص�    */

    int64_t                         mUserData;          /*  ���ӵ��û��Զ�������  */

#ifdef USE_OPENSSL
    SSL_CTX*                        mSSLCtx;
    SSL*                            mSSL;
#endif
};

#endif