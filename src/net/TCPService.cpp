#include <iostream>
#include "Typeids.h"
#include "SocketLibFunction.h"
#include "EventLoop.h"
#include "DataSocket.h"

#include "TCPService.h"

static unsigned int sDefaultLoopTimeOutMS = 100;

using namespace brynet::net;

ListenThread::PTR ListenThread::Create()
{
    struct make_shared_enabler : public ListenThread {};
    return std::make_shared<make_shared_enabler>();
}

ListenThread::ListenThread() noexcept
{
    mIsIPV6 = false;
    mAcceptCallback = nullptr;
    mPort = 0;
    mRunListen = false;
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;
#endif
}

ListenThread::~ListenThread() noexcept
{
    closeListenThread();
}

void ListenThread::startListen(bool isIPV6, const std::string& ip, int port, const char *certificate, const char *privatekey, ACCEPT_CALLBACK callback)
{
    std::lock_guard<std::mutex> lck(mListenThreadGuard);

    if (mListenThread == nullptr)
    {
        mIsIPV6 = isIPV6;
        mRunListen = true;
        mIP = ip;
        mPort = port;
        mAcceptCallback = callback;
        if (certificate != nullptr)
        {
            mCertificate = certificate;
        }
        if (privatekey != nullptr)
        {
            mPrivatekey = privatekey;
        }

        mListenThread = std::make_shared<std::thread>([shared_this = shared_from_this()](){
            shared_this->runListen();
        });
    }
}

void ListenThread::closeListenThread()
{
    std::lock_guard<std::mutex> lck(mListenThreadGuard);

    if (mListenThread != nullptr)
    {
        mRunListen = false;

        sock tmp = ox_socket_connect(mIsIPV6, mIP.c_str(), mPort);
        ox_socket_close(tmp);
        tmp = SOCKET_ERROR;

        if (mListenThread->joinable())
        {
            mListenThread->join();
        }
        mListenThread = nullptr;
    }
}

#ifdef USE_OPENSSL
SSL_CTX* ListenThread::getOpenSSLCTX()
{
    return mOpenSSLCTX;
}
#endif

