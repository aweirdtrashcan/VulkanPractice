#pragma once

#include <Windows.h>
#include <cassert>

class Window
{
public:
	Window(const wchar_t* name, int width, int height);
	~Window();

	int ProcessMessages(bool& shouldLeave);

	_NODISCARD HWND GetHandleToWindow() const { return mHwnd; }
	_NODISCARD HINSTANCE GetWindowInstance() const { return mInstance; }
	_NODISCARD int GetWindowWidth() const { return mWidth; }
	_NODISCARD int GetWindowHeight() const { return mHeight; }
	void Render();
	_NODISCARD inline double GetDeltaTime()
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&mCurrentTime);
		__int64 deltaUnits = mCurrentTime - mLastTime;
		mLastTime = mCurrentTime;
		return (double)deltaUnits * mSecondsPerCount;
	}


private:
	static LRESULT __stdcall MainWndProcPassThrough(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		static Window* wnd = nullptr;
		if (!wnd)
		{
			if (msg == WM_NCCREATE)
			{
				LPCREATESTRUCT createStruct = (LPCREATESTRUCT)lParam;
				assert(createStruct->lpCreateParams != nullptr &&
					"create params is null.");
				wnd = (Window*)createStruct->lpCreateParams;
			}
		}
		return wnd->MainWndProc(hWnd, msg, wParam, lParam);
	}
	LRESULT __stdcall MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	MSG msg = { 0 };
	HWND mHwnd = 0;
	HINSTANCE mInstance = 0;
	const wchar_t* mWindowName;
	int mWidth;
	int mHeight;

	double mSecondsPerCount = 0;
	__int64 mCurrentTime = 0;
	__int64 mLastTime = 0;
	class Renderer* mRenderer = nullptr;
};

