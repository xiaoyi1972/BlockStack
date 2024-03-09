
#define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <mutex>
#include <fstream>
#include <thread>
#include <array>
#include <random>
#include <sstream>
#include <iostream>
#include <chrono>
#include <queue>
#include <atomic>
#include <map>
#include <algorithm>
#include <execution>

#include "tetrisGame.h"
#include "tetrisCore.h"
#include "timer.hpp"
#include "sb_tree.h"

#if _MSC_VER
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif


template <class... Args>
std::string string_format(const std::string& format, Args... args) {
	std::size_t size =
		snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
	if (size <= 0) {
		throw std::runtime_error("Error during formatting.");
	}
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args...);
	return std::string(buf.get(),
		buf.get() + size - 1); // We don't want the '\0' inside
}

namespace zzz
{
	template<size_t N> struct is_key_char
	{
		bool operator()(char c, char const* arr)
		{
			return arr[N - 2] == c || is_key_char<N - 1>()(c, arr);
		}
	};
	template<> struct is_key_char<1U>
	{
		bool operator()(char c, char const* arr)
		{
			return false;
		}
	};
	template<size_t N> void split(std::vector<std::string>& out, std::string const& in, char const (&arr)[N])
	{
		out.clear();
		std::string temp;
		for (auto c : in)
		{
			if (is_key_char<N>()(c, arr))
			{
				if (!temp.empty())
				{
					out.emplace_back(std::move(temp));
					temp.clear();
				}
			}
			else
			{
				temp += c;
			}
		}
		if (!temp.empty())
		{
			out.emplace_back(std::move(temp));
		}
	}
}

std::string data;

struct EvalArgs {
	using Evaluator = Evaluator<TreeContext<TreeNode>>;
	Evaluator::BoardWeights board_weights = Evaluator::BoardWeights::default_args();
	Evaluator::PlacementWeights placement_weights = Evaluator::PlacementWeights::default_args();;
};

struct pso_dimension
{
	double x;
	double p;
	double v;
};
union pso_data
{
	pso_data() {}
	struct
	{
		std::array<double, 64> x;
		std::array<double, 64> p;
		std::array<double, 64> v;
	};
	EvalArgs param;
};

struct pso_config_element
{
	double x_min, x_max, v_max;
};
struct pso_config
{
	std::vector<pso_config_element> config;
	double c1, c2, w, d;
};


void pso_init(pso_config const& config, pso_data& item, std::mt19937& mt)
{
	for (size_t i = 0; i < config.config.size(); ++i)
	{
		auto& cfg = config.config[i];
		item.x[i] = std::uniform_real_distribution<double>(cfg.x_min, cfg.x_max)(mt);
		item.p[i] = item.x[i];
		item.v[i] = std::uniform_real_distribution<double>(-cfg.v_max, cfg.v_max)(mt);
	}
}
void pso_logic(pso_config const& config, pso_data const& best, pso_data& item, std::mt19937& mt)
{
	for (size_t i = 0; i < config.config.size(); ++i)
	{
		auto& cfg = config.config[i];
		if (std::abs(item.v[i]) <= config.d && std::abs(best.p[i] - item.p[i]) <= std::abs(best.p[i] * config.d))
		{
			item.v[i] = std::uniform_real_distribution<double>(-cfg.v_max, cfg.v_max)(mt);
		}
		item.x[i] += item.v[i];
		item.v[i] = item.v[i] * config.w
			+ std::uniform_real_distribution<double>(0, config.c1)(mt) * (item.p[i] - item.x[i])
			+ std::uniform_real_distribution<double>(0, config.c2)(mt) * (best.p[i] - item.x[i])
			;
		item.x[i] = std::clamp(item.x[i], cfg.x_min, cfg.x_max);
		item.v[i] = std::clamp(item.v[i], -cfg.v_max, cfg.v_max);
		/*if (item.v[i] > cfg.v_max)
		{
			item.v[i] = cfg.v_max;
		}
		if (item.v[i] < -cfg.v_max)
		{
			item.v[i] = -cfg.v_max;
		}*/
	}
}

class test_ai : public TetrisGame {
public:
	using TetrisGame::gd;
	using TetrisGame::opponent;
	using TetrisGame::rS;
	using TetrisGame::hS;
	using TetrisGame::gm;
	using TetrisGame::map;
	using TetrisGame::tn;

	void init(pso_data const& data, pso_config const& config, size_t round_ms) {
		restart();
		evalArgs = data.param;
	}

	const TetrisNode& cur() {
		return tn;
	}

	void callBot(const int limitTime) {
		ctx.reset(new TreeContext<TreeNode>);
		caculateBot(tn, limitTime);
	}