void ListenThread::initSSL()
{
#ifdef USE_OPENSSL
    mOpenSSLCTX = nullptr;

    if (!mCertificate.empty() && !mPrivatekey.empty())
    {
        mOpenSSLCTX = SSL_CTX_new(SSLv23_server_method());
        if (SSL_CTX_use_certificate_file(mOpenSSLCTX, mCertificate.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
        /* �����û�˽Կ */
        if (SSL_CTX_use_PrivateKey_file(mOpenSSLCTX, mPrivatekey.c_str(), SSL_FILETYPE_PEM) <= 0) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
        /* ����û�˽Կ�Ƿ���ȷ */
        if (!SSL_CTX_check_private_key(mOpenSSLCTX)) {
            SSL_CTX_free(mOpenSSLCTX);
            mOpenSSLCTX = nullptr;
        }
    }
#endif
}

void ListenThread::destroySSL()
{
#ifdef USE_OPENSSL
    if(mOpenSSLCTX != nullptr)
    {
        SSL_CTX_free(mOpenSSLCTX);
        mOpenSSLCTX = nullptr;
    }
#endif
}

void ListenThread::runListen()
{
    sock client_fd = SOCKET_ERROR;
    struct sockaddr_in socketaddress;
    struct sockaddr_in6 ip6Addr;
    socklen_t addrLen = sizeof(struct sockaddr);
    sockaddr_in* pAddr = &socketaddress;

    if (mIsIPV6)
    {
        addrLen = sizeof(ip6Addr);
        pAddr = (sockaddr_in*)&ip6Addr;
    }

    sock listen_fd = ox_socket_listen(mIsIPV6, mIP.c_str(), mPort, 512);
    initSSL();

    if (SOCKET_ERROR != listen_fd)
    {
        printf("listen : %d \n", mPort);
        for (; mRunListen;)
        {
            while ((client_fd = ox_socket_accept(listen_fd, (struct sockaddr*)pAddr, &addrLen)) == SOCKET_ERROR)
            {
                if (EINTR == sErrno)
                {
                    continue;
                }
            }

            if (SOCKET_ERROR != client_fd && mRunListen)
            {
                ox_socket_nodelay(client_fd);
                ox_socket_setsdsize(client_fd, 32 * 1024);
                mAcceptCallback(client_fd);
            }
        }

        ox_socket_close(listen_fd);
        listen_fd = SOCKET_ERROR;
    }
    else
    {
        printf("listen failed, error:%d \n", sErrno);
    }
}

namespace brynet
{
    namespace net
    {
        /*  �˽ṹ���ڱ�ʾһ���ػ����߼��̺߳������߳�ͨ����ͨ���˽ṹ�Իػ�������ز���(������ֱ�Ӵ���Channel/DataSocketָ��)  */
        union SessionId
        {
            struct
            {
                uint16_t    loopIndex;      /*  �Ự������eventloop��(��mLoops�е�)����  */
                uint16_t    index;          /*  �Ự��mDataSockets[loopIndex]�е�����ֵ */
                uint32_t    iid;            /*  ����������   */
            }data;  /*  warn::so,���������֧��0xFFFF(65536)��io loop�̣߳�ÿһ��io loop���֧��0xFFFF(65536)�����ӡ�*/

            TcpService::SESSION_TYPE id;
        };
    }
}

TcpService::TcpService() noexcept
{
    static_assert(sizeof(SessionId) == sizeof(((SessionId*)nullptr)->id), "sizeof SessionId must equal int64_t");

    mEnterCallback = nullptr;
    mDisConnectCallback = nullptr;
    mDataCallback = nullptr;

    mRunIOLoop = false;
    mListenThread = ListenThread::Create();
}

TcpService::~TcpService() noexcept
{
    closeService();
}

void TcpService::setEnterCallback(TcpService::ENTER_CALLBACK callback)
{
    mEnterCallback = std::move(callback);
}

void TcpService::setDisconnectCallback(TcpService::DISCONNECT_CALLBACK callback)
{
    mDisConnectCallback = std::move(callback);
}

void TcpService::setDataCallback(TcpService::DATA_CALLBACK callback)
{
    mDataCallback = std::move(callback);
}

const TcpService::ENTER_CALLBACK& TcpService::getEnterCallback() const
{
    return mEnterCallback;
}

const TcpService::DISCONNECT_CALLBACK& TcpService::getDisconnectCallback() const
{
    return mDisConnectCallback;
}

const TcpService::DATA_CALLBACK& TcpService::getDataCallback() const
{
    return mDataCallback;
}

void TcpService::send(SESSION_TYPE id, DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback) const
{
    union  SessionId sid;
    sid.id = id;
    auto ioLoopData = getIOLoopDataBySocketID(id);
    if (ioLoopData != nullptr)
    {
        ioLoopData->send(id, std::move(packet), std::move(callback));
    }
}

void TcpService::cacheSend(SESSION_TYPE id, DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback)
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex < mIOLoopDatas.size());
    if (sid.data.loopIndex < mIOLoopDatas.size())
    {
        mIOLoopDatas[sid.data.loopIndex]->getPacketList()->push_back(std::make_tuple(id, std::move(packet), std::move(callback)));
    }
}

void TcpService::flushCachePackectList()
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    for (const auto& ioLoopData : mIOLoopDatas)
    {
        if (!ioLoopData->getPacketList()->empty())
        {
            ioLoopData->getEventLoop()->pushAsyncProc([msgListCapture = std::move(ioLoopData->getPacketList()), shared_this = shared_from_this(), ioLoopDataCapture = ioLoopData](){
                for (const auto& v : *msgListCapture)
                {
                    union  SessionId sid;
                    sid.id = std::get<0>(v);
                    DataSocket::PTR tmp = nullptr;
                    if (ioLoopDataCapture->getDataSockets().get(sid.data.index, tmp))
                    {
                        if (tmp != nullptr)
                        {
                            auto ud = std::any_cast<SESSION_TYPE>(&tmp->getUD());
                            if (ud != nullptr && *ud == sid.id)
                            {
                                tmp->sendPacket(std::get<1>(v), std::get<2>(v));
                            }
                        }
                    }
                }
                
            });
            ioLoopData->resetPacketList();
        }
    }
}

void TcpService::shutdown(SESSION_TYPE id) const
{
    postSessionAsyncProc(id, [](DataSocket::PTR ds){
        ds->postShutdown();
    });
}

void TcpService::disConnect(SESSION_TYPE id) const
{
    postSessionAsyncProc(id, [](DataSocket::PTR ds){
        ds->postDisConnect();
    });
}

