#ifndef _CONNECTOR_H
#define _CONNECTOR_H

#include <list>
using namespace std;

#include "socketlibtypes.h"

class ThreadConnector;

/*  �첽���Ӹ������ݻ��࣬������Connector������������delete ConnectorBaseReq*,���������ڴ�й©   */
class ConnectorBaseReq
{
public:
    virtual ~ConnectorBaseReq()
    {
    }
};

class Connector
{
public:
    Connector();
    virtual ~Connector();

    /*  �����첽���ӣ������ӳɹ���ᴥ��onConnectSuccess�麯�������򴥷�onConnectFailed  */
    void            connect(const char* ip, int port, int ms, ConnectorBaseReq* ud);

    void            poll();

private:
    virtual void    onConnectFailed(ConnectorBaseReq* ud) = 0;  /*  ����ʧ��    */
    virtual void    onConnectSuccess(sock fd, ConnectorBaseReq* ud) = 0; /*  ���ӳɹ�    */

private:

    struct non_connect_req
    {
        ConnectorBaseReq*   ud;         /*  �߼�����ʹ��new��������ΪConnector������������delete ud    */
    };

    /*  �������    */
    list<non_connect_req>  m_reqs;

    /*  �����߳�connect  */
    ThreadConnector*        m_threadConnector;
};

#endif