// ansiCode.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "tetrisGame.h"
#include "tetrisBot.h"
#include "Timer.hpp"
#include <algorithm>

//#include <vld.h>
class MyTetris : public TetrisGame {
public:
	void Draw() {
		auto map_d = map;

		for (auto& point : tn.getPoints()) {
			map_d.set(point.y + tn.pos.y, point.x + tn.pos.x, true);
		}

		auto drawField = [&]() {
			auto index =  map_d.height - 23;
			for (int i = index; i < map_d.height; i++) {//draw field
				for (int j = 0; j < map_d.width; j++)
				{
					std::cout << (map_d(i, j) ? "[]" : "  ");
				}
				std::cout << "\n";
			}
		};

		auto drawStatus = [&]() {
			std::cout << "\n" << tn.type << " ";
			for(auto &one:rS.displayBag)
				std::cout << one;
			std::cout << "[" << hS.type << "]";
			std::cout << " found nodes:" << nodeCounts << "\033[0K\n";
			std::cout << "b2b:" << (gd.b2b > 0);
			std::cout << "\npath:" << pathStr << "\033[0K\n";
		};
		

		drawField();
		drawStatus();

	}

	const TetrisNode& cur(){
		return tn;
	}

	void callBot(const int limitTime) {
		playPath(caculateBot(tn, limitTime));
	}

	std::vector<Oper> caculateBot(const TetrisNode& start, const int limitTime){
		static 
			TreeContext ctx;
		auto a = start;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		auto& mapRef = const_cast<TetrisMapEx&>(map);
		ctx.createRoot(a, mapRef, dp, hS.type, !!gd.b2b, gd.combo, std::holds_alternative<Modes::versus>(gm) ? 
			std::get<Modes::versus>(gm).trashLinesCount : 0);
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
		do {
			ctx.run();
		} while ((now = high_resolution_clock::now()) < end);
		auto [best, ishold] = ctx.getBest();
		std::vector<Oper>path;
		if (!ishold)
			path = TetrisBot::make_path(a, best, mapRef, true);
		else {
			auto holdNode = TetrisNode::spawn(hS.type == Piece::None ? rS.displayBag[0] : hS.type);
			path = TetrisBot::make_path(holdNode, best, mapRef, true);
			path.insert(path.begin(),Oper::Hold);
		}
		if (path.empty()) path.push_back(Oper::HardDrop);
		pathStr.clear();
		pathStr = Tool::printPath(path);
		nodeCounts = ctx.count;
		return path;
	}

	void playPath(const std::vector<Oper> &path) {
		for (auto &i:path) {
			opers(i);
		}
	}

	std::string pathStr;
	int nodeCounts{};
};

#define Test 1
#define OUTPUT 1
int main()
{
#if Test
	timer t;
	TetrisMapEx map{ 10,40 };


//	map.data[map.height - 5] = 0b1110000000;
//	map.data[map.height - 4] = 0b1111111000;
	map.data[map.height - 3] = 0b1110000000;
	map.data[map.height - 2] = 0b1110000000;
	map.data[map.height - 1] = 0b1110111111;
	map.update();
	TetrisNode node = TetrisNode::spawn(Piece::T);

  //  node.pos = Pos{ 3,18 };
	//node.rotateState = 1;
	//node.pos.y = 18;
	for (int i = 0; i < 1; i++) {
		t.reset();
		auto nodes = TetrisBot::search<false>(node, map);
	//	auto nodes = node.getHoriZontalNodes(map);
	// 	node.attach(map);
		//std::cout <<"check:" << node.check(map) << " open:" << node.open(map) << "\n";
		auto cost = t.elapsed_micro();
		std::cout << cost << "us\n";
#if OUTPUT
		std::sort(nodes.begin(),nodes.end());
		for (auto& n : nodes) {
			n.getTSpinType();
			std::cout << n;
			if (n.type == Piece::T)
				std::cout << " " << n.typeTSpin << " " << n.lastRotate;
			std::cout << "\n";
		}
#endif
	}
#else
	MyTetris tetris;
	size_t speed{};
	std::cout << "input time for calculating:";
	std::cin >> speed;
	std::cout << "\033[2J\033[1;1H"; //\033[2J
	for (int i = 0; ; ) {
		tetris.callBot(speed);
		//std::cout << tetris.ctx.count<<"\t";
		tetris.Draw();
		std::cout << "\033[1;1H"; //\033[2J
	}
#endif
}

