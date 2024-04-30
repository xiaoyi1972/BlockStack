#pragma once
#define BotStyle 1

#include "framework.h"
#include "graphics/renderer.hpp"
#include "utility/measure.hpp"
#include "graphics/image.hpp"
#include "gui/window_helper.hpp"
#include "utility/string_conversion.hpp"

#include "tetrisGame.h"
#include "tetrisCore.h"

#include <map>

constexpr auto RESOLUTION_X = 860.f;
constexpr auto RESOLUTION_Y = 600.f;
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
	std::atomic<bool> complete_calc = false;
	int botReady = -1;
	std::optional<std::future<void>> botcall_handle;
	std::vector<Oper> botOperSource;
	bool started = false, completed = false;
};

namespace chrome {
	class Engine;
}

class MyTetris : public TetrisGame {
	friend class chrome::Engine;
public:
	MyTetris() : index(-1) {}

	auto callBot(const int limitTime) {
		return caculateBot(tn, map, limitTime);
	}

	void caculateBot(const TetrisNode& start, const TetrisMap& field, const int limitTime) {
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.end());
		state = TetrisDelegator::GameState{
			.dySpawn = dySpawn,
			.upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0,
			.bag = dp,
			.cur = start,
			.field = &field,
			.canHold = hS.able,
			.hold = hS.type,
			.b2b = !!gd.b2b,
			.combo = gd.combo,
			.path_sd = true
		};
		bot.run(state, limitTime);
	}

	std::vector<Oper> playStep() {
		TetrisDelegator::OutInfo tag{};
		state.upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		auto path = bot.suggest(state, tag);
		if (!path || path->empty()) {
			path = std::vector<Oper>{ Oper::HardDrop };
		}
		return *path;
	}

	~MyTetris() {}

	Handle_keys hK;
	TetrisDelegator bot = TetrisDelegator::launch();
	TetrisDelegator::GameState state{};

private:
	int index;
};

namespace chrome {
	class Engine {

		friend class Application;

		Engine();
		void InitializeD2D();

		auto getColors(Piece type) -> D2D1_COLOR_F&;

		//Event
		void KeyUp(WPARAM wParam);
		void KeyDown(WPARAM wParam);
		void Command(WPARAM wParam);
		void MousePosition(int x, int y);
		void MouseButtonUp(bool left, bool right);
		void MouseButtonDown(bool left, bool right);

		void Logic_with(double elapsedTime, MyTetris& tetris);
		void Logic(double elapsedTime);

		template<class T> 
		void draw_with(T&, const D2D1::Matrix3x2F&);
		void begin_paint();
		void end_paint();

		template<class T>
		bool Draw(T* Application, double elapsedTime)
		{

			if (elapsedTime < interval) {
				return false;
			}

			Application->begin_paint();
			//_renderer->push_transform(D2D1::Matrix3x2F::Identity());;

			for (auto i = 0; i < view_positions.size(); i++)
				draw_with(players[i], view_positions[i]);

			Application->end_paint();
			return true;
		}


		auto paint_mock_sidebar(measure::rectangle<float> const& window_rectangle) -> void;
		auto handle_resize() -> void;

	private:


		std::map<Piece, D2D1_COLOR_F> m_colors;

		com::unique_ptr <IDWriteTextFormat> m_pTextFormat;
		com::unique_ptr <ID2D1SolidColorBrush> m_pBrush;
		com::unique_ptr <ID2D1DrawingStateBlock> m_pStateBlock;

		const unsigned int fps = 60 / 1;
		double interval = 1. / fps;
		MyTetris players[2];
		MyTetris* active;
		bool turn = true ^ 1;
		int turn_index = 0;
		std::array<D2D1::Matrix3x2F, 2> view_positions{
			D2D1::Matrix3x2F::Translation(-RESOLUTION_X / 4.f, 0),
			D2D1::Matrix3x2F::Translation(+RESOLUTION_X / 4.f, 0) 
		};

		std::unique_ptr<graphics::renderer> _renderer;
	};
}