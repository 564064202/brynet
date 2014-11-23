#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include "platform.h"
#ifdef PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx)
    (HANDLE CompletionPort,
    LPOVERLAPPED_ENTRY lpCompletionPortEntries,
    ULONG ulCount,
    PULONG ulNumEntriesRemoved,
    DWORD dwMilliseconds,
    BOOL fAlertable);

    #define OVL_RECV    (1)
    #define OVL_SEND    (2)
#else
    #include <sys/epoll.h>
    #include <signal.h>
    #include <sys/uio.h>
#endif

#include <stdint.h>

#include <functional>
#include <vector>
#include <mutex>
#include <thread>

using namespace std;

class Channel;

class EventLoop
{
public:
    typedef std::function<void(void)>       USER_PROC;
    typedef std::function<void(Channel*)>   CONNECTION_ENTER_HANDLE;
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
    void                            pushAfterLoopProc(USER_PROC f);

private:
    void                            recalocEventSize(int size);
    void                            linkConnection(int fd, Channel* ptr);

private:
    int                             mEventEntriesNum;
#ifdef PLATFORM_WINDOWS
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
    std::unique_lock<std::mutex>    mFlagLock;
    bool                            mInWaitIOEvent;             /*  ���Ϊfalse��ʾ�϶�û�еȴ�IOCP�����Ϊtrue����ʾ�������Ѿ��ȴ�iocp*/
    bool                            mIsAlreadyPostedWakeUp;     /*  ��ʾ�Ƿ��Ѿ�Ͷ�ݹ�wakeup(���������߳�Ͷ��̫��(����Ҫ)��wakeup) */

    vector<USER_PROC>               mAsyncProcs;                /*  �첽function����,Ͷ�ݵ���eventloopִ�еĻص�����   */

    vector<USER_PROC>               mAfterLoopProcs;            /*  eventloopÿ��ѭ����ĩβҪִ�е�һϵ�к�����ֻ����io�߳������ڶԴ˶�������Ӳ���    */

    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::thread::id                 mSelfThreadid;
};

#endif