	void playStep() {
		const TetrisNode& start = tn;
		auto upAtt = std::holds_alternative<Modes::versus>(gm) ? std::get<Modes::versus>(gm).trashLinesCount : 0;
		auto [best, ishold] = ctx->getBest(upAtt);
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
		nodeCounts = ctx->count;
		info = ctx->info;
		playPath(path);
	}

	void caculateBot(const TetrisNode& start, const int limitTime) {
		auto dp = std::vector(rS.displayBag.begin(), rS.displayBag.begin() + 6);
		ctx->evalutator.board_weights = evalArgs.board_weights;
		ctx->evalutator.placement_weights = evalArgs.placement_weights;
		auto init_context = [&](auto&& context) {
			context->createRoot(start, map, dp, hS.type, hS.able, !!gd.b2b, gd.combo, dySpawn);
		};
		init_context(ctx);
		auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(limitTime);
		auto taskrun = [&](auto&& context) {
			do {
				context->run();
			} while (std::chrono::steady_clock::now() < end);
		};
		taskrun(ctx);
	}

	void playPath(const std::vector<Oper>& path) {
		for (auto& i : path) {
			opers(i);
		}
	}

	std::string pathStr, info;
	int nodeCounts{}, pathCost{};
	TetrisNode fromN, toN;
	EvalArgs evalArgs;
	std::unique_ptr<TreeContext<TreeNode>> ctx;
};

static void match(test_ai& ai1, test_ai& ai2, std::function<void(test_ai const&, test_ai const&)> out_put, size_t match_round)
{
	auto caculate_time = 100;
	size_t round = 0;
	for (; ; )
	{
		++round;

		std::vector<test_ai*> tasks{};
		tasks.emplace_back(&ai1);
		tasks.emplace_back(&ai2);
		std::for_each(std::execution::par, tasks.begin(), tasks.end(), [&caculate_time](auto ai) {ai->callBot(caculate_time); });
		ai1.playStep();
		ai2.playStep();

		if (out_put)
		{
			out_put(ai1, ai2);
		}

		if (ai1.gd.deaded || ai2.gd.deaded)
		{
			return;
		}
		if (round > match_round)
		{
			return;
		}
	}
}


double elo_init()
{
	return 1500;
}
double elo_rate(double const& self_score, double const& other_score)
{
	return 1 / (1 + std::pow(10, -(self_score - other_score) / 400));
}
double elo_get_k(int curr, int max)
{
	return 20 * (max - curr) / max + 4;
}
double elo_calc(double const& self_score, double const& other_score, double const& win, int curr, int max)
{
	return self_score + elo_get_k(curr, max) * (win - elo_rate(self_score, other_score));
}


struct BaseNode
{
	BaseNode* parent, * left, * right;
	size_t size : sizeof(size_t) * 8 - 1;
	size_t is_nil : 1;
};

struct NodeData
{
	NodeData()
	{
		score = elo_init();
		best = std::numeric_limits<double>::quiet_NaN();
		match = 0;
		gen = 0;
	}
	char name[64];
	double score;
	double best;
	uint32_t match;
	uint32_t gen;
	pso_data data;
};

struct Node : public BaseNode
{
	Node(NodeData const& d) : data(d)
	{
	}
	NodeData data;
};

struct SBTreeInterface
{
	typedef double key_t;
	typedef BaseNode node_t;
	typedef Node value_node_t;
	static key_t const& get_key(Node* node)
	{
		return *reinterpret_cast<double const*>(&node->data.score);
	}
	static bool is_nil(BaseNode* node)
	{
		return node->is_nil;
	}
	static void set_nil(BaseNode* node, bool nil)
	{
		node->is_nil = nil;
	}
	static BaseNode* get_parent(BaseNode* node)
	{
		return node->parent;
	}
	static void set_parent(BaseNode* node, BaseNode* parent)
	{
		node->parent = parent;
	}
	static BaseNode* get_left(BaseNode* node)
	{
		return node->left;
	}
	static void set_left(BaseNode* node, BaseNode* left)
	{
		node->left = left;
	}
	static BaseNode* get_right(BaseNode* node)
	{
		return node->right;
	}
	static void set_right(BaseNode* node, BaseNode* right)
	{
		node->right = right;
	}
	static size_t get_size(BaseNode* node)
	{
		return node->size;
	}
	static void set_size(BaseNode* node, size_t size)
	{
		node->size = size;
	}
	static bool predicate(key_t const& left, key_t const& right)
	{
		return left > right;
	}
};

