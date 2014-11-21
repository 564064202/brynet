#include "eventloop.h"
#include "channel.h"
#include "socketlibtypes.h"

#include <iostream>
#include <assert.h>
#include <thread>

using namespace std;

#ifndef PLATFORM_WINDOWS
class WeakupChannel : public Channel
{
public:
    WeakupChannel(int fd)
    {
        mFd = fd;

    }
private:
    void    canRecv()
    {
        //printf("weak channel can recv \n");
        char temp[1024*10];
        while(true)
        {
            ssize_t n = recv(mFd, temp, 1024*10, 0);
            if(n == -1)
            {
                break;
            }
        }
    }

    void    canSend()
    {
    }

    void    setEventLoop(EventLoop*)
    {
    }

    void    disConnect()
    {
    }

private:
    int     mFd;
};
#endif

EventLoop::EventLoop() : mLock(mMutex, std::defer_lock), mFlagLock(mFlagMutex, std::defer_lock)
{
    mSelfThreadid = std::this_thread::get_id();
#ifdef PLATFORM_WINDOWS
    mPGetQueuedCompletionStatusEx = NULL;
    HMODULE kernel32_module = GetModuleHandleA("kernel32.dll");
    if (kernel32_module != NULL) {
        mPGetQueuedCompletionStatusEx = (sGetQueuedCompletionStatusEx)GetProcAddress(
            kernel32_module,
            "GetQueuedCompletionStatusEx");
    }
    mIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
#else
    mEpollFd = epoll_create(1);
    mWeakupFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    mWeakupChannel = new WeakupChannel(mWeakupFd);
    linkConnection(mWeakupFd, mWeakupChannel);
#endif

    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    mEventEntries = NULL;
    mEventEntriesNum = 0;

    recalocEventSize(1024);
}

EventLoop::~EventLoop()
{
    printf("~ EventLoop");
}

void EventLoop::loop(int64_t timeout)
{
    /*  TODO::��ֹ��loop֮ǰ�������һЩfunction�ص�����û���µĲ���wakeup��loop������ */
    if (!mAfterLoopProcs.empty())
    {
        timeout = 0;
    }

    mInWaitIOEvent = true;

#ifdef PLATFORM_WINDOWS
    ULONG numComplete = 0;
    BOOL rc = mPGetQueuedCompletionStatusEx(mIOCP, mEventEntries, mEventEntriesNum, &numComplete, static_cast<DWORD>(timeout), false);
    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    if (rc)
    {
        for (ULONG i = 0; i < numComplete; ++i)
        {
            Channel* ds = (Channel*)mEventEntries[i].lpCompletionKey;
            if (mEventEntries[i].lpOverlapped != (LPOVERLAPPED)0xcdcdcdcd)
            {
                if (mEventEntries[i].lpOverlapped->Offset == OVL_RECV)
                {
                    ds->canRecv();
                }
                else if (mEventEntries[i].lpOverlapped->Offset == OVL_SEND)
                {
                    ds->canSend();
                }
            }
            else
            {
                assert(mEventEntries[i].dwNumberOfBytesTransferred == 0xDEADC0DE);
            }
        }
    }
    else
    {
        //cout << this << " error code:" << GetLastError() << endl;
    }
#else
    //printf("enter epoll_wait \n");
    int numComplete = epoll_wait(mEpollFd, mEventEntries, mEventEntriesNum, timeout);
    //printf("epoll_wait success: %d \n", numComplete);
    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    for (int i = 0; i < numComplete; ++i)
    {
        Channel*   ds = (Channel*)(mEventEntries[i].data.ptr);
        uint32_t event_data = mEventEntries[i].events;

        if (event_data & EPOLLRDHUP)
        {
            ds->canRecv();
            ds->disConnect();   /*  ���������öϿ������Է�canRecv��û��recv �Ͽ�֪ͨ*/
        }
        else
        {
            if (event_data & EPOLLIN)
            {
                ds->canRecv();
            }

            if (event_data & EPOLLOUT)
            {
                ds->canSend();
            }
        }
    }
#endif

    vector<USER_PROC> temp;
    mMutex.lock();
    temp.swap(mAsyncProcs);
    mMutex.unlock();
    
    for (auto& x : temp)
    {
        x();
    }

    for (auto& x : mAfterLoopProcs)
    {
        x();
    }

    mAfterLoopProcs.clear();

    if (numComplete == mEventEntriesNum)
    {
        /*  ����¼���������ˣ��������¼����д�С */
        recalocEventSize(mEventEntriesNum + 128);
    }
}

