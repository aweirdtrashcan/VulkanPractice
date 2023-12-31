#include "Window.h"

#include "WindowsException.h"

#include "Renderer.h"

#include <windowsx.h>

Window::Window(const wchar_t* name, int _width, int _height)
	:
	mWindowName(name),
	mWidth(_width),
	mHeight(_height)
{
	mInstance = GetModuleHandleA(0);
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProcPassThrough;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		throw WindowsException("RegisterClass Failed.");
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mWidth, mHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mHwnd = CreateWindowW(L"MainWnd", mWindowName,
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mInstance, this);
	if (!mHwnd)
	{
		throw WindowsException("CreateWindow Failed.");
	}

	ShowWindow(mHwnd, SW_SHOW);
	UpdateWindow(mHwnd);

	__int64 performanceFrequency = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&performanceFrequency);
	mSecondsPerCount = 1.0 / performanceFrequency;
	QueryPerformanceCounter((LARGE_INTEGER*)&mCurrentTime);
	QueryPerformanceCounter((LARGE_INTEGER*)&mLastTime);

	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CONOUT$", "w", stdout);

	mRenderer = new Renderer(this);
}

Window::~Window()
{
	delete mRenderer;
}

int Window::ProcessMessages(bool& shouldLeave)
{
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);

		if (msg.message == WM_QUIT)
			shouldLeave = true;
	}

	return static_cast<int>(msg.wParam);
}

void Window::Render()
{
	if (mCanRender)
	{
		mRenderer->Update();
		mRenderer->Draw();
	}
}

LRESULT __stdcall Window::MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			// mAppPaused = true;
			// mTimer.Stop();
		}
		else
		{
			// mAppPaused = false;
			// mTimer.Start();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mWidth = LOWORD(lParam);
		mHeight = HIWORD(lParam);
		mCanRender = false;
		if (wParam == SIZE_MINIMIZED)
		{
			mCanRender = false;
		}
		else if (wParam == SIZE_MAXIMIZED)
		{
			//mRenderer->NotifyWindowResize(mWidth, mHeight);
			mCanRender = true;
		}
		else if (wParam == SIZE_RESTORED)
		{
			//mRenderer->NotifyWindowResize(mWidth, mHeight);
			mCanRender = true;
		}
		//if (md3dDevice)
		//{
		//	if (wParam == SIZE_MINIMIZED)
		//	{
		//		mAppPaused = true;
		//		mMinimized = true;
		//		mMaximized = false;
		//	}
		//	else if (wParam == SIZE_MAXIMIZED)
		//	{
		//		mAppPaused = false;
		//		mMinimized = false;
		//		mMaximized = true;
		//		OnResize();
		//	}
		//	else if (wParam == SIZE_RESTORED)
		//	{

		//		// Restoring from minimized state?
		//		if (mMinimized)
		//		{
		//			mAppPaused = false;
		//			mMinimized = false;
		//			OnResize();
		//		}

		//		// Restoring from maximized state?
		//		else if (mMaximized)
		//		{
		//			mAppPaused = false;
		//			mMaximized = false;
		//			OnResize();
		//		}
		//		else if (mResizing)
		//		{
		//			// If user is dragging the resize bars, we do not resize 
		//			// the buffers here because as the user continuously 
		//			// drags the resize bars, a stream of WM_SIZE messages are
		//			// sent to the window, and it would be pointless (and slow)
		//			// to resize for each WM_SIZE message received from dragging
		//			// the resize bars.  So instead, we reset after the user is 
		//			// done resizing the window and releases the resize bars, which 
		//			// sends a WM_EXITSIZEMOVE message.
		//		}
		//		else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
		//		{
		//			OnResize();
		//		}
		//	}
		//}
		return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		//mAppPaused = true;
		//mResizing = true;
		//mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		//mAppPaused = false;
		//mResizing = false;
		//mTimer.Start();
		//OnResize();
		//mRenderer->NotifyWindowResize(mWidth, mHeight);
		mCanRender = true;
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		mRenderer->WaitForDeviceIdle();
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		//OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		//mRenderer->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		mRenderer->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
		{
			//Set4xMsaaState(!m4xMsaaState);
		}
		else if ((int)wParam == 'V' || (int)wParam == 'v')
		{
			mRenderer->ToggleVSync();
		}
		mRenderer->OnKeyUp((int)wParam);
		return 0;
	case WM_KEYDOWN:
		mRenderer->OnKeyDown((int)wParam);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
