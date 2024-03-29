#include <iostream>
#include <map>
#include <list>
#include <algorithm>
#include <execution>

#define CLASSIC
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "timer.hpp"
#include "tool.h"
#include "threadPool.h"
#include "pso.h"


struct EvalArgs {
	using Evaluator = Evaluator<TreeContext<TreeNode>>;
	Evaluator::BoardWeights board_weights;// = Evaluator::BoardWeights::default_args();
	Evaluator::PlacementWeights placement_weights;// = Evaluator::PlacementWeights::default_args();;
};

class MyTetris : public TetrisGame {
public:
	int thread_num = 1;

	using TetrisGame::map;
	int if_test = 2;
	Piece test_piece = Piece::L;


	void init(const EvalArgs& args) {
		restart();
		if (if_test == 1 && gd.pieces == 0) {
			for (int i = 0; i < 16; i++)
				map[i] = 0111111110_r;
			map.update(true);
			tn = gen_piece(test_piece);
			for (auto& p : rS.displayBag) {
				p = test_piece;
			}
		}
		evalArgs = args;
	}

	void Draw() {
		auto drawField = [&]() {
			std::string str;
			str.reserve(1000);
			auto& points = gd.pieceChanges;
			auto it = points.begin();
			auto h = map.height >= 20 ? 19 : map.height - 1;
			for (int y = h; y >= 0; --y) {//draw field
				if (y < 10)
					str += " ";
				str += std::to_string(y) + " ";
				for (int x = 0; x < map.width; x++)
				{
					if (it != points.end() && it->y == y && it->x == x) {
						if (map(it->x, it->y))
							str += "//";
						++it;
					}
					else {
						str += (map(x, y) == 1 ? "[]" : "  ");
					}
				}
				str += "\n";
			}
			std::cout << str;
		};

		auto drawStatus = [&]() {
			std::string str;
			str.reserve(1000);
			str += "\n";
			str += std::to_string(tn.type) + " ";
			for (auto& one : rS.displayBag)
				str += std::to_string(one);
			str += "[";
			if (hS.type != Piece::None)
				str += std::to_string(hS.type);
			str+= "]";
			str += " found:" + std::to_string(nodeCounts) + "\033[0K\n";
			str += "b2b:" + std::to_string(gd.b2b > 0) + " combo:" + std::to_string(gd.combo) + " clear_lines:" + std::to_string(gd.clear) + " pieces:" + std::to_string(gd.pieces) + "\033[0K\n";
#ifdef SHOW_PATH
			std::cout << fromN << " -> " << toN << "\033[0K\n";
			std::cout << "[" << pathCost << "us] path:" << pathStr << "\033[0K\n";
#endif
			str += gd.clearTextInfo + "\033[0K\n";
			if (!info.empty())
				str += "info:" + info + "\033[0K\n";
			std::cout << str;
		};

		drawField();
		drawStatus();
	}

	std::string Draw_Field() {
		std::string str;
		auto& points = gd.pieceChanges;
		auto it = points.begin();
		auto h = map.height >= 20 ? 19 : map.height - 1;
		for (int y = h; y >= 0; --y) {//draw field
			//std::cout << std::setw(2) << y << " ";
			for (int x = 0; x < map.width; x++)
			{
				if (it != points.end() && it->y == y && it->x == x) {
					if (map(it->x, it->y))
						str += "//";
					++it;
				}
				else {
					str += (map(x, y) == 1 ? "[]" : "  ");
				}
			}
			str += "\n";
		}
		return str;
	}

	const TetrisNode& cur() {
		return tn;
	}

	void callBot(const int limitTime) {
		if (if_test == 1 && gd.pieces >= 300) {
			if_test = 2;
			auto test_pieces = gd.pieces;
			auto args = evalArgs;
			init(args);
			gd.pieces += test_pieces;
		}
		playPath(caculateBot(tn, limitTime));
		if (if_test == 1) {
			for (auto& p : rS.displayBag) 
				p = test_piece;
		}
	}

	TetrisNode caculateBot(const TetrisNode& start, const int limitTime) {
		TreeContext<TreeNode> ctx;
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 0);
		auto upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;

