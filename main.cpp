#include "Window.h"

#include "EngineException.h"

int main(void)
{
	bool shouldLeave = false;
	int returnCode = 0;

	try
	{
		Window window(L"Vulkan Application", 1280, 600);

		while (!shouldLeave)
		{
			returnCode = window.ProcessMessages(shouldLeave);
			window.Render();
		}
	}
	catch (const EngineException& e)
	{
		MessageBoxA(nullptr, e.what(), e.GetType(), MB_OK | MB_ICONEXCLAMATION);
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONEXCLAMATION);
	}
	return returnCode;
}