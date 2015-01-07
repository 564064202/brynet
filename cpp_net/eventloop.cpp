#include <iostream>
#include <assert.h>
#include <thread>

#include "channel.h"
#include "eventloop.h"

class WakeupChannel : public Channel
{
public:
    WakeupChannel(int fd)
    {
        mFd = fd;

    }
private:
    void    canRecv()
    {
#ifdef PLATFORM_WINDOWS
#else
        /*  linux �±�������������� */
        char temp[1024 * 10];
        while (true)
        {
            ssize_t n = recv(mFd, temp, 1024 * 10, 0);
            if (n == -1)
            {
                break;
            }
        }
#endif
    }

    void    canSend()
    {
    }

    void    setEventLoop(EventLoop*)
    {
    }

    void    postDisConnect()
    {
    }

    void    onClose()
    {
    }

private:
    int     mFd;
};

EventLoop::EventLoop()
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
    memset(&mWakeupOvl, sizeof(mWakeupOvl), 0);
    mWakeupOvl.Offset = EventLoop::OVL_RECV;
    mWakeupChannel = new WakeupChannel(-1);
#else
    mEpollFd = epoll_create(1);
    mWakeupFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    mWakeupChannel = new WakeupChannel(mWakeupFd);
    linkConnection(mWakeupFd, mWakeupChannel);
#endif

    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    mEventEntries = NULL;
    mEventEntriesNum = 0;

    recalocEventSize(1024);
}

EventLoop::~EventLoop()
{
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
            if (mEventEntries[i].lpOverlapped->Offset == EventLoop::OVL_RECV)
            {
                ds->canRecv();
            }
            else if (mEventEntries[i].lpOverlapped->Offset == EventLoop::OVL_SEND)
            {
                ds->canSend();
            }
            else if (mEventEntries[i].lpOverlapped->Offset == EventLoop::OVL_CLOSE)
            {
                ds->onClose();
            }
            else
            {
                assert(false);
            }
        }
    }
    else
    {
        //cout << this << " error code:" << GetLastError() << endl;
    }
#else
    int numComplete = epoll_wait(mEpollFd, mEventEntries, mEventEntriesNum, timeout);
    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    for (int i = 0; i < numComplete; ++i)
    {
        Channel*   ds = (Channel*)(mEventEntries[i].data.ptr);
        uint32_t event_data = mEventEntries[i].events;

        if (event_data & EPOLLRDHUP)
        {
            ds->canRecv();
            ds->onClose();   /*  ���������öϿ������Է�canRecv��û��recv �Ͽ�֪ͨ*/
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

    {
        /*TODO::����mAsyncProcs֮ǰ����mAfterLoopProcs����ʱ���������жϿ��ص�����������ϲ�Ͷ���첽�رգ����ͷ�DataSocket��֮���ٻص�disconnectʱ�������׳��ֶϴ���*/
        copyAfterLoopProcs.swap(mAfterLoopProcs);
        for (auto& x : copyAfterLoopProcs)
        {
            x();
        }
        copyAfterLoopProcs.clear();
    }

    {
        std::vector<USER_PROC> temp;
        mAsyncListMutex.lock();
        temp.swap(mAsyncProcs);
        mAsyncListMutex.unlock();

        for (auto& x : temp)
        {
            x();
        }
    }

    {
        /*TODO::��������mAsyncProcs���д�������Ϊ�����ܻ���mAfterLoopProcs���flush send����*/
        copyAfterLoopProcs.swap(mAfterLoopProcs);
        for (auto& x : copyAfterLoopProcs)
        {
            x();
        }
        copyAfterLoopProcs.clear();

        /*��������mAfterLoopProcs������Ϊ������ı����п��ܻ��ռ���socket �Ͽ���������mAfterLoopProcs����˶Ͽ��ص�����*/
        copyAfterLoopProcs.swap(mAfterLoopProcs);
        for (auto& x : copyAfterLoopProcs)
        {
            x();
        }
        copyAfterLoopProcs.clear();

        assert(mAfterLoopProcs.empty());
    }

    if (numComplete == mEventEntriesNum)
    {
        /*  ����¼���������ˣ��������¼����д�С */
        recalocEventSize(mEventEntriesNum + 128);
    }
}

bool EventLoop::wakeup()
{
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
            PostQueuedCompletionStatus(mIOCP, 0, (ULONG_PTR)mWakeupChannel, &mWakeupOvl);
#else
            uint64_t one = 1;
            ssize_t n = write(mWakeupFd, &one, sizeof one);
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
                mFlagMutex.lock();

                if (!mIsAlreadyPostedWakeUp)
                {
                    mIsAlreadyPostedWakeUp = true;
#ifdef PLATFORM_WINDOWS
                    PostQueuedCompletionStatus(mIOCP, 0, (ULONG_PTR)mWakeupChannel, &mWakeupOvl);
#else
                    uint64_t one = 1;
                    ssize_t n = write(mWakeupFd, &one, sizeof one);
#endif
                    ret = true;
                }

                mFlagMutex.unlock();
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
        /*TODO::Ч���Ƿ�����Ż�������߳�ͬʱ����첽��������������Ч���½�*/
        mAsyncListMutex.lock();
        mAsyncProcs.push_back(f);
        mAsyncListMutex.unlock();

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

void EventLoop::restoreThreadID()
{
    mSelfThreadid = std::this_thread::get_id();
}

#ifdef PLATFORM_WINDOWS
HANDLE EventLoop::getIOCPHandle() const
{
    return mIOCP;
}
#endif

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