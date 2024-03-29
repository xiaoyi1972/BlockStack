#include <iostream>
#include <map>
#include <algorithm>
#include <execution>
#include <list>
#include <set>

#include "timer.hpp"
#include "tool.h"
#include "threadPool.h"

#include "tetrisGame.h"
//#define TIMER_MEASURE
//#define SHOW_PATH
#include "tetrisCore.h"

#include <unordered_map>



//#include<vld.h>

#ifndef OTHER_CODE
struct EvalArgs {
	using Evaluator = Evaluator<TreeContext<TreeNode>>;
	Evaluator::BoardWeights board_weights;// = Evaluator::BoardWeights::default_args();
	Evaluator::PlacementWeights placement_weights;// = Evaluator::PlacementWeights::default_args();;
};

class MyTetris : public TetrisGame {
public:

	using TetrisGame::map;
	using TetrisGame::dySpawn;
	using TetrisGame::gd;
	using TetrisGame::restart;

	void init(const EvalArgs& args) {
		restart();
		evalArgs = args;
	}

	void drawField_with(std::ostream& os) {
		auto upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		auto& points = gd.pieceChanges;
		auto it = points.begin();
		auto h = map.height >= 20 ? 19 : map.height - 1;
		for (int y = h; y >= 0; --y) {//draw field
			os << std::setw(2) << y << " ";
			for (int x = 0; x < map.width; x++)
			{
				if (it != points.end() && it->y == y && it->x == x) {
					if (map(it->x, it->y))
						os << "//";
					++it;
				}
				else {
					os << (map(x, y) == 1 ? "[]" : "  ");
				}
			}
			if (y < upAtt) os << "#";
			else os << " ";
			os << "\n";
		}

		{
			std::ostringstream out;

			out << tn.type << " ";
			for (auto& one : rS.displayBag)
				out << std::to_string(one);
			out << "[";
			if (hS.type != Piece::None)
				out << hS.type;
			out << "]";

			os << "  " << std::left << std::setw(map.width * 2 + 1 + 3) << out.str() << "\033[0K\n";
		}

		os << "  piece:" << std::setw(map.width * 2 + 1) << gd.pieces << "\033[0K\n";
		os << "  info:" << std::left << std::setw(map.width * 2 + 1) << info << "\033[0K\n";
		os << "  attack:" << std::setw(map.width * 2 + 1) << gd.attack << "\033[0K\n";
	};

	void Draw() {
		static std::ostringstream os;
		os.str("");

		auto drawField = [&]() {
			auto& points = gd.pieceChanges;
			auto it = points.begin();
			auto h = map.height >= 20 ? 19 : map.height - 1;
			for (int y = h; y >= 0; --y) {//draw field
				if (y < 10) 
					os << " ";
				os << y << " ";
				for (int x = 0; x < map.width; x++)
				{
					if (it != points.end() && it->y == y && it->x == x) [[unlikely]] {
						if (map(it->x, it->y))
							os << "//";
						++it;
						}
					else [[likely]] {
						os << (map(x, y) == 1 ? "[]" : "  ");
						}
				}
				os << "\n";
			}
	};

		auto drawStatus = [&]() {
			os << "\n";
			os << tn.type << " ";
			for (auto& one : rS.displayBag)
				os << std::to_string(one);
			os << "[";
			if (hS.type != Piece::None)
				os << hS.type;
			os << "]";
			os << " found:" << nodeCounts << "\033[0K\n";
			os << "b2b:" << (gd.b2b > 0) << " combo:" << gd.combo
				<< " clear_lines:" << gd.clear << " pieces:" << std::to_string(gd.pieces) << "\033[0K\n";
//#define SHOW_PATH
#ifdef SHOW_PATH
			os << *migrate << " -> " << *landpoint << "\033[0K\n";
			os << "[" << pathCost << "us] path:" << pathStr << "\033[0K\n";
#endif
			os << gd.clearTextInfo << "\033[0K\n";
			if (!info.empty())
				os << "info:" << info << "\033[0K\n";
			};

//#define SIMPLE_OUT

#ifndef SIMPLE_OUT
		drawField();
		drawStatus();
#else
		//drawStatus();
		static int lines = 0;
		//std::cout << std::boolalpha << gd.deaded << "\n";
		if (gd.deaded) {
			os << "end:" << gd.clear << "\033[0K\n" << "\n";
		}
		else {
			if (gd.clear - lines > 50000) {
				os << "proc: " << gd.clear << " / clear: " << lines << "\n";
				lines = gd.clear;
			}
		}
#endif
		std::cout << os.str();
	}

