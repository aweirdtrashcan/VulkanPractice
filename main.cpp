#include "Window.h"

#include <exception>

int main(void)
{
	bool shouldLeave = false;
	int returnCode = 0;

	try
	{
		Window window(L"Vulkan Application", 800, 600);

		while (!shouldLeave)
		{
			returnCode = window.ProcessMessages(shouldLeave);
			window.Render();
		}
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONEXCLAMATION);
	}
	return returnCode;
}