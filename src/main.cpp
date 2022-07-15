#include <iostream>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "tImer.hpp"
//#include"test.h"

class MyTetris : public TetrisGame {
public:
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
					if (it != points.end() && it->y  == y && it->x == x) {
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
			std::cout << "["<<pathCost<<"us] path:" << pathStr << "\033[0K\n";
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
		//static 
		TreeContext ctx;
		auto a = start;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		auto mapRef = map;
		ctx.createRoot(a, mapRef, dp, hS.type, !!gd.b2b, gd.combo, std::holds_alternative<Modes::versus>(gm) ?
			std::get<Modes::versus>(gm).trashLinesCount : 0 , dySpawn);
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
	   do {
			ctx.run();
		} while ((now = high_resolution_clock::now()) < end);

		auto [best, ishold] = ctx.getBest();
	  //  std::cout << best.mapping(map);
		std::vector<Oper>path;
		Timer t;
		if (!ishold) {
			fromN = a,toN = best;
			t.reset();
			path = Search::make_path(a, best, mapRef, false);
			pathCost = t.elapsed<Timer::us>();
		}
		else {
			auto holdNode = TetrisNode::spawn(hS.type ? *hS.type : rS.displayBag[0], &map);
			fromN = holdNode, toN = best;
			t.reset();
			path = Search::make_path(holdNode, best, mapRef, false);
			pathCost = t.elapsed<Timer::us>();
			path.insert(path.begin(), Oper::Hold);
		}
		if (path.empty()) path.push_back(Oper::HardDrop);
		pathStr = std::to_string(path);
		nodeCounts = ctx.count;
		return path;
	}

	void playPath(const std::vector<Oper>& path) {
		for (auto& i : path) {
			opers(i);
		}
	}

	std::string pathStr;
	int nodeCounts{}, pathCost{};
	TetrisNode fromN, toN;
};


#define TestSpeed 0
#define OUTPUT 0
#define Play 1

int main()
{
#if TestSpeed

	TetrisMap map{ 10,40 };
	//map[5] = {0b0000000000};
	//map[4] = { 0b1000000000 };
	//map[2] = { 0b0111111111 };
    //map[1] = { 0b0011111111 };
	map[0] = { 0b0111111111 };
	map.update();
	auto node = TetrisNode::spawn(Piece::T, &map, -18);
	std::cout << node.mapping(map) << "\n";
	return 0;
	Timer t;
	std::size_t times = 1000, costs = 0;
	bool is;
	for (int i = 0; i < times; i++) { 		//	std::cout << Timer::measure<Timer::us>([&]() { TetrisBot::search<true>(node, map); }) << "\n";
		t.reset();
		auto nodes = Search::search(node, map);
		auto cost = t.elapsed<Timer::us>();
		std::cout << cost << "us\n";
		costs += cost;
#if OUTPUT
		std::sort(nodes.begin(), nodes.end());
		std::cout << "\n";
		for (auto& landpoint : nodes) {
			landpoint.getTSpinType();
			std::cout << landpoint.mapping(map) << landpoint << " step:" << landpoint.step;
			if (landpoint.type == Piece::T) 
				std::cout << " TSpinType:"
				<< landpoint.typeTSpin
				<< " lastRotate:"
				<< (landpoint.lastRotate ? "true" : "false");
			std::cout << "\n\n";
		
		}
#endif
	}
	std::cout << "\nEnd to " << times << " times\navg " << double(costs) / times << "us" << "\n";
#else

#if Play
	MyTetris tetris;
	size_t speed{ 300 };
	std::cout << "input time for calculating:";
	std::cin >> speed;
	std::cout << "\033[2J\033[1;1H"; //\033[2J
	for (int i = 0; ; i++) {
		tetris.callBot(speed);
		//std::cout << tetris.ctx.count<<"\t";
		tetris.Draw();
		std::cout << "\033[1;1H"; //\033[2J
	}
#else
	TetrisMap map{ 10,20 };
	map[5] = { 0b0000000000 };
	map[4] = { 0b1111111110 };
	map[3] = { 0b0000000000 };
	map[2] = { 0b0001000000 };
	map[1] = { 0b0000000000 };
	map.update();
	auto node = TetrisNode::spawn(Piece::J, &map);
	auto nodes = Search::search(node, map);
	for (auto& land_node : nodes)
	{
		std::cout << land_node.mapping(map) << land_node <<
			" step(" << land_node.step<<") ";
		if (land_node.type == Piece::T) {
			land_node.getTSpinType();
			std::cout << " TSpinType:" << land_node.typeTSpin << " lastRotate:" << (land_node.lastRotate ? "true" : "false") << "\n";
		}
		std::cout << "=> ";
		auto path = Search::make_path(node, land_node, map);
		for (auto a : path) {
			std::cout << a << " ";
		}
		std::cout << "\n\n";
	}
	std::cout << "\nHello World!\n";
#endif
#endif
}
