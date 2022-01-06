#pragma once
#define BotStyle 1
#include "resource.h"
#include "tetrisGame.h"
#include "ThreadPool.h"
#include <map>

#if BotStyle 
#include "tetrisCore.h"
#else
#include "testNew.h"
#endif

constexpr auto RESOLUTION_X = 800 * 0.7;
constexpr auto RESOLUTION_Y = 600 * 0.8;
constexpr auto CELL_SIZE = 20;

template<class Engine>
class MyTetris : public TetrisGame {

public:
	Engine* engine = nullptr;

	MyTetris(Engine* _engine) : engine(_engine) { 
		{
			m_colors.insert({ Piece::Trash,D2D1::ColorF(115. / 255, 115. / 255, 115. / 255) });
			m_colors.insert({ Piece::None,D2D1::ColorF(255. / 255, 255. / 255, 255.255, 0) });
			m_colors.insert({ Piece::O,D2D1::ColorF(245. / 255, 220. / 255, 0. / 255) });
			m_colors.insert({ Piece::I,D2D1::ColorF(57. / 255, 195. / 255, 199. / 255) });
			m_colors.insert({ Piece::T,D2D1::ColorF(138. / 255, 43. / 255, 227. / 255) });
			m_colors.insert({ Piece::L,D2D1::ColorF(255. / 255, 166. / 255, 0. / 255) });
			m_colors.insert({ Piece::J,D2D1::ColorF(0. / 255, 0. / 255, 255. / 255) });
			m_colors.insert({ Piece::S,D2D1::ColorF(51. / 255, 204. / 255, 51. / 255) });
			m_colors.insert({ Piece::Z,D2D1::ColorF(255. / 255, 0. / 255, 0. / 255) });
		}
	}

	auto callBot(const int limitTime) {
		return caculateBot(tn, map, limitTime);
	}

#if BotStyle
	std::vector<Oper> caculateBot(TetrisNode start, TetrisMap field, const int limitTime) {
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		static 
		TreeContext ctx;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		ctx.createRoot(start, field, dp, hS.type, !!gd.b2b, gd.combo, std::holds_alternative<Modes::versus>(gm) ?
			std::get<Modes::versus>(gm).trashLinesCount : 0, dySpawn);
		auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
		do {
			ctx.run();
		} while ((now = high_resolution_clock::now()) < end);
		auto [best, ishold] = ctx.getBest();
		std::vector<Oper>path;
		if (!ishold) {
			path = Search::make_path(start, best, field, true);
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map, dySpawn);
			path = Search::make_path(holdNode, best, field, true);
			path.insert(path.begin(), Oper::Hold);
		}
		if (path.empty()) path.push_back(Oper::HardDrop);
		return path;
	}
#else
	std::vector<Oper> caculateBot(TetrisNode start, TetrisMap field, const int limitTime) {
		using namespace CC_LIKE;
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		static 
			TreeContext<TreeNode> ctx;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		ctx.createRoot(start, field, dp, hS.type, !!gd.b2b, gd.combo, std::holds_alternative<Modes::versus>(gm) ?
			std::get<Modes::versus>(gm).trashLinesCount : 0, dySpawn);
		auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
		do {
			ctx.run();
		} while ((now = high_resolution_clock::now()) < end);
		auto [best, ishold] = ctx.getBest();
		std::vector<Oper>path;
		if (!ishold) {
			path = Search::make_path(start, best, field, true);
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map, dySpawn);
			path = Search::make_path(holdNode, best, field, true);
			path.insert(path.begin(), Oper::Hold);
		}
		if (path.empty()) path.push_back(Oper::HardDrop);
		return path;
	}
