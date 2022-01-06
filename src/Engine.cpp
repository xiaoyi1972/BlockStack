#include "framework.h"
#include "Engine.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "Windowscodecs.lib")

Engine::Engine() : m_pDirect2dFactory(NULL), m_pRenderTarget(NULL)
{
	// Constructor
	// Initialize your game elements here
	srand(time(NULL));
}

Engine::~Engine()
{
	// Destructor

	SafeRelease(&m_pDirect2dFactory);
	SafeRelease(&m_pRenderTarget);

	// Safe-release your game elements here
}

HRESULT Engine::InitializeD2D(HWND m_hwnd)
{
	// Initializes Direct2D, for drawing
	D2D1_SIZE_U size = D2D1::SizeU(RESOLUTION_X, RESOLUTION_Y);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);
	m_pDirect2dFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(m_hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
		&m_pRenderTarget
	);
	m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	// Initialize the D2D part of your game elements here
	InitializeTextAndScore();
	tetris.InitializeD2D(m_pRenderTarget);
	return S_OK;
}

void Engine::InitializeTextAndScore()
{
	DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(m_pDWriteFactory),
		reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
	);

	m_pDWriteFactory->CreateTextFormat(
		L"Verdana",
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		20,
		L"", //locale
		&m_pTextFormat
	);

	m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

	m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	m_pRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White),
		&m_pWhiteBrush
	);
}

void Engine::KeyUp(WPARAM wParam)
{
	if (wParam == VK_DOWN) downPressed = false;
	else if (wParam == VK_LEFT) {
		leftPressed = -1;
		leftTimer = autoMoveLeft = switchLeft = 0;
		if (switchRight) {
			rightTimer = autoMoveRight = switchRight = 0;
			rightPressed = 1;
		}
	}
	else if (wParam == VK_RIGHT) {
		rightPressed = -1;
		rightTimer = autoMoveRight = switchRight = 0;
		if (switchLeft) {
			leftTimer = autoMoveLeft = switchLeft = 0;
			leftPressed = 1;
		}
	}
	else if (wParam == VK_SPACE) spacePressed = -1;
	else if (wParam == 'Z') cwPressed = -1;
	else if (wParam == 'X') ccwPressed = -1;
	else if (wParam == 'C') holdPressed = -1;
	else if (wParam == 'R') restartPressed = -1;
}

void Engine::KeyDown(WPARAM wParam)
{
	if (wParam == VK_DOWN) downPressed = true;
	else if (wParam == VK_LEFT && leftPressed == -1) {
		if (rightPressed != -1) {
			rightPressed = -1;
			switchRight = 1;
			rightTimer = autoMoveRight = 0;
		}
		leftPressed = 1;
	}
	else if (wParam == VK_RIGHT && rightPressed == -1) {
		if (leftPressed != -1) {
			leftPressed = -1;
			switchLeft = 1;
			leftTimer = autoMoveLeft = 0;
		}
		rightPressed = 1;
	}
	else if (wParam == VK_SPACE && spacePressed == -1) spacePressed = 1;
	else if (wParam == 'Z' && cwPressed == -1) cwPressed = 1;
	else if (wParam == 'X' && ccwPressed == -1) ccwPressed = 1;
	else if (wParam == 'C' && holdPressed == -1) holdPressed = 1;
	else if (wParam == 'W') {
		if (botPressed == -1) botPressed = 1;
		else botPressed = -1;
	}
	else if (wParam == 'R') restartPressed = 1;
}

void Engine::MousePosition(int x, int y)
{
	// Campture mouse position when the mouse moves
	// Don't do any logic here, you want to control the actual logic in the Logic method below
}

void Engine::MouseButtonUp(bool left, bool right)
{
	// If mouse button is released, set the mouse flags
	// Don't do any logic here, you want to control the actual logic in the Logic method below
}

void Engine::MouseButtonDown(bool left, bool right)
{
	// If mouse button is pressed, set the mouse flags
	// Don't do any logic here, you want to control the actual logic in the Logic method below
}

