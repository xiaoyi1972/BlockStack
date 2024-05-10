#include "app.h"
#pragma comment (lib ,"imm32.lib")

// Forward declarations of functions included in this code module:
#define USE_CONSOLE

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	FILE* stream;

#ifdef USE_CONSOLE
	AllocConsole();
	freopen_s(&stream, "CONOUT$", "w+t", stdout);
	freopen_s(&stream, "CONIN$", "r+t", stdin);
#endif


	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	chrome::Application application;
	return application.execute();
}