#endif

	auto &getColors(Piece type) {
		return m_colors[type];
	}

	void InitializeD2D(ID2D1HwndRenderTarget* m_pRenderTarget)
	{
		// Creates a red brush for drawing
		m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &m_pBrush);
		engine->m_pDirect2dFactory->CreateDrawingStateBlock(&m_pStateBlock);
	}

	void Draw() {
		auto& m_pRenderTarget = engine->m_pRenderTarget;
	    auto& m_pDirect2dFactory = engine->m_pDirect2dFactory;

		const int gridSize = CELL_SIZE;
		constexpr int offsetY = 2;
		auto h = map.height >= 20 ? 20 : map.height;
		int margin_x = (RESOLUTION_X - map.width * gridSize) / 2;
		int margin_y = (RESOLUTION_Y - (h + 2) * gridSize) / 2;

		auto drawField = [&]() {
			D2D1_RECT_F border_rect = D2D1::RectF(0 + margin_x, 0 + margin_y + 2 * gridSize, 0, 0);
			border_rect.right = border_rect.left + map.width * gridSize;
			border_rect.bottom = border_rect.top + (h) * gridSize;
			m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
			m_pRenderTarget->DrawRectangle(&border_rect, m_pBrush);
			for (int y = 0; y < h; y++) //draw field
				for (int x = 0; x < map.width; x++)
				{
					if (map(x, y))
					{
						m_pBrush->SetColor(getColors(map.colorDatas[y][x]));
						auto py = (h - 1 - y + offsetY);
						D2D1_RECT_F grid_cell = D2D1::RectF(margin_x + x * gridSize + 0.5, margin_y + py * gridSize + 0.5, 0, 0);
						grid_cell.right = grid_cell.left + gridSize - 1;
						grid_cell.bottom = grid_cell.top + gridSize - 1;
						m_pRenderTarget->FillRectangle(&grid_cell, m_pBrush);
					}
				}
		};

		auto drawActive = [&]() {
			auto dropDis = tn.getDrop(map);
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
			m_pRenderTarget->SaveDrawingState(m_pStateBlock);
			for (const auto type : rS.displayBag) {
				auto points = TetrisNode::rotateDatas[static_cast<int>(type)][0];
				auto height = points.back().y - points.front().y;
				m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Translation(map.width * gridSize, offset * gridSize + count * 20 + 60));
				m_pBrush->SetColor(getColors(type));
				for (auto& point : points) {
					const auto y = height - (point.y + (type == Piece::I ? -1 : 0));
					D2D1_RECT_F cur_cell = D2D1::RectF(
						margin_x + (point.x) * gridSize + 0.5,
						margin_y + (y)*gridSize + 0.5, 0, 0);
					cur_cell.right = cur_cell.left + gridSize - 1;
					cur_cell.bottom = cur_cell.top + gridSize - 1;
					m_pRenderTarget->FillRectangle(&cur_cell, m_pBrush);
				}
				offset += height + 1;
				count++;
			}
			m_pRenderTarget->RestoreDrawingState(m_pStateBlock);
		};

		auto drawHold = [&]() {
			if (hS.type) {
				m_pRenderTarget->SaveDrawingState(m_pStateBlock);
				auto count = 0, offset = 0;
				auto points = TetrisNode::rotateDatas[static_cast<int>(*hS.type)][0];
				auto height = points.back().y - points.front().y;
				m_pRenderTarget->SetTransform(
					D2D1::Matrix3x2F::Translation((-3 - (*hS.type == Piece::I || *hS.type == Piece::O)) * gridSize, 0 + 60));
				m_pBrush->SetColor(getColors(hS.able ? *hS.type : Piece::Trash));
				for (auto& point : points) {
					const auto y = height - (point.y + (*hS.type == Piece::I ? -1 : 0));
					D2D1_RECT_F cur_cell = D2D1::RectF(
						margin_x + (point.x) * gridSize + 0.5,
						margin_y + (y)*gridSize + 0.5, 0, 0);
					cur_cell.right = cur_cell.left + gridSize - 1;
					cur_cell.bottom = cur_cell.top + gridSize - 1;
					m_pRenderTarget->FillRectangle(&cur_cell, m_pBrush);
				}
				m_pRenderTarget->RestoreDrawingState(m_pStateBlock);
			}
		};
		drawField();
		drawActive();
		drawPreviews();
		drawHold();
	}

	~MyTetris() {
		SafeRelease(&m_pStateBlock);
		SafeRelease(&m_pBrush);
	}

	std::map<Piece, D2D1_COLOR_F> m_colors;
	ID2D1SolidColorBrush* m_pBrush;
	ID2D1DrawingStateBlock* m_pStateBlock;
};

class Engine
{
public:
	friend class MyTetris<Engine>;
	Engine();
	~Engine();

	HRESULT InitializeD2D(HWND m_hwnd);
	void KeyUp(WPARAM wParam);
	void KeyDown(WPARAM wParam);
	void MousePosition(int x, int y);
	void MouseButtonUp(bool left, bool right);
	void MouseButtonDown(bool left, bool right);
	void Logic(double elapsedTime);
	bool Draw(double elapsedTime);

private:
	const unsigned int fps = 60;
	double interval = 1. / fps;
	ID2D1Factory* m_pDirect2dFactory;
	ID2D1HwndRenderTarget* m_pRenderTarget;
	IDWriteFactory* m_pDWriteFactory;
	IDWriteTextFormat* m_pTextFormat;
	ID2D1SolidColorBrush* m_pWhiteBrush;

	void InitializeTextAndScore();
	void DrawTextAndScore();

	MyTetris<Engine> tetris{ this };

	bool gameOver = false;

	double das = .085, arr = .005, botDelay = 0.015;
	double leftTimer = 0, rightTimer = 0, botTimer = 0;
	int  leftPressed = -1, autoMoveLeft = 0, switchLeft = 0;
	int  rightPressed = -1, autoMoveRight = 0, switchRight = 0;
	bool downPressed = false;
	int spacePressed = -1;
	int cwPressed = -1, ccwPressed = -1, holdPressed = -1, botPressed = -1, restartPressed = -1;

	bool hao = false;
	int botHandle = -1, botReady = -1;
	std::future<void> test;
	std::vector<Oper> botOperSource;
	bool started = false, completed = false;
	ThreadPool pool{ 1 };
};

