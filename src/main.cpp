#include <iostream>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "tImer.hpp"
#include <map>
#include "tool.h"
#include "ThreadPool.h"
//#include<vld.h>
//#include"test.h"


class MyTetris : public TetrisGame {
public:
	ThreadPool threadPool{ 2 };

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
			//std::cout << "\nmap.roof:" << map.roof << "\033[0K\n";
			std::cout << "\n";
			std::cout << tn.type << " ";
			for (auto& one : rS.displayBag)
				std::cout << one;
			std::cout << "[";
			if (hS.type) std::cout << *hS.type;
			std::cout << "]";
			std::cout << " found:" << nodeCounts << "\033[0K\n";
			std::cout << "b2b:" << (gd.b2b > 0) << " combo:" << gd.combo << "\033[0K\n";
			std::cout << fromN << " -> " << toN << "\033[0K\n";
			std::cout << "[" << pathCost << "us] path:" << pathStr << "\033[0K\n";
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
		static 
		TreeContext<TreeNode> ctx;
		auto a = start;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		auto mapRef = map;
		ctx.createRoot(a, mapRef, dp, hS.type, hS.able, !!gd.b2b, gd.combo, std::holds_alternative<Modes::versus>(gm) ?
			std::get<Modes::versus>(gm).trashLinesCount : 0, dySpawn);

		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
		auto taskrun = [&]() {
			do {
				ctx.run();
			} while ((now = high_resolution_clock::now()) < end);
		};
		//for (int i = 0; i < threadPool.thread_num(); i++)
		//threadPool.enqueue(taskrun);

		//do {
			//ctx.run();
			//threadPool.enqueue([]() {ctx.run(); });
		//} while ((now = high_resolution_clock::now()) < end);

		std::vector<std::future<void>> futures;
		auto thm = threadPool.get_idl_num();
		for (auto i = 0; i < thm; i++)
			futures.emplace_back(threadPool.commit(taskrun));
		for (auto& run : futures)
		run.wait();
		auto [best, ishold] = ctx.getBest();
		//  std::cout << best.mapping(map);
		std::vector<Oper>path;
		Timer t;
		if (!ishold) {
			fromN = a, toN = best;
			t.reset();
			path = Search::make_path(a, best, mapRef, false);
			pathCost = t.elapsed<Timer::us>();
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map, dySpawn);
			fromN = holdNode, toN = best;
			t.reset();
			path = Search::make_path(holdNode, best, mapRef, false);
			pathCost = t.elapsed<Timer::us>();
			path.insert(path.begin(), Oper::Hold);
		}
		if (path.empty()) path.push_back(Oper::HardDrop);
		pathStr = std::to_string(path);
		nodeCounts = ctx.count;
		info = ctx.info;
		return path;
	}

	void playPath(const std::vector<Oper>& path) {
		for (auto& i : path) {
			opers(i);
		}
	}

	std::string pathStr, info;
	int nodeCounts{}, pathCost{};
	TetrisNode fromN, toN;
};


#define TestSpeed 0
#define Play 1

