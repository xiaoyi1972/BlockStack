#include "engine.h"

//#define USE_QUICK_MOVE

namespace chrome {

	Engine::Engine() {

		players[0].bot.set_thread_num(1);
		players[1].bot.set_thread_num(2);

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

	void Engine::InitializeD2D() {
		auto [factory, resource, render_target] = _renderer->render_dc();

		ID2D1Factory* factory1 = nullptr;



		ID2D1SolidColorBrush* temporary_pBrush;
		ID2D1DrawingStateBlock* temporary_pStateBlock;
		resource->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &temporary_pBrush);
		m_pBrush = com::make_unique(temporary_pBrush);

		resource->GetFactory(&factory1);
		com::make_unique(factory1);

		factory1->CreateDrawingStateBlock(&temporary_pStateBlock);

		m_pStateBlock = com::make_unique(temporary_pStateBlock);
	}

	auto Engine::getColors(Piece type)  -> D2D1_COLOR_F& {
		return m_colors[type];
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


	void Engine::Logic_with(double elapsedTime, MyTetris& tetris)
	{
		if (tetris.hK.restartPressed == 1 && tetris.hK.complete_calc) {
			tetris.restart();
			tetris.hK.restartPressed = 0;
		}

		tetris.hK.gameOver = tetris.gd.deaded;

		if (turn && turn_index != tetris.index)
			return;

		if (tetris.hK.gameOver) { // Do nothing if game over
#define With(active) active->hK.restartPressed = 1;
			for (auto& player : players)
				With((&player));
#undef With
			return;
		}

		if (tetris.hK.complete_calc) {
			tetris.hK.botOperSource = tetris.playStep();
			tetris.hK.complete_calc = false;
			tetris.hK.botReady = 0;
		}
		if (tetris.hK.botReady != -1) {
			tetris.hK.botTimer += elapsedTime;
#ifdef USE_QUICK_MOVE
			for (; tetris.hK.botReady < tetris.hK.botOperSource.size(); tetris.hK.botReady)
				tetris.opers(tetris.hK.botOperSource[tetris.hK.botReady++]);
#endif
			if (tetris.hK.botReady == tetris.hK.botOperSource.size()) {
				turn_index = turn_index == 0 ? 1 : 0;
				tetris.hK.botReady = -1;
				tetris.hK.botOperSource.clear();
				if (tetris.hK.botPressed == 0) {
					tetris.hK.botPressed = 1;
				}
			}
#ifndef USE_QUICK_MOVE
			else if (tetris.hK.botTimer > (tetris.hK.botOperSource[tetris.hK.botReady] == Oper::SoftDrop ? 0.014 : 0.025)) {
				tetris.opers(tetris.hK.botOperSource[tetris.hK.botReady++]);
				tetris.hK.botTimer = 0;
			}
#endif
		}
		if (active == &tetris) {
			if (tetris.hK.spacePressed == 1) {
				tetris.opers(Oper::HardDrop);
				if (turn_index == 0)
					turn_index = 1;
				else
					turn_index = 0;
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
			constexpr int time = 100;
			auto call_bot = [&](auto time) {
				tetris.callBot(time);
				tetris.hK.complete_calc = true;
				};
			tetris.hK.botcall_handle = std::async(std::launch::async, call_bot, time);
			//call_bot(time);
			tetris.hK.botPressed = 0;
		}
	}

	void Engine::Logic(double elapsedTime)
	{
		for (auto& player : players)
			Logic_with(elapsedTime, player);
	}

	template<>
	void Engine::draw_with<MyTetris>(MyTetris& object, const D2D1::Matrix3x2F& view_offset) {
		auto sets = _renderer->render_dc();
		auto render_factory = std::get<0>(sets);
		auto render_source = std::get<1>(sets);
		auto render_target = std::get<2>(sets);

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

			_renderer->push_transform(view_offset);

			render_target->DrawRectangle(&border_rect, m_pBrush.get());
			for (int y = 0; y < h; y++) //draw field
				for (int x = 0; x < map.width; x++) {
					if (map(x, y)) {
						m_pBrush->SetColor(getColors(map.colorDatas[y][x]));
						auto py = (h - 1 - y + offsetY);
						D2D1_RECT_F grid_cell = D2D1::RectF(margin_x + x * gridSize + 0.5, margin_y + py * gridSize + 0.5, 0, 0);
						grid_cell.right = grid_cell.left + gridSize - 1;
						grid_cell.bottom = grid_cell.top + gridSize - 1;
						render_target->FillRectangle(&grid_cell, m_pBrush.get());
					}
				}
			_renderer->pop_transform();
			};

		auto drawTrashTrough = [&]() {
			_renderer->push_transform(view_offset);

			D2D1_RECT_F border_rect = D2D1::RectF(0 + margin_x + map.width * gridSize
				, 0 + margin_y + 2 * gridSize, 0, 0);
			auto h = 20;
			border_rect.right = border_rect.left + troughWidth;
			border_rect.bottom = border_rect.top + (h)*gridSize;
			m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
			render_target->DrawRectangle(&border_rect, m_pBrush.get());
			if (auto pval = std::get_if<Modes::versus>(&object.gm)) {
				border_rect.top = border_rect.bottom - std::min(pval->trashLinesCount, 20) * gridSize;
				border_rect.left += .5;
				border_rect.right -= .5;
				m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
				render_target->FillRectangle(&border_rect, m_pBrush.get());
			}


			_renderer->pop_transform();
			};

		auto drawActive = [&]() {
			auto dropDis = tn.getDrop(map);

			_renderer->push_transform(view_offset);

			for (auto& point : tn.data().points) {
				m_pBrush->SetColor(getColors(tn.type));
				const auto y = (h - 1) - tn.y - point.y + offsetY;
				D2D1_RECT_F cur_cell = D2D1::RectF(
					margin_x + (tn.x + point.x) * gridSize + 0.5,
					margin_y + (y)*gridSize + 0.5, 0, 0);
				cur_cell.right = cur_cell.left + gridSize - 1;
				cur_cell.bottom = cur_cell.top + gridSize - 1;
				render_target->FillRectangle(&cur_cell, m_pBrush.get());
				if (dropDis > 0) {
					D2D1_RECT_F shadow_cell = D2D1::RectF(
						cur_cell.left + 0.5, cur_cell.top + dropDis * gridSize + 0.5,
						cur_cell.right - 1, cur_cell.bottom + dropDis * gridSize - 1);
					render_target->DrawRectangle(&shadow_cell, m_pBrush.get());
				}
			}

			_renderer->pop_transform();
			};

		auto drawPreviews = [&]() {
			auto count = 0, offset = 0;

			_renderer->push_transform(view_offset);

			for (const auto type : rS.displayBag) {
				auto points = TetrisNode::rotateDatas[static_cast<int>(type)][0].points;
				auto height = points.back().y - points.front().y;

				_renderer->push_transform(D2D1::Matrix3x2F::Translation(map.width * gridSize + troughWidth,
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
					render_target->FillRectangle(&cur_cell, m_pBrush.get());
				}

				_renderer->pop_transform();

				offset += height + 1;
				count++;
			}

			_renderer->pop_transform();
			};

		auto drawHold = [&]() {
			if (hS.type != Piece::None) {
				_renderer->push_transform(view_offset);

				auto count = 0, offset = 0;
				auto points = TetrisNode::rotateDatas[static_cast<int>(hS.type)][0].points;
				auto height = points.back().y - points.front().y;

				_renderer->push_transform(D2D1::Matrix3x2F::Translation(
					(-3 - (hS.type == Piece::I || hS.type == Piece::O)) * gridSize, 0 + 60));

				m_pBrush->SetColor(getColors(hS.able ? hS.type : Piece::Trash));
				for (auto& point : points) {
					const auto x = (hS.type != Piece::I && hS.type != Piece::O ? -1 : 0);
					const auto y = height - (point.y + (hS.type == Piece::I ? -1 : 0));
					D2D1_RECT_F cur_cell = D2D1::RectF(
						margin_x + (point.x) * gridSize + gridSize * x / 2 + 0.5,
						margin_y + (y)*gridSize + 0.5, 0, 0);
					cur_cell.right = cur_cell.left + gridSize - 1;
					cur_cell.bottom = cur_cell.top + gridSize - 1;
					render_target->FillRectangle(&cur_cell, m_pBrush.get());
				}

				_renderer->pop_transform();
				_renderer->pop_transform();

			}
			};

		auto drawStatus = [&]() {

			_renderer->push_transform(view_offset);

			m_pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
			D2D1_RECT_F rect = D2D1::RectF(0 + margin_x, 0 + margin_y + 22 * gridSize, 0, 0);
			rect.right = rect.left + map.width * CELL_SIZE * 2;
			rect.bottom = rect.top + 20;
			std::string wstr(gd.clearTextInfo.begin(), gd.clearTextInfo.end());

			_renderer->draw_text(
				wstr, measure::point<float>{ rect.left, rect.top },
				"Times New Roman", 16.0f, measure::color{ 0.f, 0.f, 0.f, 1.0f }
			);

			auto rect1 = rect;
			rect1.top += 20;
			rect1.bottom += 20;
			_renderer->draw_text(
				"clear:" + std::to_string(gd.clear), measure::point<float>{ rect1.left, rect1.top },
				"Times New Roman", 16.0f, measure::color{ 0.f, 0.f, 0.f, 1.0f }
			);

			/*render_target->DrawText(
				wstr.c_str(), wstr.length(), m_pTextFormat.get(), rect, m_pBrush.get()
			);*/

			_renderer->pop_transform();
			};

		auto draw_view = [&]() {
			//_renderer->fill_rectangle(measure::rectangle{0.f, 0.f, 40.f, 70.f}, measure::color(1.f, 0.f, 0.f));;

			drawField();
			drawActive();
			drawTrashTrough();;
			drawPreviews();
			drawHold();
			drawStatus();

			};
		draw_view();

	}


	auto Engine::paint_mock_sidebar(measure::rectangle<float> const& window_rectangle) -> void {

		_renderer->fill_rectangle(
			measure::rectangle<float> {
			window_rectangle.origin.x, 55.0f,
				window_rectangle.origin.x + 400.0f,
				window_rectangle.dimension.height
		}, measure::color{ 0.92f, 0.92f, 0.92f }
			);

	}

	auto Engine::handle_resize() -> void {
		if (_renderer) _renderer->resize_buffers();
	}
}