void TcpService::setPingCheckTime(SESSION_TYPE id, int checktime)
{
    postSessionAsyncProc(id, [checktime](DataSocket::PTR ds){
        ds->setCheckTime(checktime);
    });
}

void TcpService::postSessionAsyncProc(SESSION_TYPE id, std::function<void(DataSocket::PTR)> callback) const
{
    union  SessionId sid;
    sid.id = id;
    auto ioLoopData = getIOLoopDataBySocketID(id);
    if (ioLoopData != nullptr)
    {
        const auto& eventLoop = ioLoopData->getEventLoop();
        eventLoop->pushAsyncProc([callbackCapture = std::move(callback), sid, shared_this = shared_from_this(), ioLoopDataCapture = std::move(ioLoopData)](){
            DataSocket::PTR tmp = nullptr;
            if (callbackCapture != nullptr &&
                ioLoopDataCapture->getDataSockets().get(sid.data.index, tmp) &&
                tmp != nullptr)
            {
                auto ud = std::any_cast<SESSION_TYPE>(&tmp->getUD());
                if (ud != nullptr && *ud == sid.id)
                {
                    callbackCapture(tmp);
                }
            }
        });
    }
}

void TcpService::closeService()
{
    closeListenThread();
    closeWorkerThread();
}

void TcpService::closeListenThread()
{
    std::lock_guard<std::mutex> lck(mServiceGuard);

    if (mListenThread != nullptr)
    {
        mListenThread->closeListenThread();
    }
}

void TcpService::closeWorkerThread()
{
    stopWorkerThread();
}

void TcpService::stopWorkerThread()
{
    std::lock_guard<std::mutex> lck(mServiceGuard);
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    mRunIOLoop = false;

    for (const auto& v : mIOLoopDatas)
    {
        v->getEventLoop()->wakeup();
        if (v->getIOThread()->joinable())
        {
            v->getIOThread()->join();
        }
    }
    mIOLoopDatas.clear();
}

void TcpService::startListen(bool isIPV6, const std::string& ip, int port, int maxSessionRecvBufferSize, const char *certificate, const char *privatekey)
{
    std::lock_guard<std::mutex> lck(mServiceGuard);

    mListenThread->startListen(isIPV6, ip, port, certificate, privatekey, [maxSessionRecvBufferSize, shared_this = shared_from_this()](sock fd){
        std::string ip = ox_socket_getipoffd(fd);
        auto channel = new DataSocket(fd, maxSessionRecvBufferSize);
        bool ret = true;
#ifdef USE_OPENSSL
        if (mListenThread.getOpenSSLCTX() != nullptr)
        {
            ret = channel->initAcceptSSL(mListenThread.getOpenSSLCTX());
        }
#endif
        if (ret)
        {
            ret = shared_this->helpAddChannel(channel, ip, shared_this->mEnterCallback, shared_this->mDisConnectCallback, shared_this->mDataCallback);
        }
        
        if (!ret)
        {
            delete channel;
            channel = nullptr;
        }
    });
}

void TcpService::startWorkerThread(size_t threadNum, FRAME_CALLBACK callback)
{
    std::lock_guard<std::mutex> lck(mServiceGuard);
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    if (mIOLoopDatas.empty())
    {
        mRunIOLoop = true;

        mIOLoopDatas.resize(threadNum);
        for (auto& v : mIOLoopDatas)
        {
            auto eventLoop = std::make_shared<EventLoop>();
            v = IOLoopData::Create(eventLoop, std::make_shared<std::thread>([callback,
                shared_this = shared_from_this(),
                eventLoop]() {
                while (shared_this->mRunIOLoop)
                {
                    eventLoop->loop(eventLoop->getTimerMgr()->isEmpty() ? sDefaultLoopTimeOutMS : eventLoop->getTimerMgr()->nearEndMs().count());
                    if (callback != nullptr)
                    {
                        callback(eventLoop);
                    }
                }
            }));
            v->resetPacketList();
        }
    }
}

void TcpService::wakeup(SESSION_TYPE id) const
{
    union  SessionId sid;
    sid.id = id;
    auto eventLoop = getEventLoopBySocketID(id);
    if (eventLoop != nullptr)
    {
        eventLoop->wakeup();
    }
}

