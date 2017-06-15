#ifndef BRYNET_NET_TCP_SERVICE_H_
#define BRYNET_NET_TCP_SERVICE_H_

#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <cstdint>
#include <memory>

#include "DataSocket.h"
#include "NonCopyable.h"

namespace brynet
{
    namespace net
    {
        class EventLoop;

        class ListenThread : public NonCopyable, public std::enable_shared_from_this<ListenThread>
        {
        public:
            typedef std::shared_ptr<ListenThread>   PTR;
            typedef std::function<void(sock fd)> ACCEPT_CALLBACK;

            /*  ���������߳�  */
            void                                startListen(bool isIPV6, const std::string& ip, int port, const char *certificate, const char *privatekey, ACCEPT_CALLBACK callback);
            void                                closeListenThread();
#ifdef USE_OPENSSL
            SSL_CTX*                            getOpenSSLCTX();
#endif
            static  PTR                         Create();

        private:
            ListenThread() noexcept;
            virtual ~ListenThread() noexcept;

            void                                runListen();
            void                                initSSL();
            void                                destroySSL();

        private:
            ACCEPT_CALLBACK                     mAcceptCallback;
            bool                                mIsIPV6;
            std::string                         mIP;
            int                                 mPort;
            bool                                mRunListen;
            std::shared_ptr<std::thread>        mListenThread;
            std::string                         mCertificate;
            std::string                         mPrivatekey;
#ifdef USE_OPENSSL
            SSL_CTX*                            mOpenSSLCTX;
#endif
        };

        struct IOLoopData;

        /*  ������IDΪ����Ự��ʶ���������   */
        class TcpService : public NonCopyable, public std::enable_shared_from_this<TcpService>
        {
        public:
            typedef std::shared_ptr<TcpService>                                         PTR;
            typedef int64_t SESSION_TYPE;

            typedef std::function<void(EventLoop::PTR)>                                 FRAME_CALLBACK;
            typedef std::function<void(SESSION_TYPE, const std::string&)>               ENTER_CALLBACK;
            typedef std::function<void(SESSION_TYPE)>                                   DISCONNECT_CALLBACK;
            typedef std::function<size_t(SESSION_TYPE, const char* buffer, size_t len)> DATA_CALLBACK;

        public:
            static  PTR                         Create();

        public:
            /*  ����Ĭ���¼��ص�    */
            void                                setEnterCallback(TcpService::ENTER_CALLBACK&& callback);
            void                                setEnterCallback(const TcpService::ENTER_CALLBACK& callback);

            void                                setDisconnectCallback(TcpService::DISCONNECT_CALLBACK&& callback);
            void                                setDisconnectCallback(const TcpService::DISCONNECT_CALLBACK& callback);

            void                                setDataCallback(TcpService::DATA_CALLBACK&& callback);
            void                                setDataCallback(const TcpService::DATA_CALLBACK& callback);

            const TcpService::ENTER_CALLBACK&       getEnterCallback() const;
            const TcpService::DISCONNECT_CALLBACK&  getDisconnectCallback() const;
            const TcpService::DATA_CALLBACK&        getDataCallback() const;

            void                                send(SESSION_TYPE id, const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr) const;

            /*  �߼��̵߳��ã���Ҫ���͵���Ϣ��������������һ����ͨ��flushCachePackectList���뵽�����߳�    */
            void                                cacheSend(SESSION_TYPE id, const DataSocket::PACKET_PTR& packet, const DataSocket::PACKED_SENDED_CALLBACK& callback = nullptr);

            void                                flushCachePackectList();

            void                                shutdown(SESSION_TYPE id) const;

            /*�����Ͽ���id���ӣ�����Ȼ���յ���id�ĶϿ��ص�����Ҫ�ϲ��߼��Լ��������"����"(����ͳһ�ڶϿ��ص�������������ȹ���) */
            void                                disConnect(SESSION_TYPE id) const;

            void                                setPingCheckTime(SESSION_TYPE id, int checktime);

            bool                                addDataSocket(sock fd,
                                                                const TcpService::ENTER_CALLBACK& enterCallback,
                                                                const TcpService::DISCONNECT_CALLBACK& disConnectCallback,
                                                                const TcpService::DATA_CALLBACK& dataCallback,
                                                                bool isUseSSL,
                                                                size_t maxRecvBufferSize,
                                                                bool forceSameThreadLoop = false);

            /*  ���������߳�  */
            void                                startListen(bool isIPV6, const std::string& ip, int port, int maxSessionRecvBufferSize, const char *certificate = nullptr, const char *privatekey = nullptr);
            /*  ����IO�����߳�    */
            void                                startWorkerThread(size_t threadNum, FRAME_CALLBACK callback = nullptr);

            /*  �رշ���(�������ڴ�):���̰߳�ȫ    */
            void                                closeService();
            void                                closeListenThread();
            void                                closeWorkerThread();

            /*  ������ֹͣ�����߳��Լ���ÿ��EventLoop�˳�ѭ���������ͷ�EventLoop�ڴ� */
            void                                stopWorkerThread();

            /*  wakeupĳid���ڵ����繤���߳�:���̰߳�ȫ    */
            void                                wakeup(SESSION_TYPE id) const;
            /*  wakeup ���е����繤���߳�:���̰߳�ȫ  */
            void                                wakeupAll() const;
            /*  �����ȡһ��EventLoop:���̰߳�ȫ   */
            EventLoop::PTR                      getRandomEventLoop();
            EventLoop::PTR                      getEventLoopBySocketID(SESSION_TYPE id) const noexcept;

        private:
            TcpService() noexcept;
            virtual ~TcpService() noexcept;

            bool                                helpAddChannel(DataSocket::PTR channel,
                                                                const std::string& ip,
                                                                const TcpService::ENTER_CALLBACK& enterCallback,
                                                                const TcpService::DISCONNECT_CALLBACK& disConnectCallback,
                                                                const TcpService::DATA_CALLBACK& dataCallback,
                                                                bool forceSameThreadLoop = false);
        private:
            SESSION_TYPE                        MakeID(size_t loopIndex);
            void                                procDataSocketClose(DataSocket::PTR);
            /*  ��id��ʶ��DataSocketͶ��һ���첽����(���������߳�ִ��)(����֤ID����Ч��)  */
            void                                postSessionAsyncProc(SESSION_TYPE id, const std::function<void(DataSocket::PTR)>& callback) const;

        private:
            std::vector<std::shared_ptr<IOLoopData>>    mIOLoopDatas;

            bool                                mRunIOLoop;

            ListenThread::PTR                   mListenThread;

            /*  ���������ص��������ڶ��߳��е���(ÿ���̼߳�һ��eventloop������io loop)(����RunListen�е�ʹ��)   */
            TcpService::ENTER_CALLBACK          mEnterCallback;
            TcpService::DISCONNECT_CALLBACK     mDisConnectCallback;
            TcpService::DATA_CALLBACK           mDataCallback;
        };
    }
}

#endif
