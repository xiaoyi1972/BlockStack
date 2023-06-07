#include "framework.h"
#include "engine.h"

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")
#pragma comment(lib, "Windowscodecs.lib")

Engine::Engine() : m_pDirect2dFactory(NULL), m_pRenderTarget(NULL)
{
	// Constructor
	// Initialize your game elements here
	srand(time(NULL));

	m_colors.insert({ Piece::Trash,D2D1::ColorF(115. / 255, 115. / 255, 115. / 255) });
	m_colors.insert({ Piece::None,D2D1::ColorF(255. / 255, 255. / 255, 255.255, 0) });
	m_colors.insert({ Piece::O,D2D1::ColorF(245. / 255, 220. / 255, 0. / 255) });
	m_colors.insert({ Piece::I,D2D1::ColorF(57. / 255, 195. / 255, 199. / 255) });
	m_colors.insert({ Piece::T,D2D1::ColorF(138. / 255, 43. / 255, 227. / 255) });
	m_colors.insert({ Piece::L,D2D1::ColorF(255. / 255, 166. / 255, 0. / 255) });
	m_colors.insert({ Piece::J,D2D1::ColorF(0. / 255, 0. / 255, 255. / 255) });
	m_colors.insert({ Piece::S,D2D1::ColorF(51. / 255, 204. / 255, 51. / 255) });
	m_colors.insert({ Piece::Z,D2D1::ColorF(255. / 255, 0. / 255, 0. / 255) });

	active = &players[0];
	players[0].opponent = &players[1];
	players[0].index = 0;
	players[1].opponent = &players[0];
	players[1].index = 1;
	turn_index = 0;
}

Engine::~Engine()
{
	// Destructor

	SafeRelease(&m_pDirect2dFactory);
	SafeRelease(&m_pRenderTarget);
	SafeRelease(&m_pStateBlock);
	SafeRelease(&m_pBrush);
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

	// Creates a red brush for drawing
	m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &m_pBrush);

	m_pDirect2dFactory->CreateDrawingStateBlock(&m_pStateBlock);

	//tetris.InitializeD2D(m_pRenderTarget);
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
		L"Times New Roman",
		NULL,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		17,
		L"", //locale
		&m_pTextFormat
	);

	m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

	m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	m_pRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::White),
		&m_pWhiteBrush
	);
}

void Engine::KeyUp(WPARAM wParam)
{
	if (wParam == VK_DOWN) 	active->hK.downPressed = false;
	else if (wParam == VK_LEFT) {
		active->hK.leftPressed = -1;
		active->hK.leftTimer = active->hK.autoMoveLeft = active->hK.switchLeft = 0;
		if (active->hK.switchRight) {
			active->hK.rightTimer = active->hK.autoMoveRight = active->hK.switchRight = 0;
			active->hK.rightPressed = 1;
		}
	}
	else if (wParam == VK_RIGHT) {
		active->hK.rightPressed = -1;
		active->hK.rightTimer = active->hK.autoMoveRight = active->hK.switchRight = 0;
		if (active->hK.switchLeft) {
			active->hK.leftTimer = active->hK.autoMoveLeft = active->hK.switchLeft = 0;
			active->hK.leftPressed = 1;
		}
	}
	else if (wParam == VK_SPACE) 	active->hK.spacePressed = -1;
	else if (wParam == 'Z') 	active->hK.cwPressed = -1;
	else if (wParam == 'X') 	active->hK.ccwPressed = -1;
	else if (wParam == 'C') 	active->hK.holdPressed = -1;
	else if (wParam == 'R') 	active->hK.restartPressed = -1;
}

