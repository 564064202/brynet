#include "Timer.h"

using namespace brynet;

const std::chrono::steady_clock::time_point& Timer::getEndMs() const
{
    return mEndTime;
}

void Timer::cancel()
{
    mActive = false;
}

Timer::Timer(std::chrono::steady_clock::time_point endTime, Callback callback) noexcept : mEndTime(endTime), mCallback(callback)
{
    mActive = true;
}

void Timer::operator() ()
{
    if (mActive)
    {
        mCallback();
    }
}

void TimerMgr::schedule()
{
    while (!mTimers.empty())
    {
        auto tmp = mTimers.top();

        if (tmp->getEndMs() < std::chrono::steady_clock::now())
        {
            mTimers.pop();
            tmp->operator() ();
        }
        else
        {
            break;
        }
    }
}

bool TimerMgr::isEmpty() const
{
    return mTimers.empty();
}

/* ���ض�ʱ���������������һ����ʱ�������õ���(�����ʱ��Ϊ�ջ��Ѿ������򷵻�0) */
std::chrono::milliseconds TimerMgr::nearEndMs() const
{
    if (mTimers.empty())
    {
        return std::chrono::milliseconds();
    }

    auto result = std::chrono::duration_cast<std::chrono::milliseconds>(mTimers.top()->getEndMs() - std::chrono::steady_clock::now());
    if (result.count() < 0)
    {
        return std::chrono::milliseconds();
    }

    return result;
}

void TimerMgr::clear()
{
    while (!mTimers.empty())
    {
        mTimers.pop();
    }
}