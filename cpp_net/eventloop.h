#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <stdint.h>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>

#include "socketlibtypes.h"
#include "currentthread.h"

class Channel;

class EventLoop
{
public:
    typedef std::function<void(void)>       USER_PROC;
    typedef std::function<void(Channel*)>   CHANNEL_ENTER_HANDLE;

#ifdef PLATFORM_WINDOWS
    enum OLV_VALUE
    {
        OVL_RECV,
        OVL_SEND,
        OVL_CLOSE,
    };
#endif

public:
    EventLoop();
    ~EventLoop();

    void                            loop(int64_t    timeout);

    bool                            wakeup();

    /*  Ͷ��һ�����ӣ�����eventloopEventLoop�󶨣�����eventloop����ʱ�ᴥ��f�ص�, TODO::Channel*���������ڹ�������(�����Ǵ˴�����ʧ�ܣ��Լ�����Channel close֮��)  */
    void                            addChannel(int fd, Channel*, CHANNEL_ENTER_HANDLE f);

    /*  Ͷ��һ���첽�ص�����EventLoop::loop�����Ѻ�ִ�� */
    void                            pushAsyncProc(const USER_PROC& f);
    void                            pushAsyncProc(USER_PROC&& f);

    /*  ���̰߳�ȫ:Ͷ�ݻص������ڵ���loop��βʱִ��   */
    void                            pushAfterLoopProc(const USER_PROC& f);
    void                            pushAfterLoopProc(USER_PROC&& f);

    void                            restoreThreadID();

#ifdef PLATFORM_WINDOWS
    HANDLE                          getIOCPHandle() const;
#else
    int                             getEpollHandle() const;
#endif

private:
    void                            recalocEventSize(int size);
    void                            linkChannel(int fd, Channel* ptr);
    void                            processAfterLoopProcs();
    void                            processAsyncProcs();
    bool                            isInLoopThread();
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

    bool                            mInWaitIOEvent;             /*  ���Ϊfalse��ʾ�϶�û�д���epoll/iocp wait�����Ϊtrue����ʾ�������Ѿ��ȴ�*/
    bool                            mIsAlreadyPostedWakeUp;     /*  ��ʾ�Ƿ��Ѿ�Ͷ�ݹ�wakeup(���������߳�Ͷ��̫��(����Ҫ)��wakeup) */

    std::vector<USER_PROC>          mAsyncProcs;                /*  Ͷ�ݵ���eventloop���첽function����    */

    std::vector<USER_PROC>          mAfterLoopProcs;            /*  eventloopÿ��ѭ����ĩβҪִ�е�һϵ�к���   */
    std::vector<USER_PROC>          copyAfterLoopProcs;         /*  ������loop�д���mAfterLoopProcs���б������������;���������Ԫ��  */

    std::mutex                      mAsyncProcsMutex;

    /*����loop��������thread��id*/
    CurrentThread::THREAD_ID_TYPE   mSelfThreadid;
};

#endif