	const TetrisNode& cur() {
		return tn;
	}

	void callBot(const int limitTime) {
#ifdef USE_OLD_caculateBot
		playPath(caculateBot(tn, limitTime));
#endif
		caculateBot(tn, map, limitTime);
		auto action = getStep();
		playPath(action);
	}


	void caculateBot(const TetrisNode& start, const TetrisMap& field, const int limitTime) {
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
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
			.path_sd = false
		};
		bot.run(state, limitTime);
	}

	void caculateBot(const int limitTime) {
		caculateBot(tn, map, limitTime);
	}

	std::optional<std::vector<Oper>> getStep() {
		TetrisDelegator::OutInfo tag{};
		state.upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		auto path = bot.suggest(state, tag);
		if (!path) {
			path = std::vector<Oper>{ Oper::HardDrop };
		}
		//pull information
		nodeCounts = tag.nodeCounts;
		info = tag.info;
		migrate = tag.migrate;
		landpoint = tag.landpoint;
		pathCost = tag.pathCost;
		return *path;
	}

#ifdef USE_OLD_caculateBot
	std::optional<std::vector<Oper>> caculateBot(const TetrisNode& start, const int limitTime) {
#ifdef  CLASSIC
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 0);
#else
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
#endif

		TetrisDelegator::GameState state{
			.dySpawn = dySpawn,
			.upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0,
			.bag = &dp,
			.cur = &start,
			.field = &map,
			.canHold = hS.able,
			.hold = hS.type,
			.b2b= !!gd.b2b, 
			.combo = gd.combo,
			.path_sd = false
		};

		TetrisDelegator::OutInfo tag{};
		bot.run(state, limitTime);
		auto path = bot.suggest(state, tag);

		nodeCounts = tag.nodeCounts;
		info = tag.info;
		migrate = tag.migrate;
		landpoint = tag.landpoint;
		pathCost = tag.pathCost;
		return path;
	}
#endif

	void lock_simple() {
		auto [pieceChanges, clear] = tn.attachs(map);
		std::sort(pieceChanges.begin(), pieceChanges.end(), [](auto& lhs, auto& rhs) {
			return lhs.y == rhs.y ? lhs.x < rhs.x : lhs.y > rhs.y; });
		for (auto it = pieceChanges.begin(); !clear.empty() && it != pieceChanges.end(); ) {
			auto dec = 0;
			auto del = false;
			for (auto n : clear) {
				if (it->y == n) {
					del = true;
					break;
				}
				else if (it->y > n) dec++;
			}
			if (del) it = pieceChanges.erase(it);
			else {
				if (it == pieceChanges.begin() && dec == 0) break;
				it->y -= dec; ++it;
			}
		}
		gd.pieceChanges = std::move(pieceChanges);
		gd.clear += clear.size();
		gd.pieces++;
		tn = gen_piece(rS.getOne());
	}

	void playPath(const std::optional<std::vector<Oper>>& path) {
#ifdef  CLASSIC
		if (landpoint)
			tn = *landpoint;
		lock_simple();
#else
		if (path) {
#ifdef SHOW_PATH
			pathStr = std::to_string(*path);
#endif
			for (auto& i : *path) {
				opers(i);
			}
		}
		else {
			opers(Oper::HardDrop);
		}
#endif
	}

	void setOpponent(TetrisGame* player) {
		opponent = player;
	}

	std::string pathStr, info;
	int nodeCounts{}, pathCost{};
	std::optional<TetrisNode> migrate, landpoint;
	EvalArgs evalArgs;
	TetrisDelegator::GameState state{};
	TetrisDelegator bot = TetrisDelegator::launch({
		.use_static = true,
		.delete_byself = true,
		.thread_num = 2u
	});
};


