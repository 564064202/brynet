#include <cassert>
#include <set>
#include <vector>
#include <map>
#include <thread>
#include <string>
#include <cstring>

#include <brynet/net/SocketLibFunction.h>
#include <brynet/net/fdset.h>

#include <brynet/net/Connector.h>

using namespace brynet;
using namespace brynet::net;

namespace brynet
{
    namespace net
    {
        class AsyncConnectAddr
        {
        public:
            AsyncConnectAddr() noexcept
            {
                mPort = 0;
            }

            AsyncConnectAddr(const char* ip, 
                int port, 
                std::chrono::milliseconds timeout, 
                AsyncConnector::COMPLETED_CALLBACK successCB,
                AsyncConnector::FAILED_CALLBACK failedCB) : mIP(ip),
                mPort(port), 
                mTimeout(timeout), 
                mSuccessCB(successCB),
                mFailedCB(failedCB)
            {
            }

            const auto&     getIP() const
            {
                return mIP;
            }

            auto                getPort() const
            {
                return mPort;
            }

            const auto&         getSuccessCB() const
            {
                return mSuccessCB;
            }

            const auto&         getFailedCB() const
            {
                return mFailedCB;
            }

            auto                getTimeout() const
            {
                return mTimeout;
            }

        private:
            std::string         mIP;
            int                 mPort;
            std::chrono::milliseconds   mTimeout;
            AsyncConnector::COMPLETED_CALLBACK mSuccessCB;
            AsyncConnector::FAILED_CALLBACK mFailedCB;
        };

        class ConnectorWorkInfo final : public NonCopyable
        {
        public:
            typedef std::shared_ptr<ConnectorWorkInfo>    PTR;

            ConnectorWorkInfo() noexcept;

            void                checkConnectStatus(int timeout);
            bool                isConnectSuccess(sock clientfd) const;
            void                checkTimeout();
            void                processConnect(AsyncConnectAddr);

        private:

            struct ConnectingInfo
            {
                std::chrono::steady_clock::time_point startConnectTime;
                std::chrono::milliseconds     timeout;
                AsyncConnector::COMPLETED_CALLBACK successCB;
                AsyncConnector::FAILED_CALLBACK failedCB;
            };

            std::map<sock, ConnectingInfo>  mConnectingInfos;
            std::set<sock>                  mConnectingFds;

            struct FDSetDeleter
            {
                void operator()(struct fdset_s* ptr) const
                {
                    ox_fdset_delete(ptr);
                }
            };

            std::unique_ptr<struct fdset_s, FDSetDeleter> mFDSet;
        };
    }
}

ConnectorWorkInfo::ConnectorWorkInfo() noexcept
{
    mFDSet.reset(ox_fdset_new());
}

bool ConnectorWorkInfo::isConnectSuccess(sock clientfd) const
{
    bool connect_ret = false;

    if (ox_fdset_check(mFDSet.get(), clientfd, WriteCheck))
    {
        int error;
        int len = sizeof(error);
        if (getsockopt(clientfd, SOL_SOCKET, SO_ERROR, (char*)&error, (socklen_t*)&len) != -1)
        {
            connect_ret = error == 0;
        }
    }

    return connect_ret;
}

void ConnectorWorkInfo::checkConnectStatus(int timeout)
{
    if (ox_fdset_poll(mFDSet.get(), timeout) <= 0)
    {
        return;
    }

    std::set<sock>       complete_fds;   /*  ��ɶ���    */
    std::set<sock>       failed_fds;     /*  ʧ�ܶ���    */

    for (auto& v : mConnectingFds)
    {
        if (ox_fdset_check(mFDSet.get(), v, ErrorCheck))
        {
            complete_fds.insert(v);
            failed_fds.insert(v);
        } 
        else if (ox_fdset_check(mFDSet.get(), v, WriteCheck))
        {
            complete_fds.insert(v);
            if (!isConnectSuccess(v))
            {
                failed_fds.insert(v);
            }
        }
    }

    for (auto fd : complete_fds)
    {
        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

        auto it = mConnectingInfos.find(fd);
        if (it != mConnectingInfos.end())
        {
            if (failed_fds.find(fd) != failed_fds.end())
            {
                ox_socket_close(fd);
                if (it->second.failedCB != nullptr)
                {
                    it->second.failedCB();
                }
            }
            else
            {
                if (it->second.successCB != nullptr)
                {
                    it->second.successCB(fd);
                }
            }

            mConnectingInfos.erase(it);
        }

        mConnectingFds.erase(fd);
    }
}

