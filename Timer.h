#pragma once

class Timer
{
public:
	Timer();
	~Timer() = default;

	void MarkTime();
	double PeekTime() const;
	double GetDelta();
	template<typename T>
	T GetDeltaT();

private:
	__int64 mMarkedTime = 0;
	__int64 mLastDelta = 0;
	double mSecondsPerFrequency = 0.0;
};

template<typename T>
inline T Timer::GetDeltaT()
{
	return T(GetDelta());
}