void bench();
void test_others();
void test_treeNode_death_sort();

#define TestSpeed 0
#define Play 1

void test_const_GameState(const TetrisDelegator::GameState& v) {
	const auto& [dySpawn, upAtt, bag_v, cur_v, field_v, canHold, hold, b2b, combo, path_sd] = v;
	auto& bag = bag_v();
	std::cout << bag.size();
	auto dp = std::vector(bag.begin(), bag.begin() + 6);
	return;
}


void test_temp_vec(std::vector<Piece>& dp) {
	dp.emplace_back(Piece::O);
	return;
}

int main()
{
#ifdef UNIT
	TetrisDelegator::GameState v;
	TetrisDelegator::OutInfo tag;

	std::vector<Piece> dp{ Piece::T };

	v.field.emplace<TetrisMap>(10, 40);
	v.bag = &dp;//.emplace<std::vector<Piece>>();

	//auto& [dySpawn, upAtt, bag_v, cur_v, field_v, canHold, hold, b2b, combo, path_sd] = v;
	//auto& bag = bag_v();

	//bag.push_back(Piece::T);

	//test_const_GameState(v);

	test_temp_vec(v.bag);

	/*

	auto& map = v.field();
	auto& bag = v.bag();


	bag.push_back(Piece::T);*/


	//bag.push_back(Piece::T);


	return 1;
#endif


#if TestSpeed
	TetrisMap map(10, 24);
	map[19] = 1000000011_r;
	map[18] = 0000001111_r;
	map[17] = 1011111011_r;
	map[16] = 1111111110_r;
	map[15] = 1111111110_r;
	map[14] = 1011111111_r;

	map.update();
	auto piece = TetrisNode::spawn(Piece::T, nullptr, 19);
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
	tetris.init(tetris.evalArgs);

	//tetris.setOpponent(&tetris);
	size_t speed{ 300 };
	std::cout << "input time for calculating:";
	std::cin >> speed;
	std::cout << "\033[2J\033[1;1H"; //\033[2J

	//tetris.Draw();
#ifdef SHOW_WITH_INTERVAL
	auto delay = 1000;
	auto start = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
#endif
	for (int i = 0; !tetris.gd.deaded; i++) {
		try {
			tetris.callBot(speed);
		}
		catch (std::exception& e) {
			std::cout << "exception: " << e.what() << std::endl;
		}
#ifdef SHOW_WITH_INTERVAL
		if (auto end = std::chrono::steady_clock::now(); end > start)
		{
#endif
			tetris.Draw();
#ifdef SHOW_WITH_INTERVAL
			start = end + std::chrono::milliseconds(delay);
		}
#endif
		std::cout << "\033[1;1H";
	}
#else
	//test_others();
	bench();

#ifdef USE_PLAY_BATTLE
	struct Player {
		MyTetris unit{};
		std::size_t wins{ 0 };
	}player[2];

	auto render = [&]() {
		std::stringstream os[2];
		player[0].unit.drawField_with(os[0]);
		player[1].unit.drawField_with(os[1]);
		std::cout << "\033[1;1H";
		for (std::string line; std::getline(os[0], line);) {
			std::cout << line;
			std::getline(os[1], line);
			std::cout << "\t" << line << "\n";
		}
	};

	//set args
	player[0].unit.bot.set_thread_num(1);
	player[1].unit.bot.set_thread_num(2);
	//player[0].unit.bot.set_use_static(false);
	player[0].unit.setOpponent(&player[1].unit);
	player[1].unit.setOpponent(&player[0].unit);

	auto matches = 50, cal_time = 100;
	const char* sign = "\\|/-";
	int idx = 0;

	std::cout << "\033[2J\033[1;1H";
	for (auto i = 0; i < matches; i++) {
		for (auto& p : player) {
			p.unit.restart();
		}
		while (!player[0].unit.gd.deaded && !player[1].unit.gd.deaded) {
			std::vector<std::future<void>> exec;
			for (auto& p : player) {
				exec.emplace_back(std::async(std::launch::async, [&]() { p.unit.caculateBot(cal_time); }));
			}
			for (auto& task : exec) {
				task.get();
			}
			for (auto& p : player) {
				auto action = p.unit.getStep();
				p.unit.playPath(action);
			}

			std::cout << "\033[1;1H";
			//player[1].unit.Draw();

			render();
			//std::cout << "\033[1;1H";
			std::cout << sign[idx] << " process: " << i << "/" << matches << "\033[0K\n";
			std::cout << "\033[0K\n1 thread won: " << player[0].wins << "\033[0K\n2 thread won: " << player[1].wins << "\033[0K\n";
			idx++;
			idx &= 3;
	
		}
		player[0].wins += player[1].unit.gd.deaded;
		player[1].wins += player[0].unit.gd.deaded;
	}
	std::cout << "\033[2J\033[1;1H";
	std::cout << "total games: " << matches 
		<< "\n 1 thread won: " << player[0].wins << "\n 2 thread won: " << player[1].wins << "\n";
#endif

#endif
#endif
}

