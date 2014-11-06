#ifndef _EVENT_LOOP_H
#define _EVENT_LOOP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdint.h>

#include <functional>
#include <vector>
#include <mutex>
#include <thread>

using namespace std;

typedef BOOL(WINAPI *sGetQueuedCompletionStatusEx)
(HANDLE CompletionPort,
LPOVERLAPPED_ENTRY lpCompletionPortEntries,
ULONG ulCount,
PULONG ulNumEntriesRemoved,
DWORD dwMilliseconds,
BOOL fAlertable);

#define OVL_RECV    (1)
#define OVL_SEND    (2)

class Channel;

class EventLoop
{
    typedef std::function<void(void)>       USER_PROC;
    typedef std::function<void(Channel*)>   CONNECTION_ENTER_HANDLE;
public:
    EventLoop();

    void                            loop(int64_t    timeout);

    void                            wakeup();

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
    OVERLAPPED_ENTRY*               mEventEntries;

    sGetQueuedCompletionStatusEx    mPGetQueuedCompletionStatusEx;
    HANDLE                          mIOCP;

    BOOL                            mInWaitIOEvent;
    BOOL                            mIsAlreadyPostedWakeUp;

    vector<USER_PROC>               mAsyncProcs;

    vector<USER_PROC>               mAfterLoopProcs;

    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::thread::id                 mSelfThreadid;
};

#endif