void Engine::KeyDown(WPARAM wParam)
{
	if (wParam == VK_DOWN) 	active->hK.downPressed = true;
	else if (wParam == VK_LEFT && active->hK.leftPressed == -1) {
		if (active->hK.rightPressed != -1) {
			active->hK.rightPressed = -1;
			active->hK.switchRight = 1;
			active->hK.rightTimer = active->hK.autoMoveRight = 0;
		}
		active->hK.leftPressed = 1;
	}
	else if (wParam == VK_RIGHT && active->hK.rightPressed == -1) {
		if (active->hK.leftPressed != -1) {
			active->hK.leftPressed = -1;
			active->hK.switchLeft = 1;
			active->hK.leftTimer = active->hK.autoMoveLeft = 0;
		}
		active->hK.rightPressed = 1;
	}
	else if (wParam == VK_SPACE && active->hK.spacePressed == -1) 	active->hK.spacePressed = 1;
	else if (wParam == 'Z' && active->hK.cwPressed == -1) 	active->hK.cwPressed = 1;
	else if (wParam == 'X' && active->hK.ccwPressed == -1)   active->hK.ccwPressed = 1;
	else if (wParam == 'C' && active->hK.holdPressed == -1)   active->hK.holdPressed = 1;
	else if (wParam == 'W') {
#define With(active) \
		if (active->hK.botPressed == -1) \
			active->hK.botPressed = 1; \
		else active->hK.botPressed = -1;
		//for (auto& player : players) 
			//With((&player));
		With(active)
#undef With
	}
	else if (wParam == 'R') {
#define With(active) active->hK.restartPressed = 1;
		for (auto& player : players)
			With((&player));
#undef With
	}
	else if (wParam == 'P') {
		if (active == &players[1])
			active = &players[0];
		else active = &players[1];
	}
	else if (wParam == 'M') {
#define With(active) active->restart();
		for (auto& player : players)
			With((&player));
#undef With
		//players[0].hK.botPressed = 1;
	    players[1].hK.botPressed = 1;
		turn_index = 0;
	}
}

