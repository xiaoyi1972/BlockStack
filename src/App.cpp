// Tetris.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Engine.h"
#include "App.h"

#pragma comment(lib, "d2d1")
#pragma comment (lib ,"imm32.lib")

// Forward declarations of functions included in this code module:

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	if (SUCCEEDED(CoInitialize(NULL)))
	{
		MainApp app;

		if (SUCCEEDED(app.Initialize()))
		{
			app.RunMessageLoop();
		}

		CoUninitialize();
	}

	return 0;
}


MainApp::MainApp() : m_hwnd(NULL)
{
	constexpr bool USE_CONSOLE = true ^ 1;
	if constexpr (USE_CONSOLE) {
		FILE* stream1;
		AllocConsole();
		freopen_s(&stream1, "CONOUT$", "w+t", stdout);
		freopen_s(&stream1, "CONIN$", "r+t", stdin);
	}
	engine = new Engine();
}


MainApp::~MainApp()
{
}


void MainApp::RunMessageLoop()
{
	MSG msg;
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	int frames = 0;
	double framesTime = 0, lastElapsedTime = 0;
	boolean running = true;
	
	while (running)
	{
		end = std::chrono::steady_clock::now();
		std::chrono::duration<double> deltaPoint = end - begin;
		double elapsed_secs = deltaPoint.count(); 
		begin = end;
		framesTime += elapsed_secs; // 统计FPS
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // 处理消息和用户输入
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT) running = false;
		}
		auto delta = framesTime - lastElapsedTime;
		engine->Logic(elapsed_secs); // 游戏逻辑call
		if (engine->Draw(delta) == true) { // 绘图call
			lastElapsedTime = framesTime;
			frames++;
		}
		if (framesTime > 1) {
			WCHAR fpsText[32];
			swprintf(fpsText, 32, L"Game: %d FPS", frames);
			SetWindowText(m_hwnd, fpsText);
			frames = lastElapsedTime = framesTime = 0;
		}
		}
}

HRESULT MainApp::Initialize()
{
	HRESULT hr = S_OK;
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) }; // Register the window class.
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = MainApp::WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = HINST_THISCOMPONENT;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = NULL;
	wcex.hCursor = LoadCursor(NULL, IDI_APPLICATION);
	wcex.lpszClassName = L"D2DMainApp";
	ATOM x = RegisterClassEx(&wcex);
	m_hwnd = CreateWindowEx( // Create the window.
		NULL,
		L"D2DMainApp",
		L"Game",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		RESOLUTION_X,
		RESOLUTION_Y,
		NULL,
		NULL,
		HINST_THISCOMPONENT,
		this
	);
	hr = m_hwnd ? S_OK : E_FAIL;
	RECT rect1,rect2; 	// Adjust it so the client area is RESOLUTION_X/RESOLUTION_Y
	GetWindowRect(m_hwnd, &rect1);
	GetClientRect(m_hwnd, &rect2);
	SetWindowPos(
		m_hwnd,
		NULL,
		rect1.left,
		rect1.top,
		RESOLUTION_X + ((rect1.right - rect1.left) - (rect2.right - rect2.left)),
		RESOLUTION_Y + ((rect1.bottom - rect1.top) - (rect2.bottom - rect2.top)),
		NULL
	);

	if (SUCCEEDED(hr))
	{
		engine->InitializeD2D(m_hwnd);
		ShowWindow(m_hwnd, SW_SHOWNORMAL);
		UpdateWindow(m_hwnd);
	}

	ImmAssociateContext(m_hwnd, NULL); //ban imme
	return hr;
}

LRESULT CALLBACK MainApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

	if (message == WM_CREATE)
	{
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		MainApp* pMainApp = (MainApp*)pcs->lpCreateParams;

		::SetWindowLongPtrW(
			hwnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(pMainApp)
		);

		result = 1;
	}
	else
	{
		MainApp* pMainApp = reinterpret_cast<MainApp*>(static_cast<LONG_PTR>(
			::GetWindowLongPtrW(
				hwnd,
				GWLP_USERDATA
			)));

		bool wasHandled = false;

		if (pMainApp)
		{
			switch (message)
			{

			case WM_DISPLAYCHANGE:
			{
				InvalidateRect(hwnd, NULL, FALSE);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_KEYDOWN:
			{
				pMainApp->engine->KeyDown(wParam);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_KEYUP:
			{
				pMainApp->engine->KeyUp(wParam);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_MOUSEMOVE:
			{
				pMainApp->engine->MousePosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_LBUTTONUP:
			{
				pMainApp->engine->MouseButtonUp(true, false);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_LBUTTONDOWN:
			{
				pMainApp->engine->MouseButtonDown(true, false);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_RBUTTONUP:
			{
				pMainApp->engine->MouseButtonUp(false, true);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_RBUTTONDOWN:
			{
				pMainApp->engine->MouseButtonDown(false, true);
			}
			result = 0;
			wasHandled = true;
			break;

			case WM_DESTROY:
			{
				PostQuitMessage(0);
			}
			result = 1;
			wasHandled = true;
			break;
			}
		}

		if (!wasHandled)
		{
			result = DefWindowProc(hwnd, message, wParam, lParam);
		}
	}

	return result;
}