		ctx.isOpenHold = false;
		ctx.evalutator.board_weights = evalArgs.board_weights, ctx.evalutator.placement_weights = evalArgs.placement_weights;

		ctx.createRoot(start, map, dp, hS.type, hS.able, !!gd.b2b, gd.combo, dySpawn);
		ctx.run(0);
		auto [best, ishold] = ctx.getBest(upAtt);

		if (best == nullptr) 
			return start;
	
		nodeCounts = ctx.count;
		info = ctx.info;
		return *best;
	}

	void playPath(const TetrisNode& result) {
		tn = result;
		opers(Oper::HardDrop);
	}

	void setOpponent(TetrisGame* player) {
		opponent = player;
	}

	template<class F>
	auto run(F f) {
		while (!gd.deaded) {
			callBot(1);
			f(gd.clear, false);
			//Draw();
		//	std::cout << "\033[1;1H";
		}
	//	std::cout << "clear: " << gd.clear << "\t";
		return gd.clear;
	}

	std::string pathStr, info;
	int nodeCounts{}, pathCost{};
	EvalArgs evalArgs;
};


void bench();

struct Fitness
{

	template<class T>
	double operator()(T& p) const
	{
		using BoardWeights_t = decltype(EvalArgs::board_weights);
		static std::atomic<std::size_t> best_clears = 0;
		auto c = 0.;
		auto session = 10;

		auto output = [&](int clear, bool ignore) {
			std::ostringstream os;
			static thread_local int last_clear{};

			if (ignore || clear - last_clear > 50000) {
				os.clear();
				os << "clear: " << std::to_string(clear) << (!ignore ? "\n" : "") << "\t" << " parms: \n";
				auto symbol_indent = (ignore ? "" : "\t       ");
				auto indent = std::string(symbol_indent) + "  ";
				os << symbol_indent << "[\n";

				for (auto it = p.begin(); it != p.end(); ++it) {
					auto it_index = std::distance(p.begin(), it);
					auto [str, str_end] = BoardWeights_t::vecparms_desccription(it_index);
					if (str != "")
						os << indent;
					os << str << *it << str_end;
					//os << "\n";
					if (it + 1 == p.end())
						os << symbol_indent << "]\n";
					//os << (*it > 0 ? " " : "") << std::to_string(*it) << (it + 1 != p.end() ? ", " : "]\n");
				}

				{
					std::scoped_lock lock(pso::out_mtx);
					if (!ignore)
						std::cout << "proc: " << last_clear << " / ";
					std::cout << os.str();
				}
				last_clear = clear;
			}
			};

		for (int i = 0; i < session; i++) {
			MyTetris ai;
			EvalArgs args;
			BoardWeights_t::from_vecparms(args.board_weights, p);
			ai.init(args);
			auto cl = ai.run(output);
			c += cl;
		}

		c /= session;

		if (best_clears <= c)
		{
			best_clears = c;
			output(c, true);
		}
		return c;
	}
};



struct test1 {
	template<class T>
	double operator()(T& p) {
		auto x = p[0];
		return x * x + 2 * x + 3;
	}
};

void test_others() 
{
	/*CE::CrossEntropyMethod<Fitness> cem(10, 20, 100, 10, {
		{-10, 0, 0, 0, -10, -10,-10,-10,-10,-10},
		{ -1, 0, 0, 0,  -1,  -1, -1, -1, -1, -1}
	}, true);
	cem.opt();*/

	pso::ParticleSwarmOptimization<Fitness> pso{};
	pso.set_bound(pso::Bound{
		{{-15, -1},  {0, 0}, {0, 0}, {0, 0}, {-40, -1}, {-15, -1}, {-15, -1}, {-10, -1}, { -30,-1}, { -30,-1}},
		{{-3, 3}, {0, 0}, {0, 0}, {0, 0}, {-8, 8},  {-3, 3},   {-3, 3},   {-2, 2},   {-6, 6},   {-6, 6}}
	});

	pso.set_particle_num(500);
	pso.set_iterations(1000);
	pso.init_particles();
	pso.run();

}

int main()
{
	std::ios_base::sync_with_stdio(false);
	test_others();
}