#include<unordered_set>
void test_others() {

	std::cout << "std::atomic<std::shared_ptr<int>>:" << std::boolalpha << std::atomic<std::shared_ptr<TetrisNode>>::is_always_lock_free << "\n";
#ifdef TEST_THIS_ONE
	struct M {
		int value;
		bool dead;

		bool operator<(const M& other) {
			if (dead == other.dead) {
				return value > other.value;
			}
			else
				return dead < other.dead;
		}
	};

	auto print = [](auto& v) {
		for (auto& el : v) {
			std::cout << "{value: " << el.value << ", dead:" << std::boolalpha << el.dead << "}\n";
		}
	};

	std::vector<M> vec{
		/*{4,false},
		{5,false},*/
		{1,true},
		{3,true},
		//{2,false},
	};

	std::cout << "before:\n";
	print(vec);
	std::sort(vec.begin(), vec.end());
	std::cout << "\nafter:\n";
	print(vec);

	std::size_t death_index{};

	for (auto it = vec.begin(); it != vec.end(); ++it) {
		if (it->dead) {
			death_index = -std::distance(vec.end(), it);
			break;
		}
	}

	std::cout << "\ndeath_index:" << death_index;

	auto offset = 1;

	//std::cout << "\nlaile " << std::boolalpha << (std::distance(vec.begin(), vec.end()) < offset) << "\n";
	
	std::span vec1{ 
		vec.begin() + (std::distance(vec.begin(), vec.end() - death_index) < offset ? 0 : offset) ,
		vec.end() - death_index 
	};

	std::cout << "\nvec1:\n";
	print(vec1);


#endif
#ifdef TEST_RANDOM_SELECT_GENERATOR
	using rand_type = std::mt19937; //std::minstd_rand;
#define init_seed() (static_cast<rand_type::result_type>(std::chrono::system_clock::now().time_since_epoch().count()) \
		+ std::hash<std::thread::id>{}(std::this_thread::get_id()))
	rand_type rng(init_seed());
#undef init_seed
	std::uniform_real_distribution<> d(0, 1);
	const double exploration = std::log(100);
	auto select = [&](std::size_t _range) {
		return static_cast<std::size_t>(
			std::fmod(-std::log(1. - d(rng)) / exploration, _range)
			);
		};
	std::map<std::size_t, std::size_t> lut{};
	int amount = 20000;
	std::size_t range = 34; //34;
	for (auto i = 0; i < amount; i++) {
		auto result = select(range);
		if (lut.contains(result)) {
			lut[result]++;
		}
		else {
			lut.try_emplace(result, 1);
		}
	}

	for (auto& it : lut) {
		std::cout << "value:" << std::setw(2) << it.first << " probability:" << double(it.second) / amount << "\n";
	}