void TcpService::wakeupAll() const
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    for (const auto& v : mIOLoopDatas)
    {
        v->getEventLoop()->wakeup();
    }
}

EventLoop::PTR TcpService::getRandomEventLoop()
{
    EventLoop::PTR ret;
    {
        auto randNum = rand();
        std::lock_guard<std::mutex> lock(mIOLoopGuard);
        if (!mIOLoopDatas.empty())
        {
            ret = mIOLoopDatas[randNum % mIOLoopDatas.size()]->getEventLoop();
        }
    }

    return ret;
}

TcpService::PTR TcpService::Create()
{
    struct make_shared_enabler : public TcpService {};
    return std::make_shared<make_shared_enabler>();
}

EventLoop::PTR TcpService::getEventLoopBySocketID(SESSION_TYPE id) const noexcept
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex < mIOLoopDatas.size());
    if (sid.data.loopIndex < mIOLoopDatas.size())
    {
        return mIOLoopDatas[sid.data.loopIndex]->getEventLoop();
    }
    else
    {
        return nullptr;
    }
}

std::shared_ptr<IOLoopData> TcpService::getIOLoopDataBySocketID(SESSION_TYPE id) const noexcept
{
    std::lock_guard<std::mutex> lock(mIOLoopGuard);

    union  SessionId sid;
    sid.id = id;
    assert(sid.data.loopIndex < mIOLoopDatas.size());
    if (sid.data.loopIndex < mIOLoopDatas.size())
    {
        return mIOLoopDatas[sid.data.loopIndex];
    }
    else
    {
        return nullptr;
    }
}

TcpService::SESSION_TYPE TcpService::MakeID(size_t loopIndex, const std::shared_ptr<IOLoopData>& loopData)
{
    union SessionId sid;
    sid.data.loopIndex = static_cast<uint16_t>(loopIndex);
    sid.data.index = static_cast<uint16_t>(loopData->getDataSockets().claimID());
    sid.data.iid = loopData->incID();

    return sid.id;
}

void TcpService::procDataSocketClose(DataSocket::PTR ds)
{
    auto ud = std::any_cast<SESSION_TYPE>(&ds->getUD());
    if (ud != nullptr)
    {
        union SessionId sid;
        sid.id = *ud;

        mIOLoopDatas[sid.data.loopIndex]->getDataSockets().set(nullptr, sid.data.index);
        mIOLoopDatas[sid.data.loopIndex]->getDataSockets().reclaimID(sid.data.index);
    }
}

bool TcpService::helpAddChannel(DataSocket::PTR channel, const std::string& ip, 
    const TcpService::ENTER_CALLBACK& enterCallback, const TcpService::DISCONNECT_CALLBACK& disConnectCallback, const TcpService::DATA_CALLBACK& dataCallback,
    bool forceSameThreadLoop)
{
    std::shared_ptr<IOLoopData> ioLoopData;
    size_t loopIndex = 0;
    {
        auto randNum = rand();
        std::lock_guard<std::mutex> lock(mIOLoopGuard);

        if (mIOLoopDatas.empty())
        {
            return false;
        }
        
        if (forceSameThreadLoop)
        {
            bool find = false;
            for (size_t i = 0; i < mIOLoopDatas.size(); i++)
            {
                if (mIOLoopDatas[i]->getEventLoop()->isInLoopThread())
                {
                    loopIndex = i;
                    find = true;
                    break;
                }
            }
            if (!find)
            {
                return false;
            }
        }
        else
        {
            /*  ���Ϊ�����ӷ���һ��eventloop */
            loopIndex = randNum % mIOLoopDatas.size();
        }

        ioLoopData = mIOLoopDatas[loopIndex];
    }
    

    const auto& loop = ioLoopData->getEventLoop();

    channel->setEnterCallback([ip, loopIndex, enterCallback, disConnectCallback, dataCallback, shared_this = shared_from_this(), loopDataCapture = std::move(ioLoopData)](DataSocket::PTR dataSocket){
        auto id = shared_this->MakeID(loopIndex, loopDataCapture);
        union SessionId sid;
        sid.id = id;
        loopDataCapture->getDataSockets().set(dataSocket, sid.data.index);
        dataSocket->setUD(id);
        dataSocket->setDataCallback([dataCallback, id](DataSocket::PTR ds, const char* buffer, size_t len){
            return dataCallback(id, buffer, len);
        });

        dataSocket->setDisConnectCallback([disConnectCallback, id, shared_this](DataSocket::PTR arg){
            shared_this->procDataSocketClose(arg);
            disConnectCallback(id);
            delete arg;
        });

        if (enterCallback != nullptr)
        {
            enterCallback(id, ip);
        }
    });

    loop->pushAsyncProc([loop, channel](){
        if (!channel->onEnterEventLoop(std::move(loop)))
        {
            delete channel;
        }
    });

    return true;
}

