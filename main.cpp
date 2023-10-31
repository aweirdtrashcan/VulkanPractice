#include "Window.h"

#include <exception>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	bool shouldLeave = false;
	int returnCode = 0;

	try
	{
		Window window(L"Vulkan Application", 800, 600);

		while (!shouldLeave)
		{
			returnCode = window.ProcessMessages(shouldLeave);
		}
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONEXCLAMATION);
	}

	return returnCode;
}