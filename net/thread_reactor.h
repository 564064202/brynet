#ifndef _THREAD_REACTOR_H
#define _THREAD_REACTOR_H

#include <stdint.h>
#include "stdbool.h"
#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct nr_mgr;
struct net_session_s;

enum nrmgr_net_msg_type
{
    nrmgr_net_msg_connect,
    nrmgr_net_msg_close,
    nrmgr_net_msg_data,
};

struct nrmgr_net_msg
{
    struct net_session_s* session;
    void* ud;
    enum nrmgr_net_msg_type type;
    int data_len;
    char data[1];
};

struct nrmgr_send_msg_data
{
    int ref;
    int data_len;
    char data[1];
};

typedef void (*pfn_nrmgr_logicmsg)(struct nr_mgr* mgr, struct nrmgr_net_msg*);
typedef int (*pfn_nrmgr_check_packet)(void* ud, const char* buffer, int len);

DLL_CONF struct nr_mgr* ox_create_nrmgr(
    int thread_num,                 /*  ����㿪����reactor�߳�����   */
    int rbsize,                     /*  �����Ự�����ý��ջ�������С  */
    pfn_nrmgr_check_packet check    /*  �������յ�����ʱ�жϰ������ԵĻص�����    */
    );

DLL_CONF void ox_nrmgr_delete(struct nr_mgr* self);

/*  ����׽��ֵ������   */
DLL_CONF void ox_nrmgr_addfd(struct nr_mgr* mgr, void* ud, int fd);

/*  �߼��������������Ϣ  */
DLL_CONF struct nrmgr_send_msg_data* ox_nrmgr_make_sendmsg(struct nr_mgr* mgr, const char* src, int len);

/*  �߼��㷢����Ϣ������� */
DLL_CONF void ox_nrmgr_sendmsg(struct nr_mgr* mgr, struct nrmgr_send_msg_data* data, struct net_session_s* session);

/*  ���������������������Ͽ�ĳ����    */
DLL_CONF void ox_nrmgr_request_closesession(struct nr_mgr* mgr, struct net_session_s* session);
/*  �ر�handle�Ự����(���յ�close��Ϣ����ô˺���   */
DLL_CONF void ox_nrmgr_closesession(struct nr_mgr* mgr, struct net_session_s* session);

/*  �߼��㴦��������������Ϣ   */
DLL_CONF void ox_nrmgr_logic_poll(struct nr_mgr* mgr, pfn_nrmgr_logicmsg msghandle, int64_t timeout);

DLL_CONF void ox_nrmgr_setuserdata(struct nr_mgr* mgr, void* ud);
DLL_CONF void*  ox_nrmgr_getuserdata(struct nr_mgr* mgr);

#ifdef  __cplusplus
}
#endif

#endif