bool EventLoop::wakeup()
{
    //printf("try wakeup \n");
    bool ret = false;
    /*  TODO::1����֤�̰߳�ȫ��2����֤io�̴߳���wait״̬ʱ����羡�����ٷ���wakeup��3���Ƿ�io�߳��б�Ҫ�Լ�������Ͷ��һ��wakeup��������һ��wait�� */
    if (mSelfThreadid != std::this_thread::get_id())
    {
        if (mInWaitIOEvent)
        {
            /*������ܴ���iocp wait��ֱ��Ͷ��wakeup(������ܵ��¶���߳��ظ�Ͷ���˺ܶ�wakeup*/
            /*����ֻ��������Ͷ�ݣ������ܸ���mIsAlreadyPostedWakeUp��־�жϣ���Ϊ����iocp wait����û����*/
            /*���iocp�Ѿ�wakeup��Ȼ�����ﲻͶ�ݵĻ����п����޷�������һ��iocp (�ⲻ��epoll ltģʽ�µ�eventfd��ֻҪ������û������epoll�ܻ�wakeup)*/
            mIsAlreadyPostedWakeUp = true;
#ifdef PLATFORM_WINDOWS
            PostQueuedCompletionStatus(mIOCP, 0xDEADC0DE, 0, (LPOVERLAPPED)0xcdcdcdcd);
#else
            uint64_t one = 1;
            ssize_t n = write(mWeakupFd, &one, sizeof one);
            //printf("write %d to eventfd :%d\n", n, mWeakupFd);
#endif
            ret = true;
        }
        else
        {
            /*���һ��û�д���iocp wait*/
            /*�����else����mIsAlreadyPostedWakeUpһ��Ϊfalse*/
            /*����߳�ʹ�û������ж�mIsAlreadyPostedWakeUp��־���Ա�֤���ظ�Ͷ��*/
            if (!mIsAlreadyPostedWakeUp)
            {
                mFlagLock.lock();
                if (!mIsAlreadyPostedWakeUp)
                {
                    mIsAlreadyPostedWakeUp = true;
#ifdef PLATFORM_WINDOWS
                    PostQueuedCompletionStatus(mIOCP, 0xDEADC0DE, 0, (LPOVERLAPPED)0xcdcdcdcd);
#else
                    uint64_t one = 1;
                    ssize_t n = write(mWeakupFd, &one, sizeof one);
                    //printf("write %d to eventfd :%d\n", n, mWeakupFd);
#endif
                    ret = true;
                }
                mFlagLock.unlock();
            }
        }
    }

    return ret;
}

void EventLoop::linkConnection(int fd, Channel* ptr)
{
    printf("link to fd:%d \n", fd);
#ifdef PLATFORM_WINDOWS
    HANDLE ret = CreateIoCompletionPort((HANDLE)fd, mIOCP, (DWORD)ptr, 0);
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    ev.data.ptr = ptr;
    int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev);
    printf("epoll ctl ret:%d \n", ret);
#endif
}

void EventLoop::addConnection(int fd, Channel* c, CONNECTION_ENTER_HANDLE f)
{
    printf("addConnection fd:%d\n", fd);
    pushAsyncProc([fd, c, this, f] () {
            linkConnection(fd, c);
            c->setEventLoop(this);
            f(c);
    });
}

void EventLoop::pushAsyncProc(std::function<void(void)> f)
{
    if (mSelfThreadid != std::this_thread::get_id())
    {
        mLock.lock();
        mAsyncProcs.push_back(f);
        mLock.unlock();

        wakeup();
    }
    else
    {
        f();
    }
}

void EventLoop::pushAfterLoopProc(USER_PROC f)
{
    mAfterLoopProcs.push_back(f);
}

void EventLoop::recalocEventSize(int size)
{
    if (mEventEntries != NULL)
    {
        delete[] mEventEntries;
    }

#ifdef PLATFORM_WINDOWS
    mEventEntries = new OVERLAPPED_ENTRY[size];
#else
    mEventEntries = new epoll_event[size];
#endif

    mEventEntriesNum = size;
}