void bench() {
	std::map < std::string_view, TetrisMap> maps{
		{"tspin",
			TetrisMap(10, 40, {
			{7,0b1000000000},
			{6,0b1000000000},
			{5,0b1100000011},
			{4,0b1110000111},
			{3,0b1111001111},
			{2,0b1111001111},
			{1,0b1110001111},
			{0,0b1111011111}, })
		},
		{"dtd",
			TetrisMap(10, 40, {
			{8,0b0000000011},
			{7,0b1111001111},
			{6,0b1110001111},
			{5,0b1110111111},
			{4,0b1110011111},
			{3,0b1100011111},
			{2,0b1110111111},
			{1,0b1110111111},
			{0,0b1111011111}, })
		},
		{"terrible",
			TetrisMap(10, 40, {
			{11,0b1111111100},
			{10,0b1111111100},
			{9,0b1000000000},
			{8,0b1000000000},
			{7,0b1001111111},
			{6,0b1001111111},
			{5,0b1000000001},
			{4,0b1000000001},
			{3,0b1111111001},
			{2,0b1111111001},
			{1,0b0000000001},
			{0,0b0000000001}, })
		},
		{"empty",
			TetrisMap(10, 40)
		}
	};

	std::array pieces{
		TetrisNode::spawn(Piece::T, nullptr, 19),
		TetrisNode::spawn(Piece::O, nullptr, 19),
		TetrisNode::spawn(Piece::Z, nullptr, 19),
		TetrisNode::spawn(Piece::S, nullptr, 19),
		TetrisNode::spawn(Piece::J, nullptr, 19),
		TetrisNode::spawn(Piece::L, nullptr, 19),
		TetrisNode::spawn(Piece::I, nullptr, 19),
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
				auto nodes = Search::search<0, Vec<TetrisNode>>(node, map);
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

auto tspin_detect(const TetrisMap& map) {
	auto tslot_tsd = [](const TetrisMap& map) -> std::optional<TetrisNode> {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h1 <= h2 - 1 &&
				map(x, h2) && !map(x, h2 + 1) && map(x, h2 + 2)
				) {
				//std::cout << "find tsd with left\n";
				TetrisNode T{ Piece::T,x,h2,2 };
				std::cout << T.mapping(map) << T;
				return TetrisNode{ Piece::T, x, h2, 2 };
			}
			else if (h1 <= h0 - 1 &&
				map(x + 2, h0) && !map(x + 2, h0 + 1) && map(x + 2, h0 + 2)
				) {
				//std::cout << "find tsd with right\n";
				TetrisNode T{ Piece::T,x,h0,2 };
				std::cout << T.mapping(map) << T;
				return TetrisNode{ Piece::T, x, h0, 2 };
			}
		}
		return std::nullopt;
	};

	auto tslot_tst = [](const TetrisMap& map) -> std::optional<TetrisNode> {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h0 <= h1 && map(x - 1, h1 + 1) == map(x - 1, h1 + 2) &&
				!map(x + 1, h1 - 1) &&
				!map(x + 2, h1) && !map(x + 2, h1 - 1) && !map(x + 2, h1 - 2) && !map(x + 2, h1 + 1) && map(x + 2, h1 + 2)
				) {
				//std::cout << "find tst with left\n";
				//TetrisNode T{ Piece::T, x + 1, h1 - 2, 3 };
				//std::cout << T.mapping(map) << T;
				return TetrisNode{ Piece::T, x + 1, h1 - 2, 3 };
			}
			else if (h2 <= h1 && map(x + 3, h1 + 1) == map(x + 3, h1 + 2) &&
				!map(x + 1, h1 - 1) &&
				!map(x, h1) && !map(x, h1 - 1) && !map(x, h1 - 2) && !map(x, h1 + 1) && map(x, h1 + 2)
				) {
				//std::cout << "find tst with right\n";
				//TetrisNode T{ Piece::T,x - 1,h1 - 2,1 };
				//std::cout << T.mapping(map) << T;
				return TetrisNode{ Piece::T, x - 1 ,h1 - 2,1 };
			}
		}
		return std::nullopt;
	};

	return tslot_tst(map);
}


auto cave_tslot = [](const TetrisMap& map, const TetrisNode& piece) -> std::optional<TetrisNode> {
	auto node = piece.drop(map);
	auto& x = node.x, & y = node.y;
	if (node.rs == 1) {
		if (!map(x, y + 1) && map(x, y) && map(x, y + 2) && map(x + 2, y)) {
			auto T = TetrisNode{ Piece::T, x, y, 2 };
			std::cout << "find tst with left - 1\n";
			std::cout << T.mapping(map) << T;
			return T;
		}
		else if (
			!map(x + 2, y) && !map(x + 3, y) && !map(x + 2, y - 1) &&
			map(x, y + 1) && map(x + 3, y + 1) && map(x + 1, y - 2) && map(x + 3, y - 2)
			) {
			auto T = TetrisNode{ Piece::T, x + 1, y - 1, 2 };
			std::cout << "find tst with left - 2\n";
			std::cout << T.mapping(map) << T;
			return T;
		}
	}
	if (node.rs == 3) {
		if (!map(x + 2, y + 1) && map(x, y) && map(x + 2, y + 2) && map(x + 2, y)) {
			auto T = TetrisNode{ Piece::T, x, y, 2 };
			std::cout << "find tst with right - 1\n";
			std::cout << T.mapping(map) << T;
			return T;
		}
		else if (
			!map(x , y) && !map(x -1, y) && !map(x, y - 1) &&
			map(x-1, y + 1) && map(x -1, y -2) && map(x + 1, y - 2) && map(x + 2, y + 1)
			) {
			auto T = TetrisNode{ Piece::T, x - 1, y - 1, 2 };
			std::cout << "find tst with right - 2\n";
			std::cout << T.mapping(map) << T;
			return T;
		}
	}
	return std::nullopt;
};

