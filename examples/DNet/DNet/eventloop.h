#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <stdint.h>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>

#include "socketlibtypes.h"

class Channel;

class EventLoop
{
public:
    typedef std::function<void(void)>       USER_PROC;
    typedef std::function<void(Channel*)>   CONNECTION_ENTER_HANDLE;

    #ifdef PLATFORM_WINDOWS
    enum OLV_VALUE
    {
        OVL_RECV = 1,
        OVL_SEND,
    };
    #endif

public:
    EventLoop();
    ~EventLoop();

    void                            loop(int64_t    timeout);

    bool                            wakeup();

    /*  Ͷ��һ�����ӣ�����eventloopEventLoop�󶨣�����eventloop����ʱ�ᴥ��f�ص�  */
    void                            addConnection(int fd, Channel*, CONNECTION_ENTER_HANDLE f);

    /*  Ͷ��һ���첽function����eventloop�����Ѻ󣬻�ص���f*/
    void                            pushAsyncProc(USER_PROC f);

                                    /*  ����һ��ÿ��loop���Ҫִ�еĺ���(TODO::����loop�߳������ڵ���)  */
                                    /*  ����ʵ��һ��datasocket�ж��bufferҪ����ʱ������һ��function���кϲ���flush������ÿһ��buffer����һ��send   */
                                    /*  ���̰߳�ȫ   */
    void                            pushAfterLoopProc(USER_PROC f);

    void                            restoreThreadID();

private:
    void                            recalocEventSize(int size);
    void                            linkConnection(int fd, Channel* ptr);

private:
    int                             mEventEntriesNum;
#ifdef PLATFORM_WINDOWS
    typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx) (HANDLE ,LPOVERLAPPED_ENTRY ,ULONG ,PULONG ,DWORD ,BOOL );

    OVERLAPPED_ENTRY*               mEventEntries;
    sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
    HANDLE                          mIOCP;
    OVERLAPPED                      mWakeupOvl;
    Channel*                        mWakeupChannel;
#else
    int                             mEpollFd;
    epoll_event*                    mEventEntries;
    int                             mWakeupFd;
    Channel*                        mWakeupChannel;
#endif

    std::mutex                      mFlagMutex;

    bool                            mInWaitIOEvent;             /*  ���Ϊfalse��ʾ�϶�û�еȴ�IOCP�����Ϊtrue����ʾ�������Ѿ��ȴ�iocp*/
    bool                            mIsAlreadyPostedWakeUp;     /*  ��ʾ�Ƿ��Ѿ�Ͷ�ݹ�wakeup(���������߳�Ͷ��̫��(����Ҫ)��wakeup) */

    std::vector<USER_PROC>          mAsyncProcs;                /*  �첽function����,Ͷ�ݵ���eventloopִ�еĻص�����   */

    std::vector<USER_PROC>          mAfterLoopProcs;            /*  eventloopÿ��ѭ����ĩβҪִ�е�һϵ�к�����ֻ����io�߳������ڶԴ˶�������Ӳ���    */

    std::mutex                      mAsyncListMutex;

    /*����loop��������thread��id*/
    std::thread::id                 mSelfThreadid;
};

#endif