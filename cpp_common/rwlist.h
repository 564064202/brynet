#ifndef _RWLIST_H
#define _RWLIST_H

#include <mutex>
#include <condition_variable>
#include <deque>

template<typename T>
class Rwlist
{
public:
    typedef std::deque<T>   Container;

    Rwlist() : mLock(mMutex, std::defer_lock)
    {}

    void    Push(T& t)
    {
        mWriteList.push_back(t);
    }

    void    Push(T&& t)
    {
        mWriteList.push_back(t);
    }

    /*  ͬ��д���嵽�������(������б���Ϊ��)    */
    void    TrySyncWrite()
    {
        if (!mWriteList.empty() && mSharedList.empty())
        {
            mLock.lock();

            mSharedList.swap(mWriteList);
            mCond.notify_one();

            mLock.unlock();
        }
    }

    /*  ǿ��ͬ��    */
    void    ForceSyncWrite()
    {
        if (!mWriteList.empty())
        {
            if (mSharedList.empty())
            {
                /*  ����������Ϊ�գ�����н���  */
                TrySyncWrite();
            }
            else
            {
                mLock.lock();

                /*  ǿ��д��    */
                if (mWriteList.size() > mSharedList.size())
                {
                    for (auto x : mSharedList)
                    {
                        mWriteList.push_front(x);
                    }

                    mSharedList.clear();
                    mSharedList.swap(mWriteList);
                }
                else
                {
                    for (auto x : mWriteList)
                    {
                        mSharedList.push_back(x);
                    }

                    mWriteList.clear();
                }

                mLock.unlock();
            }
        }
    }

    T&      PopFront()
    {
        if (!mReadList.empty())
        {
            T& ret = mReadList.front();
            mReadList.pop_front();
            return ret;
        }
        else
        {
            return *(T*)nullptr;
        }
    }

    T&      PopBack()
    {
        if (!mReadList.empty())
        {
            T& ret = mReadList.back();
            mReadList.pop_back();
            return ret;
        }
        else
        {
            return *(T*)nullptr;
        }
    }

    /*  �ӹ������ͬ������������(�����������Ϊ��ʱ) */
    void    SyncRead(int waitMicroSecond)
    {
        if (mReadList.empty())
        {
            mLock.lock();

            if (mSharedList.empty() && waitMicroSecond > 0)
            {
                /*  ����������û��������timeout����0����Ҫ�ȴ�֪ͨ,����ֱ�ӽ���ͬ��    */
                mCond.wait_for(mLock, std::chrono::microseconds(waitMicroSecond), [](){return false; });
            }

            if (!mSharedList.empty())
            {
                mSharedList.swap(mReadList);
            }

            mLock.unlock();
        }
    }

    size_t  ReadListSize() const
    {
        return mReadList.size();
    }

    size_t  WriteListSize() const
    {
        return mWriteList.size();
    }

private:
    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::condition_variable         mCond;

    /*  д���� */
    Container                       mWriteList;
    /*  �������    */
    Container                       mSharedList;
    /*  ��������    */
    Container                       mReadList;
};

#endif