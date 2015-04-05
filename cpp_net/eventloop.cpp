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
    void    canRecv() override
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

    void    canSend() override
    {
    }

    void    setEventLoop(EventLoop*) override
    {
    }

    void    onClose() override
    {
    }

private:
    int     mFd;
};

EventLoop::EventLoop()
{
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
    linkChannel(mWakeupFd, mWakeupChannel);
#endif

    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

    mEventEntries = NULL;
    mEventEntriesNum = 0;

    recalocEventSize(1024);
    mSelfThreadid = 0;
}

EventLoop::~EventLoop()
{
    delete mWakeupChannel;
    mWakeupChannel = nullptr;
#ifdef PLATFORM_WINDOWS
    CloseHandle(mIOCP);
    mIOCP = INVALID_HANDLE_VALUE;
#else
    close(mWakeupFd);
    mWakeupFd = -1;
    close(mEpollFd);
    mEpollFd = -1;
#endif
    delete[] mEventEntries;
    mEventEntries = nullptr;
}

void EventLoop::loop(int64_t timeout)
{
#ifndef NDEBUG
    assert(isInLoopThread());
#endif
    /*  warn::���mAfterLoopProcs��Ϊ�գ�Ŀǰ������һ��loop(ʱ��֮ǰ������˻ص�������timeout��Ϊ0����ʾ������iocp/epoll wait   */
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
            ds->onClose();   /*  ���������öϿ�����(��ȫ�ģ���������ظ�close)���Է�canRecv��û��recv �Ͽ�֪ͨ*/
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

    /*
    warn::  ��ʱmAfterLoopProcs������ܰ����˶Ͽ��ص�����mAsyncProcs��Ҳ�������첽�Ͽ�����
    �ڴ���mAsyncProcs�ĶϿ������л��ͷ�Channel����˱����ȴ���mAfterLoopProcs����ִ�жϿ��ص��������mAsyncProcs֮����mAfterLoopProcs��
    �ͻ���ֶδ���
    */
    processAfterLoopProcs();

    /*  ִ���첽����  */
    processAsyncProcs();

    /*  warn::  �����Ͽ��Բ������������mAfterLoopProcs��������Ϊloop����֮ǰ���ж�mAfterLoopProcs�Ƿ�Ϊ�ա������Ϊ�գ��򲻻�����epoll��iocp��������ֱ����epoll/iocp���ؿ��ܻ��˷�һЩCPU����Ҫ������ԣ�    */

    /*  warn::  ��������mAfterLoopProcs������Ϊ��ִ���첽�������ʱ���ܻ���mAfterLoopProcs���flush send�Ȼص�  */
    processAfterLoopProcs();
    /*  warn::  ��������mAfterLoopProcs������Ϊ������ı���(ִ�лص����п��ܻ��ռ���socket �Ͽ���������mAfterLoopProcs����˶Ͽ��ص�����*/
    processAfterLoopProcs();

    assert(mAfterLoopProcs.empty());

    if (numComplete == mEventEntriesNum)
    {
        /*  ����¼���������ˣ��������¼�������д�С��������һ��epoll/iocp wait��þ����ܸ����֪ͨ */
        recalocEventSize(mEventEntriesNum + 128);
    }
}

void EventLoop::processAfterLoopProcs()
{
    copyAfterLoopProcs.swap(mAfterLoopProcs);
    for (auto& x : copyAfterLoopProcs)
    {
        x();
    }
    copyAfterLoopProcs.clear();
}

void EventLoop::processAsyncProcs()
{
    std::vector<USER_PROC> temp;
    mAsyncProcsMutex.lock();
    temp.swap(mAsyncProcs);
    mAsyncProcsMutex.unlock();

    for (auto& x : temp)
    {
        x();
    }
}

bool EventLoop::isInLoopThread()
{
    return mSelfThreadid == CurrentThread::tid();
}

bool EventLoop::wakeup()
{
    bool ret = false;
    /*  TODO::1����֤�̰߳�ȫ��2����֤io�̴߳���wait״̬ʱ����羡�����ٷ���wakeup��3���Ƿ�io�߳��б�Ҫ�Լ�������Ͷ��һ��wakeup��������һ��wait�� */
    if (!isInLoopThread())
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
            /*���һ��û�д���epoll/iocp wait*/
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

void EventLoop::linkChannel(int fd, Channel* ptr)
{
#ifdef PLATFORM_WINDOWS
    HANDLE ret = CreateIoCompletionPort((HANDLE)fd, mIOCP, (DWORD)ptr, 0);
#else
    struct epoll_event ev = { 0, { 0 } };
    ev.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
    ev.data.ptr = ptr;
    int ret = epoll_ctl(mEpollFd, EPOLL_CTL_ADD, fd, &ev);
#endif
}

void EventLoop::addChannel(int fd, Channel* c, CHANNEL_ENTER_HANDLE f)
{
    /*  TODO::�˴���ش����Լ�Channel�ӿ����̫�鷳,�������   */
    pushAsyncProc([fd, c, this, f] () {
        c->setNoBlock();
        linkChannel(fd, c);
        c->setEventLoop(this);
        f(c);
    });
}

void EventLoop::pushAsyncProc(const USER_PROC& f)
{
    if (!isInLoopThread())
    {
        /*TODO::Ч���Ƿ�����Ż�������߳�ͬʱ����첽��������������Ч���½�*/
        mAsyncProcsMutex.lock();
        mAsyncProcs.push_back(f);
        mAsyncProcsMutex.unlock();

        wakeup();
    }
    else
    {
        f();
    }
}

void EventLoop::pushAsyncProc(USER_PROC&& f)
{
    CurrentThread::THREAD_ID_TYPE fuck = CurrentThread::tid();
    if (!isInLoopThread())
    {
        /*TODO::Ч���Ƿ�����Ż�������߳�ͬʱ����첽��������������Ч���½�*/
        mAsyncProcsMutex.lock();
        mAsyncProcs.push_back(std::move(f));
        mAsyncProcsMutex.unlock();

        wakeup();
    }
    else
    {
        f();
    }
}

void EventLoop::pushAfterLoopProc(const USER_PROC& f)
{
    mAfterLoopProcs.push_back(f);
}

void EventLoop::pushAfterLoopProc(USER_PROC&& f)
{
    mAfterLoopProcs.push_back(std::move(f));
}

void EventLoop::restoreThreadID()
{
    mSelfThreadid = CurrentThread::tid();
}

#ifdef PLATFORM_WINDOWS
HANDLE EventLoop::getIOCPHandle() const
{
    return mIOCP;
}
#else
int EventLoop::getEpollHandle() const
{
    return mEpollFd;
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