#endif
#ifdef TEST_WASTE_T
	auto test_wast_t = [](const TetrisNode& node, int clear) {
		bool result = false;
		if (node.type == Piece::T && !(node.typeTSpin == TSpinType::TSpin && clear > 1))
			result = true;
		return result;
		};

	struct NodeParm {
		TetrisNode node;
		int clear;
	};

	auto get_node = [](TSpinType t_type, int clear) -> NodeParm {
		auto node = TetrisNode::spawn(Piece::T);
		node.typeTSpin = t_type;
		return { .node = node, .clear = clear };
		};

	std::array types{ TSpinType::None,TSpinType::TSpinMini,TSpinType::TSpin };

	for (const auto& type : types) {
		for (auto i = 0; i < 4; ++i) {
			if (type == TSpinType::TSpinMini && i > 1)
				break;
			auto np = get_node(type, i);
			std::cout << type << " with clear(" << i << ") -> " << std::boolalpha << test_wast_t(np.node, np.clear) << "\n";
		}
	}
#endif
#ifdef TEST_MAP_HASH
	std::unordered_map<TetrisMap,int> lut;
	std::ostringstream os;
	TetrisMap field(10, 22);
	field[2] = 1100000001_r;
	field[1] = 1100000111_r;
	field[0] = 1111101111_r;
	field.update();

	auto field1 = field;

	//lut.insert({ field,1 });

	{
		if (lut.contains(field1)) {
			os << "1st find a map\n";
		}
		else {
			os << "not find\n";
			lut.try_emplace(field, 1);
		}
	}

	auto node = TetrisNode::spawn(Piece::S, &field);
	os << node.mapping(field);

	//node.attach(field1);

	{
		if (lut.contains(field1)) {
			os << "2nd find a map\n";
		}
		else {
			os << "not find\n";
			lut.try_emplace(field, 1);
		}
	}

	std::cout << os.str();

#endif
#ifdef TEST_SEARCH_LANDPOINT
	std::ostringstream os;
	TetrisMap field(10, 22);
	field[2] = 1100000001_r;
	field[1] = 1100000111_r;
	field[0] = 1111101111_r;
	field.update();
	auto node = TetrisNode::spawn(Piece::S, &field);
	os << node.mapping(field);
	auto landpoints = Search::search(node, field);
	for (auto& p : landpoints) {
		os << p.mapping(field) << "\n";
		os << "lastRotate:" << p.lastRotate << " step:" << p.step << "\n\n";
	}
	std::cout << os.str();