void ConnectorWorkInfo::checkTimeout()
{
    for (auto it = mConnectingInfos.begin(); it != mConnectingInfos.end();)
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - it->second.startConnectTime) < it->second.timeout)
        {
            ++it;
            continue;
        }

        auto fd = it->first;
        auto cb = it->second.failedCB;

        ox_fdset_del(mFDSet.get(), fd, WriteCheck | ErrorCheck);

        mConnectingFds.erase(fd);
        mConnectingInfos.erase(it++);

        ox_socket_close(fd);
        if (cb != nullptr)
        {
            cb();
        }
    }
}

void ConnectorWorkInfo::processConnect(AsyncConnectAddr addr)
{
    bool addToFDSet = false;
    bool connectSuccess = false;

    struct sockaddr_in server_addr;
    sock clientfd = SOCKET_ERROR;

    ox_socket_init();

    clientfd = ox_socket_create(AF_INET, SOCK_STREAM, 0);
    ox_socket_nonblock(clientfd);

    if (clientfd != SOCKET_ERROR)
    {
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(addr.getIP().c_str());
        server_addr.sin_port = htons(addr.getPort());

        int n = connect(clientfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
        if (n < 0)
        {
            int check_error = 0;
#if defined PLATFORM_WINDOWS
            check_error = WSAEWOULDBLOCK;
#else
            check_error = EINPROGRESS;
#endif
            if (check_error != sErrno)
            {
                ox_socket_close(clientfd);
                clientfd = SOCKET_ERROR;
            }
            else
            {
                ConnectingInfo ci;
                ci.startConnectTime = std::chrono::steady_clock::now();
                ci.successCB = addr.getSuccessCB();
                ci.failedCB = addr.getFailedCB();
                ci.timeout = addr.getTimeout();

                mConnectingInfos[clientfd] = ci;
                mConnectingFds.insert(clientfd);

                ox_fdset_add(mFDSet.get(), clientfd, WriteCheck | ErrorCheck);
                addToFDSet = true;
            }
        }
        else if (n == 0)
        {
            connectSuccess = true;
        }
    }

    if (connectSuccess)
    {
        if (addr.getSuccessCB() != nullptr)
        {
            addr.getSuccessCB()(clientfd);
        }
    }
    else
    {
        if (!addToFDSet && addr.getFailedCB() != nullptr)
        {
            addr.getFailedCB()();
        }
    }
}

AsyncConnector::AsyncConnector()
{
    mIsRun = false;
}

AsyncConnector::~AsyncConnector()
{
    destroy();
}

void AsyncConnector::run()
{
    while (mIsRun)
    {
        mEventLoop.loop(10);
        mWorkInfo->checkConnectStatus(0);
        mWorkInfo->checkTimeout();
    }
}

void AsyncConnector::startThread()
{
    std::lock_guard<std::mutex> lck(mThreadGuard);
    if (mThread == nullptr)
    {
        mIsRun = true;
        mWorkInfo = std::make_shared<ConnectorWorkInfo>();
        mThread = std::make_shared<std::thread>([shared_this = shared_from_this()](){
            shared_this->run();
        });
    }
}

void AsyncConnector::destroy()
{
    std::lock_guard<std::mutex> lck(mThreadGuard);
    if (mThread != nullptr)
    {
        mIsRun = false;
        if (mThread->joinable())
        {
            mThread->join();
        }
        mThread = nullptr;
        mWorkInfo = nullptr;
    }
}

//TODO::�����Ѿ������˹����̣߳���Ͷ���첽������������⣨���޵ȴ���
void AsyncConnector::asyncConnect(const char* ip, int port, int ms, COMPLETED_CALLBACK successCB, FAILED_CALLBACK failedCB)
{
    mEventLoop.pushAsyncProc([shared_this = shared_from_this(), 
        address = AsyncConnectAddr(ip, port, std::chrono::milliseconds(ms), 
            successCB, 
            failedCB)]() {
        shared_this->mWorkInfo->processConnect(address);
    });
}

AsyncConnector::PTR AsyncConnector::Create()
{
    struct make_shared_enabler : public AsyncConnector {};
    return std::make_shared<make_shared_enabler>();
}