void Engine::Logic(double elapsedTime)
{
	if (gameOver) // Do nothing if game over
		return;

	if (hao && botReady == -1) {
		botReady = 0;
		hao = false;
	}

	if (botReady != -1) {
		botTimer += elapsedTime;
		if (botReady == botOperSource.size() || botTimer > (botOperSource[botReady] == Oper::SoftDrop ? 0.014  : 0.025)) {
			tetris.opers(botOperSource[botReady++]);
			if (botReady == botOperSource.size()) {
				botReady = -1;
				botOperSource.clear();
				if (botPressed == 0) {
					botPressed = 1;
				}
			}
			botTimer = 0;
		}
	}

	if (spacePressed == 1) {
		tetris.opers(Oper::HardDrop);
		spacePressed = 0;
	}
	if (leftPressed != -1) { 	// Move left or right
		leftTimer += elapsedTime;
		if (leftPressed == 1) {
			tetris.opers(Oper::Left);
			leftPressed = 0;
		}
		else if (leftPressed == 0) {
			if (leftTimer >= das && !autoMoveLeft) {
				autoMoveLeft = 1;
				leftTimer = 0;
			}
			else if (autoMoveLeft) {
				if (leftTimer >= arr) {
					tetris.opers(Oper::Left);
					leftTimer = 0;
				}
			}
		}
	}
	if (rightPressed != -1) {
		rightTimer += elapsedTime;
		if (rightPressed == 1) {
			tetris.opers(Oper::Right);
			rightPressed = 0;
		}
		else if (rightPressed == 0) {
			if (rightTimer >= das && !autoMoveRight) {
				autoMoveRight = 1;
				rightTimer = 0;
			}
			else if (autoMoveRight) {
				if (rightTimer >= arr) {
					tetris.opers(Oper::Right);
					rightTimer = 0;
				}
			}
		}
	}
	if (downPressed) {
		tetris.opers(Oper::SoftDrop);
	}
	if (cwPressed == 1) {
		tetris.opers(Oper::Cw);
		cwPressed = 0;
	}
	if (ccwPressed == 1) {
		tetris.opers(Oper::Ccw);
		ccwPressed = 0;
	}
	if (holdPressed == 1) {
		tetris.opers(Oper::Hold);
		holdPressed = 0;
	}
	if (botPressed == 1) {
		constexpr int time = 130;
		test = pool.enqueue(
			[&](auto time) {
				botOperSource = std::move(tetris.callBot(time));
				hao = true; }, time);
		botPressed = 0;
	}
	if (restartPressed == 1) {
		tetris.restart();
		restartPressed = 0;
	}
}

bool Engine::Draw(double elapsedTime)
{

	if (elapsedTime < interval) {
		return false;
	}
	// This is the drawing method of the engine.
	// It runs every frame
	// Draws the elements in the game using Direct2D
	HRESULT hr;
	//m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	m_pRenderTarget->BeginDraw();
	m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

	// Below you can add drawing logic for your game elements
	DrawTextAndScore();
	tetris.Draw(/*m_pRenderTarget, m_pDirect2dFactory*/);

	hr = m_pRenderTarget->EndDraw();
	return true;
}

void Engine::DrawTextAndScore()
{
	/*   // Text and score
	   int constexpr STACK_HEIGHT = 0;
	   int constexpr STACK_WIDTH = 0;
	   int padding = (RESOLUTION_Y - (STACK_HEIGHT + 1) * CELL_SIZE) / 2 - 100;
	   int centerRight = RESOLUTION_X - (RESOLUTION_X - padding - (STACK_WIDTH + 2) * CELL_SIZE) / 2;


	   D2D1_RECT_F rectangle1 = D2D1::RectF(centerRight - 200, padding, centerRight + 200, padding + 100);
	   m_pRenderTarget->DrawText(
		   L"Next Piece",
		   15,
		   m_pTextFormat,
		   rectangle1,
		   m_pWhiteBrush
	   );


	   D2D1_RECT_F rectangle2 = D2D1::RectF(centerRight - 200, padding + 200, centerRight + 200, padding + 300);
	   m_pRenderTarget->DrawText(
		   L"Score",
		   5,
		   m_pTextFormat,
		   rectangle2,
		   m_pWhiteBrush
	   );


	   D2D1_RECT_F rectangle3 = D2D1::RectF(centerRight - 200, padding + 300, centerRight + 200, padding + 400);
	   WCHAR scoreStr[64];
	   swprintf_s(scoreStr, L"%d        ", score);
	   m_pRenderTarget->DrawText(
		   scoreStr,
		   7,
		   m_pTextFormat,
		   rectangle3,
		   m_pWhiteBrush
	   );*/
}