bool TcpService::addDataSocket( sock fd,
                                const TcpService::ENTER_CALLBACK& enterCallback,
                                const TcpService::DISCONNECT_CALLBACK& disConnectCallback,
                                const TcpService::DATA_CALLBACK& dataCallback,
                                bool isUseSSL,
                                size_t maxRecvBufferSize,
                                bool forceSameThreadLoop)
{
    std::string ip = ox_socket_getipoffd(fd);
    DataSocket::PTR channel = nullptr;
#ifdef USE_OPENSSL
    bool ret = true;
    channel = new DataSocket(fd, maxRecvBufferSize);
    if (isUseSSL)
    {
        ret = channel->initConnectSSL();
    }
#else
    bool ret = false;
    if (!isUseSSL)
    {
        channel = new DataSocket(fd, maxRecvBufferSize);
        ret = true;
    }
#endif
    if (ret)
    {
        ret = helpAddChannel(channel, ip, enterCallback, disConnectCallback, dataCallback, forceSameThreadLoop);
    }

    if (!ret)
    {
        if (channel != nullptr)
        {
            delete channel;
            channel = nullptr;
        }
        else
        {
            ox_socket_close(fd);
        }
    }

    return ret;
}

IOLoopData::PTR IOLoopData::Create(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread)
{
    struct make_shared_enabler : public IOLoopData
    {
        make_shared_enabler(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread) :
            IOLoopData(std::move(eventLoop), std::move(ioThread))
        {}
    };

    return std::make_shared<make_shared_enabler>(std::move(eventLoop), std::move(ioThread));
}

IOLoopData::IOLoopData(EventLoop::PTR eventLoop, std::shared_ptr<std::thread> ioThread)
    : mEventLoop(std::move(eventLoop)), mIOThread(std::move(ioThread))
{}

const EventLoop::PTR& IOLoopData::getEventLoop() const
{
    return mEventLoop;
}

std::shared_ptr<IOLoopData::MSG_LIST>& IOLoopData::getPacketList()
{
    return cachePacketList;
}

void IOLoopData::resetPacketList()
{
    cachePacketList = std::make_shared<MSG_LIST>();
}

brynet::TypeIDS<DataSocket::PTR>& IOLoopData::getDataSockets()
{
    return mDataSockets;
}

std::shared_ptr<std::thread>& IOLoopData::getIOThread()
{
    return mIOThread;
}

int IOLoopData::incID()
{
    return incId++;
}

void IOLoopData::send(TcpService::SESSION_TYPE id, DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback)
{
    union  SessionId sid;
    sid.id = id;
    /*  �����ǰ���������߳���ֱ��send,����ʹ��pushAsyncProc����lambda(���ٶ���Ӱ��),�����ʹ��postSessionAsyncProc */
    if (mEventLoop->isInLoopThread())
    {
        DataSocket::PTR tmp = nullptr;
        if (mDataSockets.get(sid.data.index, tmp) &&
            tmp != nullptr)
        {
            auto ud = std::any_cast<TcpService::SESSION_TYPE>(&tmp->getUD());
            if (ud != nullptr && *ud == sid.id)
            {
                tmp->sendPacketInLoop(std::move(packet), std::move(callback));
            }
        }
    }
    else
    {
        mEventLoop->pushAsyncProc([packetCapture = std::move(packet), callbackCapture = std::move(callback), sid, ioLoopDataCapture = shared_from_this()](){
            DataSocket::PTR tmp = nullptr;
            if (ioLoopDataCapture->mDataSockets.get(sid.data.index, tmp) &&
                tmp != nullptr)
            {
                auto ud = std::any_cast<TcpService::SESSION_TYPE>(&tmp->getUD());
                if (ud != nullptr && *ud == sid.id)
                {
                    tmp->sendPacketInLoop(std::move(packetCapture), std::move(callbackCapture));
                }
            }
        });
    }
}
