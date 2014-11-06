#include "eventloop.h"
#include "channel.h"
#include <iostream>
#include <assert.h>
#include <thread>

using namespace std;

EventLoop::EventLoop() : mLock(mMutex, std::defer_lock)
{
    mSelfThreadid = std::this_thread::get_id();
    mPGetQueuedCompletionStatusEx = NULL;
    HMODULE kernel32_module = GetModuleHandleA("kernel32.dll");
    if (kernel32_module != NULL) {
        mPGetQueuedCompletionStatusEx = (sGetQueuedCompletionStatusEx)GetProcAddress(
            kernel32_module,
            "GetQueuedCompletionStatusEx");
    }

    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;
    mIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);

    mEventEntries = NULL;
    mEventEntriesNum = 0;

    recalocEventSize(1024);
}

void EventLoop::loop(int64_t timeout)
{
    /*  TODO::��ֹ��loop֮ǰ�������һЩfunction�ص�����û���µĲ���wakeup��loop������ */
    if (!mAfterLoopProcs.empty())
    {
        timeout = 0;
    }

    ULONG numComplete = 0;
    mInWaitIOEvent = true;
    BOOL rc = mPGetQueuedCompletionStatusEx(mIOCP, mEventEntries, mEventEntriesNum, &numComplete, static_cast<DWORD>(timeout), false);

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
        cout << "iocp error" << endl;
    }

    mIsAlreadyPostedWakeUp = false;
    mInWaitIOEvent = false;

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

void EventLoop::wakeup()
{
    /*  TODO::1����֤�̰߳�ȫ��2����֤io�̴߳���wait״̬ʱ��������ֻ�ܷ���һ��wakeup�������ǿ��Զ��Ͷ�ݣ�3���Ƿ�io�߳��б�Ҫ�Լ�������Ͷ��һ��wakeup��������һ��wait�� */
    if (mSelfThreadid != std::this_thread::get_id())
    {
        if (!mIsAlreadyPostedWakeUp)
        {
            mIsAlreadyPostedWakeUp = true;
            PostQueuedCompletionStatus(mIOCP, 0xDEADC0DE, 0, (LPOVERLAPPED)0xcdcdcdcd);
        }
    }
}

void EventLoop::linkConnection(int fd, Channel* ptr)
{
    HANDLE ret = CreateIoCompletionPort((HANDLE)fd, mIOCP, (DWORD)ptr, 0);
}

void EventLoop::addConnection(int fd, Channel* c, CONNECTION_ENTER_HANDLE f)
{
    pushAsyncProc([fd, c, this, f] () {
            linkConnection(fd, c);
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

    mEventEntries = new OVERLAPPED_ENTRY[size];
    mEventEntriesNum = size;
}