#pragma once
#define BotStyle 1
#include "resource.h"
#include "tetrisGame.h"
#include "threadPool.h"
#include <map>

#if BotStyle 
#include "tetrisCore.h"
#else
#include "testNew.h"
#endif

constexpr auto RESOLUTION_X = 800 * 0.7 * 1.45;
constexpr auto RESOLUTION_Y = 600 * 0.8;
constexpr auto CELL_SIZE = 20;
constexpr auto VIEW_WIDTH = CELL_SIZE * (4 + 4 + 20);

struct Handle_keys {
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
};

class MyTetris : public TetrisGame {
	friend class Engine;
	int index;
public:

	MyTetris() : index(-1) {}

	auto callBot(const int limitTime) {
		return caculateBot(tn, map, limitTime);
	}

	std::vector<Oper> caculateBot(TetrisNode start, TetrisMap field, const int limitTime) {
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		//static 
		TreeContext<TreeNode> ctx;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		auto upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		ctx.createRoot(start, field, dp, hS.type, hS.able, !!gd.b2b, gd.combo, dySpawn);
		//#define USE_ITERATION
#ifdef USE_ITERATION
		auto times = 10000;
		for (auto i = 0; i < times; i++) {
			ctx.run();
		}
#else
		auto end = high_resolution_clock::now() + milliseconds(limitTime);
		auto taskrun = [&]() {
			do {
				ctx.run();
			} while (high_resolution_clock::now() < end);
		};
		std::vector<std::future<void>> futures;
		auto thm = pool.get_idl_num();
		for (auto i = 0; i < thm; i++)
			futures.emplace_back(pool.commit(taskrun));
		for (auto& run : futures)
			run.wait();
#endif
#undef USE_ITERATION
		auto [best, ishold] = ctx.getBest(upAtt);
		std::vector<Oper>path;
		if (best == nullptr)
			goto end;
		if (!ishold) {
			path = Search::make_path(start, *best, field, true);
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map, dySpawn);
			path = Search::make_path(holdNode, *best, field, true);
			path.insert(path.begin(), Oper::Hold);
		}
		end:
		if (path.empty()) path.push_back(Oper::HardDrop);
		return path;
	}

	~MyTetris() {}

	Handle_keys hK;
	ThreadPool pool{ 1 };
};

class Engine
{
public:
	friend class MyTetris;
	Engine();
	~Engine();

	HRESULT InitializeD2D(HWND m_hwnd);
	void KeyUp(WPARAM wParam);
	void KeyDown(WPARAM wParam);
	void Command(WPARAM wParam);
	void MousePosition(int x, int y);
	void MouseButtonUp(bool left, bool right);
	void MouseButtonDown(bool left, bool right);
	void Logic(double elapsedTime);
	void Logic_with(double , MyTetris& );
	bool Draw(double elapsedTime);

	template<class T> void draw_with(T&, const D2D1::Matrix3x2F&);

private:
	const unsigned int fps = 60;
	double interval = 1. / fps;
	ID2D1Factory* m_pDirect2dFactory;
	ID2D1HwndRenderTarget* m_pRenderTarget;
	IDWriteFactory* m_pDWriteFactory;
	IDWriteTextFormat* m_pTextFormat;
	ID2D1SolidColorBrush* m_pWhiteBrush;

	std::map<Piece, D2D1_COLOR_F> m_colors;
	ID2D1SolidColorBrush* m_pBrush;
	ID2D1DrawingStateBlock* m_pStateBlock;

	std::array<D2D1::Matrix3x2F, 2> view_positions{ 
		D2D1::Matrix3x2F::Translation(-RESOLUTION_X / 4, 0),
		D2D1::Matrix3x2F::Translation(+RESOLUTION_X / 4, 0) };

	void InitializeTextAndScore();
	void DrawTextAndScore();

	auto& getColors(Piece type) {
		return m_colors[type];
	}

	MyTetris players[2];
	MyTetris* active;
	bool turn =  1^true;
	int turn_index = 0;
};

