#include <iostream>
#include <map>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "timer.hpp"
#include "tool.h"
#include "threadPool.h"

//#include<vld.h>

class MyTetris : public TetrisGame {
public:
	ThreadPool threadPool{ 1 };

	void Draw() {
		auto drawField = [&]() {
			std::string str;
			auto& points = gd.pieceChanges;
			auto it = points.begin();
			auto h = map.height >= 20 ? 19 : map.height - 1;
			for (int y = h; y >= 0; --y) {//draw field
				std::cout << std::setw(2) << y << " ";
				for (int x = 0; x < map.width; x++)
				{
					if (it != points.end() && it->y == y && it->x == x) {
						if (map(it->x, it->y))
							std::cout << "//";
						++it;
					}
					else {
						std::cout << (map(x, y) == 1 ? "[]" : "  ");
					}
				}
				std::cout << "\n";
			}
		};

		auto drawStatus = [&]() {
			std::cout << "\n";
			std::cout << tn.type << " ";
			for (auto& one : rS.displayBag)
				std::cout << one;
			std::cout << "[";
			if (hS.type) std::cout << *hS.type;
			std::cout << "]";
			std::cout << " found:" << nodeCounts << "\033[0K\n";
			std::cout << "b2b:" << (gd.b2b > 0) << " combo:" << gd.combo << " clear_lines:" << gd.clear << "\033[0K\n";
#ifdef SHOW_PATH
			std::cout << fromN << " -> " << toN << "\033[0K\n";
			std::cout << "[" << pathCost << "us] path:" << pathStr << "\033[0K\n";
#endif
			std::cout << gd.clearTextInfo << "\033[0K\n";
			if(!info.empty())
			std::cout << "info:" << info << "\033[0K\n";
		};

		drawField();
		drawStatus();
	}

	const TetrisNode& cur() {
		return tn;
	}

	void callBot(const int limitTime) {
		playPath(caculateBot(tn, limitTime));
	}

	std::vector<Oper> caculateBot(const TetrisNode& start, const int limitTime) {
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		//auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 0);
		auto upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		//#define USE_TEST_SINGLE
		//static
		TreeContext<TreeNode> ctx;
#ifdef USE_TEST_SINGLE
		TreeContext<TreeNode> ctx_single;
#endif
		auto init_context = [&](TreeContext<TreeNode>* context) {
			context->createRoot(start, map, dp, hS.type, hS.able, !!gd.b2b, gd.combo, dySpawn);
		};
		init_context(&ctx);
#ifdef USE_TEST_SINGLE
		init_context(&ctx_single);
#endif
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		auto end = high_resolution_clock::now() + milliseconds(limitTime);
		auto taskrun = [&](TreeContext<TreeNode>* context) {
			do {
				context->run();
			} while (high_resolution_clock::now() < end);
		};

		std::vector<std::future<void>> futures;
		auto thm = threadPool.get_idl_num();
		for (auto i = 0; i < thm - 1; i++)
			futures.emplace_back(threadPool.commit(taskrun, &ctx));
		taskrun(&ctx);
#ifdef USE_TEST_SINGLE
		taskrun(&ctx_single);
#endif
		for (auto& run : futures)
			run.wait();

		auto [best, ishold] = ctx.getBest(upAtt);

		std::vector<Oper>path;
		Timer t;
		if (best == nullptr)
			goto end;
		if (!ishold) {
			fromN = start, toN = *best;
			t.reset();
			path = Search::make_path(start, *best, map, false);
			pathCost = t.elapsed<Timer::us>();
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map, dySpawn);
			fromN = holdNode, toN = *best;
			t.reset();
			path = Search::make_path(holdNode, *best, map, false);
			pathCost = t.elapsed<Timer::us>();
			path.insert(path.begin(), Oper::Hold);
		}
	end:
		if (path.empty())
			path.push_back(Oper::HardDrop);
		pathStr = std::to_string(path);
		nodeCounts = ctx.count;
		info = ctx.info;
		return path;
#undef USE_TEST_SINGLE
	}

	void playPath(const std::vector<Oper>& path) {
		for (auto& i : path) {
			opers(i);
		}
	}

	void setOpponent(TetrisGame* player) {
		opponent = player;
	}

	std::string pathStr, info;
	int nodeCounts{}, pathCost{};
	TetrisNode fromN, toN;
};

void bench();

#define TestSpeed 0
#define Play 1

