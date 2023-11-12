#pragma once

class Timer
{
public:
	Timer();
	~Timer() = default;

	void MarkTime();
	double PeekTime() const;
	float GetDelta();

private:
	__int64 mMarkedTime = 0;
	__int64 mLastDelta = 0;
	double mSecondsPerFrequency = 0.0;
};