auto piece_to_char = [](Piece type) {
	char str;
	switch (type) {
		case Piece::None:str = ' '; break;
		case Piece::O:str = 'O'; break;
		case Piece::T:str = 'T'; break;
		case Piece::I:str = 'I'; break;
		case Piece::S:str = 'S'; break;
		case Piece::Z:str = 'Z'; break;
		case Piece::L:str = 'L'; break;
		case Piece::J:str = 'J'; break;
	}
	return str;
};

auto get_hold = [](auto && ai) -> char{
	if (ai.hS.type) {
		return piece_to_char(*ai.hS.type);
	}
	else 
		return ' ';
	};

auto get_next = [](auto && ai, std::size_t i) -> char {
	return piece_to_char(ai.rS.displayBag[i]);
};

#include <memory>  // for allocator, __shared_ptr_access
#include <string>  // for char_traits, operator+, string, basic_string

#include "ftxui/component/component.hpp"       // for Input, Renderer, Vertical
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/component/loop.hpp"

ftxui::ScreenInteractive* screen_ptr;

int main(int argc, char const* argv[])
{
	std::atomic<uint32_t> count{std::max<uint32_t>(1, std::thread::hardware_concurrency() - 1)};
	std::string file = "data.bin";
	if (argc > 1)
	{
		uint32_t arg_count = std::stoul(argv[1], nullptr, 10);
		if (arg_count != 0)
		{
			count = arg_count;
		}
	}
	uint32_t node_count = count * 2;
	if (argc > 2)
	{
		uint32_t arg_count = std::stoul(argv[2], nullptr, 10);
		if (arg_count >= 2)
		{
			node_count = arg_count;
		}
	}
	std::atomic<bool> view{false};
	std::atomic<uint32_t> view_index{0};
	if (argc > 3)
	{
		file = argv[3];
	}
	std::recursive_mutex rank_table_lock;
	zzz::sb_tree<SBTreeInterface> rank_table;
	std::ifstream ifs(file, std::ios::in | std::ios::binary);
	if (ifs.good())
	{
		NodeData data;
		while (ifs.read(reinterpret_cast<char*>(&data), sizeof data).gcount() == sizeof data)
		{
			rank_table.insert(new Node(data));
		}
		ifs.close();
	}
	pso_config pso_cfg =
	{
		{}, 1, 1, 0.5, 0.01,
	};
	size_t elo_max_match = 256;
	size_t elo_min_match = 128;

	auto v = [&pso_cfg](double v, double r, double s, double b = 0)
	{
		pso_config_element d =
		{
			v - r + b, v + r + b, s
		};
		pso_cfg.config.emplace_back(d);
	};
	std::mt19937 mt;

	{
		EvalArgs p;

		v(p.board_weights.height, 100, 2);
		v(p.board_weights.holes, 100, 2);
		v(p.board_weights.cells_coveredness, 100, 2);
		v(p.board_weights.row_transition, 100, 2);
		v(p.board_weights.col_transition, 100, 2);
		v(p.board_weights.well_depth, 100, 2);
		v(p.board_weights.jeopardy, 100, 2);
		v(p.board_weights.tslot[0], 100, 2);
		v(p.board_weights.tslot[1], 100, 2);
		v(p.board_weights.tslot[2], 100, 2);
		v(p.board_weights.tslot[3], 100, 2);

		v(p.placement_weights.back_to_back, 100, 2);
		v(p.placement_weights.b2b_clear, 100, 2);
		v(p.placement_weights.clear1, 100, 2);
		v(p.placement_weights.clear2, 100, 2);
		v(p.placement_weights.clear3, 100, 2);
		v(p.placement_weights.clear4, 100, 2);
		v(p.placement_weights.tspin1, 100, 2);
		v(p.placement_weights.tspin2, 100, 2);
		v(p.placement_weights.tspin3, 100, 2);
		v(p.placement_weights.mini_tspin, 100, 2);
		v(p.placement_weights.perfect_clear, 100, 2);
		v(p.placement_weights.combo_garbage, 100, 2);
		v(p.placement_weights.wasted_t, 100, 2);
		v(p.placement_weights.soft_drop, 50, 2, -50);

		if (rank_table.empty())
		{
			NodeData init_node;

			strncpy(init_node.name, "*default", sizeof init_node.name);
			memset(&init_node.data, 0, sizeof init_node.data);
			init_node.data.param = p;
			init_node.data.p = init_node.data.x;
			rank_table.insert(new Node(init_node));
		}
	}

	while (rank_table.size() < node_count)
	{
		NodeData init_node = rank_table.at(std::uniform_int_distribution<size_t>(0, rank_table.size() - 1)(mt))->data;

		strncpy(init_node.name, ("init_" + std::to_string(rank_table.size())).c_str(), sizeof init_node.name);
		pso_logic(pso_cfg, init_node.data, init_node.data, mt);
		rank_table.insert(new Node(init_node));
	}

	std::vector<std::thread> threads;

	for (size_t i = 1; i <= count; ++i)
	{
		threads.emplace_back([&, i]()
			{
				uint32_t index = i + 1;
				auto rand_match = [&](auto& mt, size_t max)
				{
					std::pair<size_t, size_t> ret;
					ret.first = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
					do
					{
						ret.second = std::uniform_int_distribution<size_t>(0, max - 1)(mt);
					} while (ret.second == ret.first);
					return ret;
				};
				test_ai ai1, ai2;
				ai1.restart();
				ai2.restart();
				ai1.opponent = &ai2;
				ai2.opponent = &ai1;
				rank_table_lock.lock();
				rank_table_lock.unlock();
				for (; ; )
				{
					rank_table_lock.lock();
					auto m12 = rand_match(mt, rank_table.size());
					auto m1 = rank_table.at(m12.first);
					auto m2 = rank_table.at(m12.second);

					auto view_func = [m1, m2, index, &view, &view_index, &rank_table_lock](test_ai const& ai1, test_ai const& ai2)
					{
						if (view && view_index == 0)
						{
							rank_table_lock.lock();
							if (view && view_index == 0)
							{
								view_index = index;
							}
							rank_table_lock.unlock();
						}
						if (index != view_index)
						{
							return;
						}

						std::string out;
						out.reserve(2000);

						char box_0[3] = "  ", box_1[3] = "[]";

						out += string_format("HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d NAME = %s\n"
							"HOLD = %c NEXT = %c%c%c%c%c%c COMBO = %d B2B = %d UP = %2d NAME = %s\n",
							get_hold(ai1),
							get_next(ai1, 0), get_next(ai1, 1), get_next(ai1, 2), get_next(ai1, 3), get_next(ai1, 4), get_next(ai1, 5),
							ai1.gd.combo, ai1.gd.b2b, std::holds_alternative<Modes::versus>(ai1.gm) ? std::get<Modes::versus>(ai1.gm).trashLinesCount : 0, m1->data.name,

							get_hold(ai2),
							get_next(ai2, 0), get_next(ai2, 1), get_next(ai2, 2), get_next(ai2, 3), get_next(ai2, 4), get_next(ai2, 5),
							ai2.gd.combo, ai2.gd.b2b, std::holds_alternative<Modes::versus>(ai2.gm) ? std::get<Modes::versus>(ai2.gm).trashLinesCount : 0, m2->data.name);

						auto map_copy1 = ai1.map, map_copy2 = ai2.map;
						const_cast<TetrisNode&>(ai1.tn).template attach<false>(map_copy1);
						const_cast<TetrisNode&>(ai2.tn).template attach<false>(map_copy2);
						for (int y = 21; y >= 0; --y)
						{
							for (int x = 0; x < 10; ++x)
							{
								out += map_copy1(x, y) ? box_1 : box_0;
							}
								out += "  ";
							for (int x = 0; x < 10; ++x)
							{
								out += map_copy2(x, y) ? box_1 : box_0;
							}
							out += '\n';
						}

						data = std::move(out);
						screen_ptr->Post(ftxui::Event::Custom);
					};

					// size_t round_ms_min = (45 + m1->data.match) / 3;
					// size_t round_ms_max = (45 + m2->data.match) / 3;
					// if (round_ms_min > round_ms_max)
					// {
					//     std::swap(round_ms_min, round_ms_max);
					// }
					// size_t round_ms = std::uniform_int_distribution<size_t>(round_ms_min, round_ms_max)(mt);
					size_t round_ms = 20;
					size_t round_count = 3600;
					ai1.init(m1->data.data, pso_cfg, round_ms);
					ai2.init(m2->data.data, pso_cfg, round_ms);
					rank_table_lock.unlock();
					match(ai1, ai2, view_func, round_count);
					rank_table_lock.lock();

					rank_table.erase(m1);
					rank_table.erase(m2);
					bool handle_elo_1;
					bool handle_elo_2;
					if ((m1->data.match > elo_min_match) == (m2->data.match > elo_min_match))
					{
						handle_elo_1 = true;
						handle_elo_2 = true;
					}
					else
					{
						handle_elo_1 = m2->data.match > elo_min_match;
						handle_elo_2 = !handle_elo_1;
					}
					double m1s = m1->data.score;
					double m2s = m2->data.score;
					double ai1_apl = ai1.gd.clear == 0 ? 0. : 1. * ai1.gd.attack / ai1.gd.clear;
					double ai2_apl = ai2.gd.clear == 0 ? 0. : 1. * ai2.gd.attack / ai2.gd.clear;
					int ai1_win = ai2.gd.deaded * 2 + (ai1_apl > ai2_apl);
					int ai2_win = ai1.gd.deaded * 2 + (ai2_apl > ai1_apl);
					if (ai1_win == ai2_win)
					{
						if (handle_elo_1)
						{
							m1->data.score = elo_calc(m1s, m2s, 0.5, m1->data.match, elo_max_match);
						}
						if (handle_elo_2)
						{
							m2->data.score = elo_calc(m2s, m1s, 0.5, m2->data.match, elo_max_match);
						}
					}
					else if (ai1_win > ai2_win)
					{
						if (handle_elo_1)
						{
							m1->data.score = elo_calc(m1s, m2s, 1, m1->data.match, elo_max_match);
						}
						if (handle_elo_2)
						{
							m2->data.score = elo_calc(m2s, m1s, 0, m2->data.match, elo_max_match);
						}
					}
					else
					{
						if (handle_elo_1)
						{
							m1->data.score = elo_calc(m1s, m2s, 0, m1->data.match, elo_max_match);
						}
						if (handle_elo_2)
						{
							m2->data.score = elo_calc(m2s, m1s, 1, m2->data.match, elo_max_match);
						}
					}
					m1->data.match += handle_elo_1;
					m2->data.match += handle_elo_2;
					rank_table.insert(m1);
					rank_table.insert(m2);

					auto do_pso_logic = [&](Node* node)
					{
						NodeData* data = &node->data;
						if (std::isnan(data->best) || data->score > data->best)
						{
							data->best = data->score;
							data->data.p = data->data.x;
						}
						else
						{
							data->best = data->best * 0.95 + data->score * 0.05;
						}
						data->match = 0;
						++data->gen;
						rank_table.erase(node);
						data->score = elo_init();
						rank_table.insert(node);
						if (node->data.name[0] == '*' || node->data.name[0] == '-')
						{
							return;
						}
						double best;
						pso_data* best_data = nullptr;
						for (auto it = rank_table.begin(); it != rank_table.end(); ++it)
						{
							if (it->data.name[0] == '*' || std::isnan(it->data.best))
							{
								continue;
							}
							if (best_data == nullptr || it->data.best > best)
							{
								best = it->data.best;
								best_data = &it->data.data;
							}
						}
						pso_logic(pso_cfg, best_data != nullptr ? *best_data : data->data, data->data, mt);
					};
					if (m1->data.match >= elo_max_match)
					{
						do_pso_logic(m1);
					}
					if (m2->data.match >= elo_max_match)
					{
						do_pso_logic(m2);
					}
					rank_table_lock.unlock();
				}
			});
	}
	Node* edit = nullptr;
	auto print_config = [&rank_table, &rank_table_lock](Node* node)
	{
		rank_table_lock.lock();
		data = string_format(
			"[99]name         = %s\n"
			"[  ]rank         = %d\n"
			"[  ]score        = %f\n"
			"[  ]best         = %f\n"
			"[  ]match        = %d\n"
			"[ 0]height         = %8.3f, %8.3f, %8.3f\n"
			"[ 1]holes         = %8.3f, %8.3f, %8.3f\n"
			"[ 2]cells_coveredness    = %8.3f, %8.3f, %8.3f\n"
			"[ 3]row_transition    = %8.3f, %8.3f, %8.3f\n"
			"[ 4]col_transition   = %8.3f, %8.3f, %8.3f\n"
			"[ 5]well_depth    = %8.3f, %8.3f, %8.3f\n"
			"[ 6]jeopardy  = %8.3f, %8.3f, %8.3f\n"
			"[ 7]tslot_0       = %8.3f, %8.3f, %8.3f\n"
			"[ 8]tslot_1       = %8.3f, %8.3f, %8.3f\n"
			"[ 9]tslot_2       = %8.3f, %8.3f, %8.3f\n"
			"[10]tslot_3         = %8.3f, %8.3f, %8.3f\n"
			"[11]back_to_back         = %8.3f, %8.3f, %8.3f\n"
			"[12]b2b_clear       = %8.3f, %8.3f, %8.3f\n"
			"[13]clear1       = %8.3f, %8.3f, %8.3f\n"
			"[14]clear2       = %8.3f, %8.3f, %8.3f\n"
			"[15]clear3      = %8.3f, %8.3f, %8.3f\n"
			"[16]clear4      = %8.3f, %8.3f, %8.3f\n"
			"[17]tspin1      = %8.3f, %8.3f, %8.3f\n"
			"[18]tspin2      = %8.3f, %8.3f, %8.3f\n"
			"[19]tspin3      = %8.3f, %8.3f, %8.3f\n"
			"[20]mini_tspin      = %8.3f, %8.3f, %8.3f\n"
			"[21]perfect_clear      = %8.3f, %8.3f, %8.3f\n"
			"[22]combo_garbage      = %8.3f, %8.3f, %8.3f\n"
			"[23]wasted_t      = %8.3f, %8.3f, %8.3f\n"
			"[24]soft_drop   = %8.3f, %8.3f, %8.3f\n"
			, node->data.name
			, rank_table.rank(double(node->data.score))
			, node->data.score
			, node->data.best
			, node->data.match
			, node->data.data.x[0], node->data.data.p[0], node->data.data.v[0]
			, node->data.data.x[1], node->data.data.p[1], node->data.data.v[1]
			, node->data.data.x[2], node->data.data.p[2], node->data.data.v[2]
			, node->data.data.x[3], node->data.data.p[3], node->data.data.v[3]
			, node->data.data.x[4], node->data.data.p[4], node->data.data.v[4]
			, node->data.data.x[5], node->data.data.p[5], node->data.data.v[5]
			, node->data.data.x[6], node->data.data.p[6], node->data.data.v[6]
			, node->data.data.x[7], node->data.data.p[7], node->data.data.v[7]
			, node->data.data.x[8], node->data.data.p[8], node->data.data.v[8]
			, node->data.data.x[9], node->data.data.p[9], node->data.data.v[9]
			, node->data.data.x[10], node->data.data.p[10], node->data.data.v[10]
			, node->data.data.x[11], node->data.data.p[11], node->data.data.v[11]
			, node->data.data.x[12], node->data.data.p[12], node->data.data.v[12]
			, node->data.data.x[13], node->data.data.p[13], node->data.data.v[13]
			, node->data.data.x[14], node->data.data.p[14], node->data.data.v[14]
			, node->data.data.x[15], node->data.data.p[15], node->data.data.v[15]
			, node->data.data.x[16], node->data.data.p[16], node->data.data.v[16]
			, node->data.data.x[17], node->data.data.p[17], node->data.data.v[17]
			, node->data.data.x[18], node->data.data.p[18], node->data.data.v[18]
			, node->data.data.x[19], node->data.data.p[19], node->data.data.v[19]
			, node->data.data.x[20], node->data.data.p[20], node->data.data.v[20]
			, node->data.data.x[21], node->data.data.p[21], node->data.data.v[21]
			, node->data.data.x[22], node->data.data.p[22], node->data.data.v[22]
			, node->data.data.x[23], node->data.data.p[23], node->data.data.v[23]
			, node->data.data.x[24], node->data.data.p[24], node->data.data.v[24]
		);
		rank_table_lock.unlock();
	};

	std::map<std::string, std::function<bool(std::vector<std::string> const&)>> command_map;
	command_map.insert(std::make_pair("select", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			if (token.size() == 2)
			{
				size_t index = std::atoi(token[1].c_str()) - 1;
				if (index < rank_table.size())
				{
					rank_table_lock.lock();
					edit = rank_table.at(index);
					print_config(edit);
					rank_table_lock.unlock();
				}
			}
			else if (token.size() == 1 && edit != nullptr)
			{
				rank_table_lock.lock();
				print_config(edit);
				rank_table_lock.unlock();
			}
			return true;
		}));
	command_map.insert(std::make_pair("set", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			if (token.size() >= 3 && token.size() <= 5 && edit != nullptr)
			{
				size_t index = std::atoi(token[1].c_str());
				rank_table_lock.lock();
				if (index == 99 && token[2].size() < 64)
				{
					memcpy(edit->data.name, token[2].c_str(), token[2].size() + 1);
				}
				else if (index < sizeof(EvalArgs) / sizeof(double))
				{
					edit->data.data.x[index] = std::atof(token[2].c_str());
					if (token.size() >= 4)
					{
						edit->data.data.p[index] = std::atof(token[3].c_str());
					}
					if (token.size() >= 5)
					{
						edit->data.data.v[index] = std::atof(token[4].c_str());
					}
				}
				print_config(edit);
				rank_table_lock.unlock();
			}
			return true;
		}));
	command_map.insert(std::make_pair("copy", [&edit, &print_config, &rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			if (token.size() == 2 && token[1].size() < 64 && edit != nullptr)
			{
				rank_table_lock.lock();
				NodeData data = edit->data;
				memcpy(data.name, token[1].c_str(), token[1].size() + 1);
				data.match = 0;
				data.score = elo_init();
				Node* node = new Node(data);
				rank_table.insert(node);
				print_config(node);
				edit = node;
				rank_table_lock.unlock();
			}
			return true;
		}));
	command_map.insert(std::make_pair("rank", [&rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			rank_table_lock.lock();
			size_t begin = 0, end = rank_table.size();
			if (token.size() == 2)
			{
				begin = std::atoi(token[1].c_str()) - 1;
				end = begin + 1;
			}
			if (token.size() == 3)
			{
				begin = std::atoi(token[1].c_str()) - 1;
				end = begin + std::atoi(token[2].c_str());
			}
			data = "";
			for (size_t i = begin; i < end && i < rank_table.size(); ++i)
			{
				auto node = rank_table.at(i);
				data += string_format("rank = %3d elo = %4.1f best = %4.1f match = %3zd gen = %5zd name = %s\n", i + 1, node->data.score, node->data.best, node->data.match, node->data.gen, node->data.name);
			}
			rank_table_lock.unlock();
			return true;
		}));
	command_map.insert(std::make_pair("view", [&view](std::vector<std::string> const& token)
		{
			view = true;
			return true;
		}));
	command_map.insert(std::make_pair("best", [&rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			if (token.size() != 2)
			{
				return true;
			}
			rank_table_lock.lock();
			double best;
			pso_data* node = nullptr;
			for (auto it = rank_table.begin(); it != rank_table.end(); ++it)
			{
				if (std::isnan(it->data.best))
				{
					continue;
				}
				if (node == nullptr || it->data.best > best)
				{
					best = it->data.best;
					node = &it->data.data;
				}
			}
			if (node == nullptr)
			{
				node = &rank_table.front()->data.data;
			}
			if (token[1] == "bat")
			{
				data = string_format(
					"SET height=%.9f\n"
					"SET holes=%.9f\n"
					"SET cells_coveredness=%.9f\n"
					"SET row_transition=%.9f\n"
					"SET col_transition=%.9f\n"
					"SET well_depth=%.9f\n"
					"SET jeopardy=%.9f\n"
					"SET tslot_0=%.9f\n"
					"SET tslot_1=%.9f\n"
					"SET tslot_2=%.9f\n"
					"SET tslot_3=%.9f\n"
					"SET back_to_back=%.9f\n"
					"SET b2b_clear=%.9f\n"
					"SET clear1=%.9f\n"
					"SET clear2=%.9f\n"
					"SET clear3=%.9f\n"
					"SET clear4=%.9f\n"
					"SET tspin1=%.9f\n"
					"SET tspin2=%.9f\n"
					"SET tspin3=%.9f\n"
					"SET mini_tspin=%.9f\n"
					"SET perfect_clear=%.9f\n"
					"SET combo_garbage=%.9f\n"
					"SET wasted_t=%.9f\n"
					"SET soft_drop=%.9f\n"
					, node->p[0]
					, node->p[1]
					, node->p[2]
					, node->p[3]
					, node->p[4]
					, node->p[5]
					, node->p[6]
					, node->p[7]
					, node->p[8]
					, node->p[9]
					, node->p[10]
					, node->p[11]
					, node->p[12]
					, node->p[13]
					, node->p[14]
					, node->p[15]
					, node->p[16]
					, node->p[17]
					, node->p[18]
					, node->p[19]
					, node->p[20]
					, node->p[21]
					, node->p[22]
					, node->p[23]
					, node->p[24]
				);
			}
			else if (token[1] == "cpp")
			{
				data = string_format(
					"{%.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f,"
					" %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f, %.9f,"
					" %.9f, %.9f, %.9f, %.9f, %.9f}\n"
					, node->p[0]
					, node->p[1]
					, node->p[2]
					, node->p[3]
					, node->p[4]
					, node->p[5]
					, node->p[6]
					, node->p[7]
					, node->p[8]
					, node->p[9]
					, node->p[10]
					, node->p[11]
					, node->p[12]
					, node->p[13]
					, node->p[14]
					, node->p[15]
					, node->p[16]
					, node->p[17]
					, node->p[18]
					, node->p[19]
					, node->p[20]
					, node->p[21]
					, node->p[22]
					, node->p[23]
					, node->p[24]
				);
			}
			rank_table_lock.unlock();
			return true;
		}));
	command_map.insert(std::make_pair("save", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			rank_table_lock.lock();
			std::ofstream ofs(file, std::ios::out | std::ios::binary);
			for (size_t i = 0; i < rank_table.size(); ++i)
			{
				ofs.write(reinterpret_cast<char const*>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
			}
			ofs.flush();
			ofs.close();
			data = string_format("%d node(s) saved\n", rank_table.size());
			rank_table_lock.unlock();
			return true;
		}));
	command_map.insert(std::make_pair("exit", [&file, &rank_table, &rank_table_lock](std::vector<std::string> const& token)
		{
			rank_table_lock.lock();
			std::ofstream ofs(file, std::ios::out | std::ios::binary);
			for (size_t i = 0; i < rank_table.size(); ++i)
			{
				ofs.write(reinterpret_cast<char const*>(&rank_table.at(i)->data), sizeof rank_table.at(i)->data);
			}
			ofs.flush();
			ofs.close();
			rank_table_lock.unlock();
			exit(0);
			return true;
		}));
	command_map.insert(std::make_pair("help", [](std::vector<std::string> const& token)
		{
			data = string_format(
				"help                 - ...\n"
				"view                 - view a match (press enter to stop)\n"
				"rank                 - show all nodes\n"
				"best                 - print current best\n"
				"rank [rank]          - show a node at rank\n"
				"rank [rank] [length] - show nodes at rank\n"
				"select [rank]        - select a node and view info\n"
				"set [index] [value]  - set node name or config which last selected\n"
				"copy [name]          - copy a new node which last selected\n"
				"save                 - ...\n"
				"exit                 - save & exit\n"
			);
			return true;
		}));
	std::string line, last;

	auto main_loop = [&](std::string& line) {
		std::vector<std::string> token;
		zzz::split(token, line, " ");
		if (view)
		{
			view = false;
			view_index = 0;
			//return;
		}
		if (token.empty())
		{
			line = last;
			zzz::split(token, line, " ");
		}
		if (token.empty())
		{
			return;
		}
		auto find = command_map.find(token.front());
		if (find == command_map.end())
		{
			return;
		}
		if (find->second(token))
		{
			last = line;
		}
	};

	std::atomic<bool> refresh_ui_continue = true;
	auto screen = ftxui::ScreenInteractive::Fullscreen();
	screen_ptr = &screen;
	std::string command, input_entries;
	auto input_option = ftxui::InputOption();
	std::string input_add_content;
	input_option.on_enter = [&] {
		input_entries = command;
		if (input_entries.back() == '\n') {
			input_entries.pop_back();
		}
		//data += input_entries;
		main_loop(input_entries);
		command = "";
	};
	auto input_command = ftxui::Input(&command, "command", input_option);

	auto main_render = ftxui::Renderer([&]() {
		auto& s = data;
		ftxui::Elements lines;
		std::size_t start = 0U, end;
		while ((end = s.find('\n', start)) != std::string::npos) {
			lines.push_back(ftxui::hflow(ftxui::paragraph(s.substr(start, end - start))));
			start = end + 1;
		}
		lines.push_back(ftxui::hflow(ftxui::paragraph(s.substr(start))));
		return ftxui::vbox(std::move(lines));
		});

	/*std::thread refresh_ui([&] {
		while (refresh_ui_continue) {
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(0.05s);
			//data = "hello\nword\n";
			screen.Post(ftxui::Event::Custom);
		}
	});*/

	auto component2 = ftxui::Container::Vertical({ input_command, main_render });
	auto renderer = ftxui::Renderer(component2, [&] {
		return ftxui::vbox({
			main_render->Render() | ftxui::flex,
			ftxui::separatorCharacter(""),
			input_command->Render() | ftxui::focus
			}) | ftxui::flex;
		});


	/*ftxui::Loop loop(&screen, renderer);
	while (!loop.HasQuitted()) {
		using namespace std::chrono_literals;
		screen.RequestAnimationFrame();//Post(ftxui::Event::Custom);
		loop.RunOnce();
		//std::this_thread::sleep_for(0.05s);
	}*/
	screen.Loop(renderer);

	//refresh_ui_continue = false;
	//refresh_ui.join();

	/*while (true)
	{
		std::getline(std::cin, line);
		if (view)
		{
			view = false;
			view_index = 0;
			continue;
		}
		std::vector<std::string> token;
		zzz::split(token, line, " ");
		if (token.empty())
		{
			line = last;
			zzz::split(token, line, " ");
		}
		if (token.empty())
		{
			continue;
		}
		auto find = command_map.find(token.front());
		if (find == command_map.end())
		{
			continue;
		}
		if (find->second(token))
		{
			printf("-------------------------------------------------------------\n");
			last = line;
			std::cout.flush();
		}
	}*/
}