void other() {
#define USE_TSLOT 1
#if USE_TSLOT == 0
	TetrisMap map(10, 20, {
	{7,0b0000000000},
	{6,0b0000000000},
	{5,0b0000000000},
	{4,0b0000000000},
	{3,0b0000000000},
	{2,0b0001000000},
	{1,0b0000000000},
	{0,0b0001010000}, });
#elif USE_TSLOT == 1
	/*TetrisMap map(10, 20, {
	{7,0b0000000000},
	{6,0b0000000000},
	{5,0b0000000000},
	{4,0b0000011001},
	{3,0b0000010001},
	{2,0b0000010110},
	{1,0b0000010010},
	{0,0b0000010110}, });*/
	/*TetrisMap map(10, 20, {
	{7,0b0000000000},
	{6,0b0000000000},
	{5,0b0000000000},
	{4,0b0001001100},
	{3,0b0001000100},
	{2,0b0000010100},
	{1,0b0000000100},
	{0,0b0000010100}, });*/
	TetrisMap map(10, 20, {//cave_slot_left_1
	{7,0b0000000000},
	{6,0b0000000000},
	{5,0b0000000000},
	{4,0b0000000000},
	{3,0b0000000000},
	{2,0b0000100000},
	{1,0b0000000000},
	{0,0b0000101000}, });
	/*TetrisMap map(10, 20, {//cave_slot_left_2
	{7,0b0000000000},
	{6,0b0000000000},
	{5,0b0000000000},
	{4,0b0000000000},
	{3,0b0000000000},
	{2,0b0001001000},
	{1,0b0000000000},
	{0,0b0000101000}, });*/
#endif
#undef USE_TSLOT
	auto landpoint = TetrisNode::spawn(Piece::T, &map);
	landpoint.y = 1;
	landpoint.x -= 0;
	landpoint.rs =3;
	std::cout << map << "\n";
	std::cout << landpoint.mapping(map) << landpoint << "\n\n";
	cave_tslot(map, landpoint);
	//tspin_detect(map);
}

int main()
{
#if TestSpeed
	/*std::size_t times{100000}, i = 0, costs = 0;
	for (int i = 0; i < times; i++) {
		auto t1 = Timer::measure<Timer::us>([]() {
			std::vector<std::pair<std::optional<TetrisNode>, int>> vec;
			vec.reserve(1600);
			});
		costs += t1;
	}
	std::cout << "iterations " << times << " \navg " << double(costs) / times << "us" << "\n";*/
	bench();
	//other();
#else
#if Play
	MyTetris tetris;
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
	std::cout << (std::is_trivial_v<std::pair<TetrisNode, int>> ? "true" : "false") << "\n";
	/*return 0;
	std::vector<char> then;
	Vec<Pos> vec;
	vec.reserve(100);
	vec.push(Pos{2,1});
	vec.emplace_back(3, 2);
	vec.emplace_back(5, 7);
	vec.emplace_back(-1, -2);
	vec.emplace_back(-3, -5);
	std::cout << "size:" << vec.size() << "\n";

	auto it = vec.begin();
	it += 2;
	std::cout << it->x << "," << it->y << "\n";
	for (auto i : vec) {
		std::cout << "x:" << i.x << " y:" << i.y << "\n";
	}

	vec.insert(it, Pos{ 999,999 });

	std::cout << "\nsize:" << vec.size() << "\n";
	for (auto i : vec) {
		std::cout << "x:" << i.x << " y:" << i.y << "\n";
	}*/

	auto t1 = Timer::measure<Timer::us>([]() {
		std::vector<TetrisNode> vec;
		for (auto i = 0; i < 40; i++) {
			vec.emplace_back();
		}
		});
	std::cout << "t1:" << t1 << "ms\n";

	auto t2 = Timer::measure<Timer::us>([]() {
		Vec<TetrisNode> vec;
		for (auto i = 0; i < 40; i++) {
			vec.emplace_back();
		}
		/*std::vector<TetrisNode> vec2(vec.size());
		std::memcpy(vec2.data(), vec.data(), vec.size() * decltype(vec)::type_size);
		for (auto& n : vec2) {
			std::cout << n << "\n";
		}*/
		});
	std::cout << "t2:" << t2 << "ms\n";

#endif
#endif
}