int main()
{

#if TestSpeed
	//bench();
	//return 0;
	TetrisMap map(10, 40);
	map[19] = 1000000011_r;
	map[18] = 1100001111_r;
	map[17] = 1111111011_r;
	map[16] = 1111111110_r;
	map[15] = 1111111110_r;
	map[14] = 1011111111_r;

	map.update();
	auto piece = TetrisNode::spawn(Piece::L, nullptr, 19);
	std::cout << piece.mapping(map) << "\n";
	auto nodes = Search::search<0, trivail_vector<TetrisNode>>(piece, map);
	std::sort(nodes.begin(), nodes.end());
	std::cout << "\n";
	for (auto& landpoint : nodes) {
		landpoint.getTSpinType();
		std::cout << landpoint.mapping(map) << landpoint << " step:" << landpoint.step;
		std::cout << " above_stack:" << (landpoint.open(map) ? "true" : "false");
		if (landpoint.type == Piece::T)
			std::cout << " TSpinType:"
			<< landpoint.typeTSpin
			<< " lastRotate:"
			<< (landpoint.lastRotate ? "true" : "false");
		std::cout << "\n\n";
	}

#else
#if Play
	MyTetris tetris;
	//tetris.setOpponent(&tetris);
	size_t speed{ 300 };
	std::cout << "input time for calculating:";
	std::cin >> speed;
	std::cout << "\033[2J\033[1;1H"; //\033[2J
	for (int i = 0; ; i++) {
		tetris.callBot(speed);
		tetris.Draw();
		std::cout << "\033[1;1H";
	}
#else
	auto test_func = [&](const TetrisMap& map) {
		auto min_roof = *std::min_element(map.top, map.top + map.width);
		int row_trans = 0, col_trans = 0;
#define marco_side_fill(v) (v | (1 << map.width - 1))
		std::cout << "roof:" << map.roof << "\n";
		for (int y = map.roof - 1; y >= 0; y--) {
			row_trans += BitCount(marco_side_fill(map[y]) ^ marco_side_fill(map[y] >> 1));
			col_trans += BitCount(map[y] ^ map[bitwise::min(y + 1, map.height - 1)]);
		}
#undef marco_side_fill
		std::cout << "map\n" << map << "\n";
		std::cout << "row_trans:" << row_trans << " col_trans:" << col_trans << "\n";
	};

	auto test_func1 = [&](const TetrisMap& map) {
		auto wells = 0;
		for (auto x = 0; x < map.width; x++)
			for (auto y = map.roof - 1, fit = 0; y >= 0; y--, fit = 0) {
				if (x == 0 && !map(x, y) && map(x + 1, y) && ++fit)
					wells++;
				else if (x == map.width - 1 && map(x - 1, y) && !map(x, y) && ++fit)
					wells++;
				else if (map(x - 1, y) && !map(x, y) && map(x + 1, y) && ++fit)
					wells++;
				if (fit) {
					for (auto y1 = y - 1; y1 >= 0; y1--) {
						if (!map(x, y1))
							wells++;
						else
							break;
					}
				}
			}
		std::cout << "map\n" << map << "\n";
		std::cout << "wells:" << wells << "\n";
	 };
	TetrisMap map(10, 20);
	/*std::array arr{
		0000000001_r, 
		0000000010_r, 
		0000000011_r,
		1000000000_r,
		0100000000_r,
		1100000000_r
	};*/
	//for (auto b : arr) {
     	map[2] = 0010100000_r;
		map[1] = 0010100000_r;
		map[0] = 0010100000_r;
		map.update();
		test_func1(map);
	//}
#endif
#endif
}

void bench() {
	std::map <std::string_view, TetrisMap> maps{
		{"tspin",
			TetrisMap(10, 40, {
			{7,0000000001_r},
			{6,0000000001_r},
			{5,1100000011_r},
			{4,1110000111_r},
			{3,1111001111_r},
			{2,1111001111_r},
			{1,1111000111_r},
			{0,1111101111_r}, })
		},
		{"dtd",
			TetrisMap(10, 40, {
			{8,1100000000_r},
			{7,1111001111_r},
			{6,1111000111_r},
			{5,1111110111_r},
			{4,1111100111_r},
			{3,1111100011_r},
			{2,1111110111_r},
			{1,1111110111_r},
			{0,1111101111_r}, })
		},
		{"terrible",
			TetrisMap(10, 40, {
			{11,0011111111_r},
			{10,0011111111_r},
			{9,0000000001_r},
			{8,0000000001_r},
			{7,1111111001_r},
			{6,1111111001_r},
			{5,1000000001_r},
			{4,1000000001_r},
			{3,1001111111_r},
			{2,1001111111_r},
			{1,1000000000_r},
			{0,1000000000_r}, })
		},
		{"empty",
			TetrisMap(10, 40)
		}
	};

	std::array pieces{
		TetrisNode::spawn(Piece::T, nullptr, 19),
		//TetrisNode::spawn(Piece::O, nullptr, 19),
		//TetrisNode::spawn(Piece::Z, nullptr, 19),
		//TetrisNode::spawn(Piece::S, nullptr, 19),
		//TetrisNode::spawn(Piece::J, nullptr, 19),
		//TetrisNode::spawn(Piece::L, nullptr, 19),
		//TetrisNode::spawn(Piece::I, nullptr, 19),
	};


	Timer t;
	//#define ONCE
#ifndef ONCE
	std::size_t times{ 100000 }, costs{ 0 }, i = 0;
#else
	std::size_t times{ 1 }, costs{ 0 }, i = 0;
#endif
	bool is{};

#define OUTPUT 0
#define INFO
	for (auto& [tag, map] : maps) {
#ifdef INFO
		if (i++ != 0) std::cout << "\n";
		std::cout << map << "\n";
#endif
		for (auto& node : pieces) {
			costs = 0;
			for (int i = 0; i < times; i++) {
				t.reset();
				auto nodes = Search::search<0, trivail_vector<TetrisNode>>(node, map);
				auto cost = t.elapsed<Timer::us>();
				costs += cost;
#if OUTPUT
				std::sort(nodes.begin(), nodes.end());
				std::cout << "\n";
				for (auto& landpoint : nodes) {
					landpoint.getTSpinType();
					std::cout << landpoint.mapping(map) << landpoint << " step:" << landpoint.step;
					std::cout << " above_stack:" << (landpoint.open(map) ? "true" : "false");
					if (landpoint.type == Piece::T)
						std::cout << " TSpinType:"
						<< landpoint.typeTSpin
						<< " lastRotate:"
						<< (landpoint.lastRotate ? "true" : "false");
					std::cout << "\n\n";
				}
#endif
			}
#ifdef INFO
			std::cout << "\n" << node.type
				<< "\niterations " << times << " \navg " << double(costs) / times << "us" << "\n";
#endif
		}
	}
}
