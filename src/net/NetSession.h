#ifndef _NETSESSION_H
#define _NETSESSION_H

#include <string>
#include "WrapTCPService.h"

using namespace brynet::net;

/*Ӧ�÷������������Ự�������*/
class BaseNetSession
{
public:
    typedef std::shared_ptr<BaseNetSession>  PTR;
    typedef std::weak_ptr<BaseNetSession>   WEAK_PTR;

    BaseNetSession()
    {
        mService = nullptr;
    }

    virtual ~BaseNetSession()
    {
    }

    void    setSession(const WrapTcpService::PTR& service, const TCPSession::PTR& session, const std::string& ip)
    {
        mService = service;
        mSession = session;
        mIP = ip;
    }

    const WrapTcpService::PTR& getService()
    {
        return mService;
    }

    /*�����յ�������*/
    virtual size_t  onMsg(const char* buffer, size_t len) = 0;
    /*���ӽ���*/
    virtual void    onEnter() = 0;
    /*���ӶϿ�*/
    virtual void    onClose() = 0;

    const std::string&  getIP() const
    {
        return mIP;
    }

    auto         getSocketID() const
    {
        return mSession->getSocketID();
    }

    void            postClose()
    {
        mSession->postClose();
    }

    void            sendPacket(const char* data, size_t len, DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mSession->send(DataSocket::makePacket(data, len), std::move(callback));
    }

    void            sendPacket(DataSocket::PACKET_PTR packet, DataSocket::PACKED_SENDED_CALLBACK callback = nullptr)
    {
        mSession->send(std::move(packet), std::move(callback));
    }

    const auto&      getEventLoop()
    {
        return mSession->getEventLoop();
    }

private:
    std::string         mIP;
    WrapTcpService::PTR mService;
    TCPSession::PTR     mSession;
};

void WrapAddNetSession(WrapTcpService::PTR service, sock fd, BaseNetSession::PTR pClient, int pingCheckTime, size_t maxRecvBufferSize);

#endif