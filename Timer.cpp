#include "Timer.h"

#include <Windows.h>

Timer::Timer()
{
	__int64 perfFrequency = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&perfFrequency);
	mSecondsPerFrequency = (double)1.0 / perfFrequency;
}

void Timer::MarkTime()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&mMarkedTime);
}

double Timer::PeekTime() const
{
	__int64 currTime = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&currTime);

	return (double)(currTime - mMarkedTime) * mSecondsPerFrequency;
}

double Timer::GetDelta()
{
	__int64 currDelta = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&currDelta);
	double dDelta = static_cast<double>(currDelta - mLastDelta) * mSecondsPerFrequency;
	mLastDelta = currDelta;
	return dDelta;
}