void Engine::Command(WPARAM wParam) {
	switch (wParam) {
	case 2:active->switchMode(0); break;
	case 3:active->switchMode(1); break;
	case 4:active->switchMode(2); break;
	}
	active->restart();
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

template<>
void Engine::draw_with<MyTetris>(MyTetris& object, const D2D1::Matrix3x2F& view_offset) {
	const auto& map = object.map;
	const auto& hS = object.hS;
	const auto& rS = object.rS;
	auto& tn = object.tn;
	auto& gd = object.gd;

	const int gridSize = CELL_SIZE;
	constexpr int offsetY = 2;
	auto h = map.height >= 20 ? 20 : map.height;
	int margin_x = (RESOLUTION_X - map.width * gridSize) / 2;
	int margin_y = (RESOLUTION_Y - (h + 2) * gridSize) / 2;
	int troughWidth = 10;
	auto split = (RESOLUTION_X - 2 * (map.width + 8) * gridSize) / 3;

	auto drawField = [&]() {
		D2D1_RECT_F border_rect = D2D1::RectF(0 + margin_x, 0 + margin_y + 2 * gridSize, 0, 0);
		border_rect.right = border_rect.left + map.width * gridSize;
		border_rect.bottom = border_rect.top + (h)*gridSize;
		m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		m_pRenderTarget->SetTransform(view_offset);
		m_pRenderTarget->DrawRectangle(&border_rect, m_pBrush);
		for (int y = 0; y < h; y++) //draw field
			for (int x = 0; x < map.width; x++) {
				if (map(x, y)) {
					m_pBrush->SetColor(getColors(map.colorDatas[y][x]));
					auto py = (h - 1 - y + offsetY);
					D2D1_RECT_F grid_cell = D2D1::RectF(margin_x + x * gridSize + 0.5, margin_y + py * gridSize + 0.5, 0, 0);
					grid_cell.right = grid_cell.left + gridSize - 1;
					grid_cell.bottom = grid_cell.top + gridSize - 1;
					m_pRenderTarget->FillRectangle(&grid_cell, m_pBrush);
				}
			}
	};

	auto drawTrashTrough = [&]() {
		m_pRenderTarget->SetTransform(view_offset);
		D2D1_RECT_F border_rect = D2D1::RectF(0 + margin_x + map.width * gridSize
			, 0 + margin_y + 2 * gridSize, 0, 0);
		auto h = 20;
		border_rect.right = border_rect.left + troughWidth;
		border_rect.bottom = border_rect.top + (h)*gridSize;
		m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		m_pRenderTarget->DrawRectangle(&border_rect, m_pBrush);
		if (auto pval = std::get_if<Modes::versus>(&object.gm)) {
			border_rect.top = border_rect.bottom - std::min(pval->trashLinesCount, 20) * gridSize;
			border_rect.left += .5;
			border_rect.right -= .5;
			m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
			m_pRenderTarget->FillRectangle(&border_rect, m_pBrush);
		}


		//m_pRenderTarget->FillRectangle(&border_rect, m_pBrush);
	};

	auto drawActive = [&]() {
		auto dropDis = tn.getDrop(map);
		m_pRenderTarget->SetTransform(view_offset);
		for (auto& point : tn.getPoints()) {
			m_pBrush->SetColor(getColors(tn.type));
			const auto y = (h - 1) - tn.y - point.y + offsetY;
			D2D1_RECT_F cur_cell = D2D1::RectF(
				margin_x + (tn.x + point.x) * gridSize + 0.5,
				margin_y + (y)*gridSize + 0.5, 0, 0);
			cur_cell.right = cur_cell.left + gridSize - 1;
			cur_cell.bottom = cur_cell.top + gridSize - 1;
			m_pRenderTarget->FillRectangle(&cur_cell, m_pBrush);
			if (dropDis > 0) {
				D2D1_RECT_F shadow_cell = D2D1::RectF(
					cur_cell.left + 0.5, cur_cell.top + dropDis * gridSize + 0.5,
					cur_cell.right - 1, cur_cell.bottom + dropDis * gridSize - 1);
				m_pRenderTarget->DrawRectangle(&shadow_cell, m_pBrush);
			}
		}
	};

	auto drawPreviews = [&]() {
		auto count = 0, offset = 0;
		for (const auto type : rS.displayBag) {
			auto points = TetrisNode::rotateDatas[static_cast<int>(type)][0];
			auto height = points.back().y - points.front().y;
			m_pRenderTarget->SetTransform(
				view_offset * D2D1::Matrix3x2F::Translation(map.width * gridSize + +troughWidth, 
					offset * gridSize + count * 20 + 60));
			m_pBrush->SetColor(getColors(type));
			for (auto& point : points) {
				const auto y = height - (point.y + (type == Piece::I ? -1 : 0));
				const auto x = (type != Piece::I && type != Piece::O ? 1 : 0);
				D2D1_RECT_F cur_cell = D2D1::RectF(
					margin_x + (point.x) * gridSize + gridSize * x / 2 + 0.5,
					margin_y + (y)*gridSize + 0.5, 0, 0);
				cur_cell.right = cur_cell.left + gridSize - 1;
				cur_cell.bottom = cur_cell.top + gridSize - 1;
				m_pRenderTarget->FillRectangle(&cur_cell, m_pBrush);
			}
			offset += height + 1;
			count++;
		}
	};

	auto drawHold = [&]() {
		if (hS.type) {
			auto count = 0, offset = 0;
			auto points = TetrisNode::rotateDatas[static_cast<int>(*hS.type)][0];
			auto height = points.back().y - points.front().y;
			m_pRenderTarget->SetTransform(
				view_offset * D2D1::Matrix3x2F::Translation((-3 - (*hS.type == Piece::I || *hS.type == Piece::O)) * gridSize, 0 + 60));
			m_pBrush->SetColor(getColors(hS.able ? *hS.type : Piece::Trash));
			for (auto& point : points) {
				const auto x = (*hS.type != Piece::I && *hS.type != Piece::O ? -1 : 0);
				const auto y = height - (point.y + (*hS.type == Piece::I ? -1 : 0));
				D2D1_RECT_F cur_cell = D2D1::RectF(
					margin_x + (point.x) * gridSize + gridSize * x / 2 + 0.5,
					margin_y + (y)*gridSize + 0.5, 0, 0);
				cur_cell.right = cur_cell.left + gridSize - 1;
				cur_cell.bottom = cur_cell.top + gridSize - 1;
				m_pRenderTarget->FillRectangle(&cur_cell, m_pBrush);
			}
		}
	};

	auto drawStatus = [&]() {
		m_pRenderTarget->SetTransform(view_offset);
		m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
		D2D1_RECT_F rect = D2D1::RectF(0 + margin_x, 0 + margin_y + 22 * gridSize, 0, 0);
		rect.right = rect.left + map.width * CELL_SIZE * 2;
		rect.bottom = rect.top + 20;
		std::wstring wstr(gd.clearTextInfo.begin(), gd.clearTextInfo.end());
		m_pRenderTarget->DrawText(
			wstr.c_str(), wstr.length(), m_pTextFormat, rect, m_pBrush
		);
	};

	auto draw_view = [&]() {
		m_pRenderTarget->SaveDrawingState(m_pStateBlock);
		drawField();
		drawTrashTrough();
		drawActive();
		drawPreviews();
		drawHold();
		drawStatus();
		m_pRenderTarget->RestoreDrawingState(m_pStateBlock);
	};
	draw_view();

}

void Engine::Logic_with(double elapsedTime, MyTetris& tetris)
{
	if (turn && turn_index != tetris.index) return;

	if (tetris.hK.gameOver) // Do nothing if game over
		return;

	if (tetris.hK.hao && tetris.hK.botReady == -1) {
		tetris.hK.botReady = 0;
		tetris.hK.hao = false;
	}

	if (tetris.hK.botReady != -1) {
		tetris.hK.botTimer += elapsedTime;
///#define USE_QUICK_MOVE
#ifdef USE_QUICK_MOVE
		for (; tetris.hK.botReady < tetris.hK.botOperSource.size(); tetris.hK.botReady)
			tetris.opers(tetris.hK.botOperSource[tetris.hK.botReady++]);
#endif

#ifndef USE_QUICK_MOVE
		if (tetris.hK.botReady == tetris.hK.botOperSource.size()
			|| tetris.hK.botTimer > (tetris.hK.botOperSource[tetris.hK.botReady] == Oper::SoftDrop ? 0.014 : 0.025)) {
			tetris.opers(tetris.hK.botOperSource[tetris.hK.botReady++]);
#endif
			if (tetris.hK.botReady == tetris.hK.botOperSource.size()) {

				if (turn_index == 0) turn_index = 1;
				else turn_index = 0;

				tetris.hK.botReady = -1;
				tetris.hK.botOperSource.clear();
				if (tetris.hK.botPressed == 0) {
					//botPressed = -1;
					tetris.hK.botPressed = 1;
				}
#ifndef USE_QUICK_MOVE
			}
#endif

			tetris.hK.botTimer = 0;
		}
	}

	if (active == &tetris) {
		if (tetris.hK.spacePressed == 1) {
			tetris.opers(Oper::HardDrop);
			if (turn_index == 0) turn_index = 1;
			else turn_index = 0;
			tetris.hK.spacePressed = 0;
		}
		if (tetris.hK.leftPressed != -1) { 	// Move left or right
			tetris.hK.leftTimer += elapsedTime;
			if (tetris.hK.leftPressed == 1) {
				tetris.opers(Oper::Left);
				tetris.hK.leftPressed = 0;
			}
			else if (tetris.hK.leftPressed == 0) {
				if (tetris.hK.leftTimer >= tetris.hK.das && !tetris.hK.autoMoveLeft) {
					tetris.hK.autoMoveLeft = 1;
					tetris.hK.leftTimer = 0;
				}
				else if (tetris.hK.autoMoveLeft) {
					if (tetris.hK.leftTimer >= tetris.hK.arr) {
						tetris.opers(Oper::Left);
						tetris.hK.leftTimer = 0;
					}
				}
			}
		}
		if (tetris.hK.rightPressed != -1) {
			tetris.hK.rightTimer += elapsedTime;
			if (tetris.hK.rightPressed == 1) {
				tetris.opers(Oper::Right);
				tetris.hK.rightPressed = 0;
			}
			else if (tetris.hK.rightPressed == 0) {
				if (tetris.hK.rightTimer >= tetris.hK.das && !tetris.hK.autoMoveRight) {
					tetris.hK.autoMoveRight = 1;
					tetris.hK.rightTimer = 0;
				}
				else if (tetris.hK.autoMoveRight) {
					if (tetris.hK.rightTimer >= tetris.hK.arr) {
						tetris.opers(Oper::Right);
						tetris.hK.rightTimer = 0;
					}
				}
			}
		}
		if (tetris.hK.downPressed) {
			tetris.opers(Oper::SoftDrop);
		}
		if (tetris.hK.cwPressed == 1) {
			tetris.opers(Oper::Cw);
			tetris.hK.cwPressed = 0;
		}
		if (tetris.hK.ccwPressed == 1) {
			tetris.opers(Oper::Ccw);
			tetris.hK.ccwPressed = 0;
		}
		if (tetris.hK.holdPressed == 1) {
			tetris.opers(Oper::Hold);
			tetris.hK.holdPressed = 0;
		}
	}
	if (tetris.hK.botPressed == 1) {
		constexpr int time = 166;// 85;
		tetris.hK.test = std::async(
			[&](auto time) {
				tetris.hK.botOperSource = std::move(tetris.callBot(time));
				tetris.hK.hao = true; }, time);
		tetris.hK.botPressed =  -1;
	}
	if (tetris.hK.restartPressed == 1) {
		tetris.restart();
		tetris.hK.restartPressed = 0;
	}
}

void Engine::Logic(double elapsedTime)
{
	for (auto & player:players)
		Logic_with(elapsedTime, player);
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

	for (auto i = 0; i < view_positions.size(); i++)
		draw_with(players[i], view_positions[i]);
	//tetris.Draw();

	//DrawTextAndScore();
	hr = m_pRenderTarget->EndDraw();
	return true;
}

void Engine::DrawTextAndScore()
{
	  // Text and score
       const char* score = "a";
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
	   );
}