#endif
#ifdef TEST_TETRISNODE_FILTER
	std::ostringstream os;
	std::vector<TetrisNode> nodes;
	nodes.emplace_back(Piece::T, 0, 6, 0);
	nodes.emplace_back(Piece::T, 1, 5, 0);
	nodes.emplace_back(Piece::T, 2, 5, 0);
	nodes.emplace_back(Piece::T, 3, 4, 0);
	nodes.emplace_back(Piece::T, 4, 4, 0);
	nodes.emplace_back(Piece::T, 5, 4, 0);
	nodes.emplace_back(Piece::T, 5, 1, 0);
	nodes.emplace_back(Piece::T, 6, 6, 0);
	nodes.emplace_back(Piece::T, 6, 0, 0);
	nodes.emplace_back(Piece::T, 7, 6, 0);
	nodes.emplace_back(Piece::T, -1, 7, 1);
	nodes.emplace_back(Piece::T, 0, 6, 1);
	nodes.emplace_back(Piece::T, 1, 6, 1);
	nodes.emplace_back(Piece::T, 2, 5, 1);
	nodes.emplace_back(Piece::T, 3, 5, 1);
	nodes.emplace_back(Piece::T, 4, 5, 1);
	nodes.emplace_back(Piece::T, 5, 1, 1);
	nodes.emplace_back(Piece::T, 6, 0, 1);
	nodes.emplace_back(Piece::T, 6, 6, 1);
	nodes.emplace_back(Piece::T, 7, 7, 1);
	nodes.emplace_back(Piece::T, 0, 6, 2);
	nodes.emplace_back(Piece::T, 1, 6, 2);
	nodes.emplace_back(Piece::T, 2, 5, 2);
	nodes.emplace_back(Piece::T, 3, 5, 2);
	nodes.emplace_back(Piece::T, 4, 5, 2);
	nodes.emplace_back(Piece::T, 5, 4, 2);
	nodes.emplace_back(Piece::T, 5, 1, 2);
	nodes.emplace_back(Piece::T, 6, 0, 2);
	nodes.emplace_back(Piece::T, 6, 6, 2);
	nodes.emplace_back(Piece::T, 7, 7, 2);
	nodes.emplace_back(Piece::T, 0, 6, 3);
	nodes.emplace_back(Piece::T, 1, 6, 3);
	nodes.emplace_back(Piece::T, 2, 5, 3);
	nodes.emplace_back(Piece::T, 3, 5, 3);
	nodes.emplace_back(Piece::T, 4, 5, 3);
	nodes.emplace_back(Piece::T, 5, 4, 3);
	nodes.emplace_back(Piece::T, 5, 1, 3);
	nodes.emplace_back(Piece::T, 6, 0, 3);
	nodes.emplace_back(Piece::T, 7, 7, 3);
	nodes.emplace_back(Piece::T, 8, 7, 3);
	nodes.emplace_back(Piece::L, 0, 6, 0);
	nodes.emplace_back(Piece::L, 1, 5, 0);
	nodes.emplace_back(Piece::L, 2, 5, 0);
	nodes.emplace_back(Piece::L, 3, 4, 0);
	nodes.emplace_back(Piece::L, 4, 4, 0);
	nodes.emplace_back(Piece::L, 5, 1, 0);
	nodes.emplace_back(Piece::L, 5, 4, 0);
	nodes.emplace_back(Piece::L, 6, 2, 0);
	nodes.emplace_back(Piece::L, 6, 6, 0);
	nodes.emplace_back(Piece::L, 7, 6, 0);
	nodes.emplace_back(Piece::L, -1, 7, 1);
	nodes.emplace_back(Piece::L, 0, 6, 1);
	nodes.emplace_back(Piece::L, 1, 6, 1);
	nodes.emplace_back(Piece::L, 2, 5, 1);
	nodes.emplace_back(Piece::L, 3, 5, 1);
	nodes.emplace_back(Piece::L, 4, 5, 1);
	nodes.emplace_back(Piece::L, 5, 1, 1);
	nodes.emplace_back(Piece::L, 6, 1, 1);
	nodes.emplace_back(Piece::L, 6, 3, 1);
	nodes.emplace_back(Piece::L, 6, 7, 1);
	nodes.emplace_back(Piece::L, 7, 7, 1);
	nodes.emplace_back(Piece::L, 0, 7, 2);
	nodes.emplace_back(Piece::L, 1, 6, 2);
	nodes.emplace_back(Piece::L, 2, 6, 2);
	nodes.emplace_back(Piece::L, 3, 5, 2);
	nodes.emplace_back(Piece::L, 4, 5, 2);
	nodes.emplace_back(Piece::L, 5, 2, 2);
	nodes.emplace_back(Piece::L, 5, 5, 2);
	nodes.emplace_back(Piece::L, 6, 2, 2);
	nodes.emplace_back(Piece::L, 6, 6, 2);
	nodes.emplace_back(Piece::L, 7, 6, 2);
	nodes.emplace_back(Piece::L, 0, 6, 3);
	nodes.emplace_back(Piece::L, 1, 6, 3);
	nodes.emplace_back(Piece::L, 2, 5, 3);
	nodes.emplace_back(Piece::L, 3, 5, 3);
	nodes.emplace_back(Piece::L, 4, 5, 3);
	nodes.emplace_back(Piece::L, 5, 1, 3);
	nodes.emplace_back(Piece::L, 5, 3, 3);
	nodes.emplace_back(Piece::L, 6, 0, 3);
	nodes.emplace_back(Piece::L, 7, 7, 3);
	nodes.emplace_back(Piece::L, 8, 7, 3);
	std::set<TetrisNode> nodes1(nodes.begin(), nodes.end());

	os << "set's content:\n";
	for (auto& it : nodes1) {
		os << it << "\n";
	}
	os << "\nvec's content:\n";
	for (auto& it : nodes) {
		os << it << "\n";
	}

	os << "\nset.size():" << nodes1.size() << " vec.size():" << nodes.size() << "\n";
	std::cout << os.str();
