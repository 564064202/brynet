#ifndef _RWLIST_H
#define _RWLIST_H

#include <mutex>
#include <condition_variable>

template<typename T>
class Rwlist
{
public:
	typedef std::deque<T>	Container;

    Rwlist() : mLock(mMutex, std::defer_lock)
    {}

	void	Push(T& t)
	{
		mWriteList.push_back(t);
	}

	void	Push(T&& t)
	{
		mWriteList.push_back(t);
	}

    T&	PopFront()
	{
        if (!mReadList.empty())
        {
            T& ret = *(mReadList.begin());
            mReadList.pop_front();
            return ret;
        }
        else
        {
            return *(T*)nullptr;
        }
	}

	/*	ͬ��д���嵽�������(������б���Ϊ��)	*/
	void	SyncWrite()
	{
		if (mSharedList.empty())
		{
            mLock.lock();

			mSharedList.swap(mWriteList);
            mCond.notify_one();

            mLock.unlock();
		}
	}

	/*	�ӹ������ͬ������������(�����������Ϊ��ʱ)	*/
    void	SyncRead(int waitMicroSecond)
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

private:
    std::mutex                      mMutex;
    std::unique_lock<std::mutex>    mLock;
    std::condition_variable         mCond;

	/*	д����	*/
	Container	                    mWriteList;
	/*	�������	*/
	Container	                    mSharedList;
	/*	��������	*/
	Container	                    mReadList;
};

#endif