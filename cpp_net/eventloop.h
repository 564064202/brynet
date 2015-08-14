#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <stdint.h>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <unordered_map>

#include "socketlibtypes.h"
#include "currentthread.h"
#include "timer.h"

class Channel;

class EventLoop
{
public:
    typedef std::shared_ptr<Channel> CHANNEL_PTR;

    typedef std::function<void(void)>           USER_PROC;

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

    void                            pushAsyncChannel(CHANNEL_PTR);

    /*  Ͷ��һ���첽�ص�����EventLoop::loop�����Ѻ�ִ�� */
    void                            pushAsyncProc(const USER_PROC& f);
    void                            pushAsyncProc(USER_PROC&& f);

    /*  ���̰߳�ȫ:Ͷ�ݻص������ڵ���loop��βʱִ��   */
    void                            pushAfterLoopProc(const USER_PROC& f);
    void                            pushAfterLoopProc(USER_PROC&& f);

    void                            restoreThreadID();

    /*  ���̰߳�ȫ,��channel��������  */
    void                            addChannel(int64_t id, CHANNEL_PTR channel);
    /*  ���̰߳�ȫ,���������Ƴ�fd��Ӧ��channel  */
    void                            removeChannel(int64_t id);

    void                            linkChannel(int fd, Channel* ptr);

    TimerMgr&                       getTimerMgr();
#ifdef PLATFORM_WINDOWS
    HANDLE                          getIOCPHandle() const;
#else
    int                             getEpollHandle() const;
#endif

private:
    void                            reallocEventSize(int size);
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
    CurrentThread::THREAD_ID_TYPE           mSelfThreadid;
    std::unordered_map<int64_t, CHANNEL_PTR>    mChannels;      /*  ��ŵ�ǰ���лỰ���ӣ���֤������ָ��(�ڴ����������ڼ�)��������(���ü�������1) */

    TimerMgr                                    mTimer;
};

#endif