#endif
#ifdef TEST_RECV_UPATT
	auto dySpawn = -18;

	TetrisMap map(10, 40);
	map[21] = 0000000010_r;
	map[20] = 1100000011_r;
	map[19] = 1110000011_r;
	map[18] = 1110100111_r;
	map[17] = 1111110111_r;
	map[16] = 1111110111_r;
	map[15] = 1111110111_r;
	map[14] = 1111110111_r;
	map[13] = 1101111111_r;
	map[12] = 1110111111_r;
	map[11] = 1110111111_r;
	map[10] = 1110111111_r;
	map[9] = 1110111111_r;
	map[8] = 1110111111_r;
	map[7] = 1111111110_r;
	map[6] = 1111101111_r;
	map[5] = 1111101111_r;
	map[4] = 1011111111_r;
	map[3] = 1011111111_r;
	map[2] = 1011111111_r;
	map[1] = 1111011111_r;
	map[0] = 1111011111_r;

	/*map[21] = 0000001000_r;
	map[20] = 1110001000_r;
	map[19] = 1110001000_r;
	map[18] = 1110101000_r;
	map[17] = 1111111000_r;
	map[16] = 1111111000_r;
	map[15] = 1111111000_r;
	map[14] = 1111111000_r;
	map[13] = 1101111000_r;
	map[12] = 1110111000_r;
	map[11] = 1110111000_r;
	map[10] = 1110111000_r;
	map[9]  = 1110111000_r;
	map[8]  = 1110110000_r;
	map[7]  = 1111111110_r;
	map[6]  = 1111101111_r;
	map[5]  = 1111101111_r;
	map[4]  = 1011111111_r;
	map[3]  = 1011111111_r;
	map[2]  = 1011111111_r;
	map[1]  = 1111011111_r;
	map[0]  = 1111011111_r;*/


	//map = TetrisMap(10, 40);
	//map[19] = 1111111000_r;
	map.update();

	//auto node = TetrisNode::spawn(Piece::S, &map, dySpawn);
	
	auto node = TetrisNode::spawn(Piece::J, &map, dySpawn);

	std::cout << "node.y:" << node.y << " map.roof:" << map.roof << "\n";
	std::cout << node.mapping(map) << "\n";

	auto aim = node;
	aim.rs = 1;
	aim.x = 6;
	aim.y += 1;


	std::cout << aim.mapping(map) << "\n";
	auto path = Search::make_path(node, aim, map);
	std::cout << path;

	/*std::cout << "searched nodes:\n";
	auto nodes = Search::search(node, map);
	std::sort(nodes.begin(), nodes.end());


	for (auto& p : nodes) {
		std::cout << p.mapping(map);
		std::cout << node << " -> " << p << "\n>  ";
		auto path = Search::make_path(node, p, map);
		assert((!path.empty()));
		std::cout << path << "\n\n";
	}*/

#endif
#ifdef TEST_TSLOT_DETECT

	/*
		auto tslot_tsd = [](const TetrisMap& map) -> std::optional<TetrisNode> {
			for (auto x = 0; x < map.width - 2; x++) {
				const auto h0 = map.top[x] - 1, h1 = map.top[x + 1] - 1, h2 = map.top[x + 2] - 1;
				if (h1 <= h2 - 1 && map(x, h2) && !map(x, h2 + 1) && map(x, h2 + 2)) {
					return TetrisNode{ Piece::T, x, h2, 2 };
				}
			}
			for (auto x = 0; x < map.width - 2; x++) {
				const auto h0 = map.top[x] - 1, h1 = map.top[x + 1] - 1, h2 = map.top[x + 2] - 1;
				if (h1 <= h0 - 1 && map(x + 2, h0) && !map(x + 2, h0 + 1) && map(x + 2, h0 + 2)) {
					return TetrisNode{ Piece::T, x, h0, 2 };
				}
			}
			return std::nullopt;
		};

		TetrisMap map(10, 40);

		map[4] = 1001000000_r;
		map[3] = 1001000000_r;
		map[2] = 0001000000_r;
		map[1] = 1011000000_r;
		map[0] = 0100000000_r;

		map.update();

		std::cout << map << "\n";

		auto node = tslot_tsd(map);

		if (node) {
			std::cout << "found at " << *node << "\n" << node->mapping(map) << "\n";
		}*/

	TetrisMap map(10,24);
	auto p = TetrisNode::spawn(Piece::I, &map, -3);
	map[19] = 0110000001_r;
	map[18] = 1111000001_r;
	map.update();
	std::cout << p.mapping(map);
	auto path = Search::make_path(p, p, map);
	std::cout << path;
#endif

}

void test_treeNode_death_sort() {
#ifdef TEST
	TetrisMap map{};
	TreeNode root{};

	std::random_device rd{};
	std::mt19937 gen(rd());
	std::uniform_int_distribution distrib(0, 10000);

	std::list<TreeNode> lists;
	std::vector<TreeNode*> vec;

	TreeContext<TreeNode> ctx;

	auto print_vec = [](auto&& vec) {
		auto i = 0;
		for (auto& node : vec) {
			std::cout << (i < 10 ? " " : "") << i++ << ".\t" << node << "\tvalue:"
				<< node->props.evaluation.value
				<< "\t(" << node->children.size() << ")"
				<< "leaf:" << std::boolalpha << node->leaf
				<< "\tlead_death:" << std::boolalpha << (!node->leaf && node->children.empty())
				<< '\n';
		}
		};

	for (int i = 0; i < 34; i++) {

		//TreeNode(TreeContext * _ctx, TreeNode * _parent, Piece _cur, TetrisMap && _map, Piece _hold, bool _isHold, const EvalArgs & _props) :
			//parent(_parent), context(_ctx), map(std::forward<TetrisMap>(_map)), isHold(_isHold), cur(_cur), hold(_hold), props(_props) {
		//}

		lists.emplace_back(&ctx, &root, Piece::None, map, Piece::None, false, TreeNode::EvalArgs{});
		auto& node = lists.back();
		node.props.evaluation.value = distrib(gen);
		/*if (i % 2 != 0) {
			node.leaf = true;
		}
		else {
			node.leaf = false;
			if (i % 5 == 0) {
				node.children.push_back(&node);
			}
		}*/
		vec.push_back(&node);
	}

	pdqsort_branchless(vec.begin(), vec.end(), TreeNode::TreeNodeCompare);
	print_vec(vec);

	auto i = 6;
	auto node1 = vec[i];
	auto prev_value = node1->props.evaluation.value;

	std::cout << "[" << i << "]'s original value : " << node1->props.evaluation.value << " input value changed to : ";
	std::cin >> node1->props.evaluation.value;

	auto flag = prev_value > node1->props.evaluation.value;

	TreeNode::modify_sorted_batch(vec, node1, flag);

	std::cout << "after changed:\n";
	print_vec(vec);



#endif // TEST
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
		{ "dtd",
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
		{ "terrible",
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
		{ "empty",
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
	std::size_t times{ 1000000 }, costs{ 0 }, i = 0;
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
				auto nodes = Search::search<>(node, map);
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
			std::cout << "\n" << node.type << "\niterations " << times << " \navg " << double(costs) / times << "us" << "\n";
#endif
		}
	}
}


#else 

#include<syncstream>


void hello(int a) {
	std::cout << "hello " << a << "\n";
}

template<class F = void(*)(int)>
void test(F&& f = [](auto...arg) { hello(arg...); }) {
	f(1);
}

int main(void) {

	const unsigned int thread_num = 2;// 4;
	dp::thread_pool<> pool{ thread_num };
	lock_free_stack<int> list;
	constexpr auto count = 5;
	std::vector<std::future<void>> results;
	std::array<std::atomic<int>, count> a(0);
	
	for (auto i = 0; i < count; i++) {
		list.push(i);
	}

	auto get_one = [&]() {
		//while (list.size() != 0) {
		while (true) {
			auto v = list.pop();
			if (!v)
				break;
			else {
				std::osyncstream synced_out(std::cout);
				synced_out << "value:" << *v << " from thread_" << std::this_thread::get_id() << "\n";

				a[*v]++;
				if (a[*v] > 1) {
					exit(0);
				}
			}
		}
	};

	for (auto i = 0; i < thread_num; i++) {
		results.emplace_back(pool.enqueue(get_one));
	}

	for (auto& k : results) {
		k.get();
	}
	return 0;
}

#endif // DEBUG