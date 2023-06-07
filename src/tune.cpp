#include <iostream>
#include <map>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "timer.hpp"
#include "tool.h"
#include "threadPool.h"
#include "json.hpp"
#include <fstream>
#include <filesystem>

class Sprt
{
public:
	enum SPRT_RESULT
	{
		SPRT_H0,
		SPRT_H1,
		SPRT_NULL
	};

	static double elo_diff_to_win_rate(double elo_diff) {
		return 1.0 / (1.0 + pow(10.0, static_cast<double>(-elo_diff) / 400.0));
	}

	static double win_rate_to_elo_diff(double win_rate) {
		if (win_rate == 0.0) {
			return 0.0;
		}
		return -400.0 * log10(1.0 / win_rate - 1.0);
	}

	static double win_rate_to_elo_error_margin(double win, double draw, double loss) {
		double total = win + draw + loss;
		double win_rate = (win + draw / 2.0) / total;

		double win_p = win / total;
		double draw_p = draw / total;
		double loss_p = loss / total;

		double win_dev = win_p * pow(1.0 - win_rate, 2);
		double draw_dev = draw_p * pow(0.5 - win_rate, 2);
		double loss_dev = loss_p * pow(0.0 - win_rate, 2);
		double std_dev = sqrt(win_dev + draw_dev + loss_dev) / sqrt(total);

		double confidence_p = 0.95;
		double min_confidence_p = (1.0 - confidence_p) / 2.0;
		double max_confidence_p = 1.0 - min_confidence_p;
		double min_dev = win_rate + Sprt::phi_inverse(min_confidence_p) * std_dev;
		double max_dev = win_rate + Sprt::phi_inverse(max_confidence_p) * std_dev;

		double difference = Sprt::win_rate_to_elo_diff(max_dev) - Sprt::win_rate_to_elo_diff(min_dev);

		return difference / 2.0;
	}

	static double log_likelihood_ratio_approximate(double win, double draw, double loss, double elo_diff_0, double elo_diff_1) {
		if (win == 0 || loss == 0) {
			return 0.0;
		}

		double match_count = win + draw + loss;

		double win_rate = (win + draw / 2.0) / match_count;

		double win_rate_doubleiance = ((win + draw / 4.0) / match_count - win_rate * win_rate) / match_count;

		double win_rate_0 = elo_diff_to_win_rate(elo_diff_0);
		double win_rate_1 = elo_diff_to_win_rate(elo_diff_1);

		return (win_rate_1 - win_rate_0) * (2 * win_rate - win_rate_0 - win_rate_1) / win_rate_doubleiance / 2.0;
	}

	static double phi_inverse(double x) {
		return sqrt(2.0) * Sprt::inverse_error(2.0 * x - 1.0);
	}

	static double inverse_error(double x) {

		double pi = 3.141592654;
		double a = 8.0 * (pi - 3.0) / (3.0 * pi * (4.0 - pi));
		double y = log(1.0 - x * x);
		double z = 2.0 / (pi * a) + y / 2.0;

		double result = sqrt(sqrt(z * z - y / a) - z);

		if (x < 0) {
			return -result;
		}

		return result;
	}

	static SPRT_RESULT sprt(double win, double draw, double loss, double elo_diff_0, double elo_diff_1, double false_positive_rate, double false_negative_rate) {
		double log_likelihood_ratio = Sprt::log_likelihood_ratio_approximate(win, draw, loss, elo_diff_0, elo_diff_1);

		double lower_bound = log(false_negative_rate / (1.0 - false_positive_rate));
		double upper_bound = log((1.0 - false_negative_rate) / false_positive_rate);

		if (log_likelihood_ratio >= upper_bound) {
			return SPRT_H1;
		}
		else if (log_likelihood_ratio <= lower_bound) {
			return SPRT_H0;
		}
		else {
			return SPRT_NULL;
		}
	}
};

struct EvalArgs {
	using Evaluator = Evaluator<TreeContext<TreeNode>>;
	Evaluator::BoardWeights board_weights = Evaluator::BoardWeights::default_args();
	Evaluator::PlacementWeights placement_weights = Evaluator::PlacementWeights::default_args();;
};


class Spsa
{
#define VARY_WEIGHT_PARAMETER(p_name, delta) vary_value(base.p_name, v1.p_name, v2.p_name, delta);
#define VARY_WEIGHT_STATIC_PARAMETER(p_name, delta) vary_value_static(base.p_name, v1.p_name, v2.p_name, delta);
#define APPROACH_WEIGHT_PARAMETER(p_name, ap_v) approach_value(base.p_name, v.p_name, ap_v);

public:
	Spsa() {
		this->generator = std::default_random_engine((unsigned int)std::chrono::steady_clock::now().time_since_epoch().count());
	}

private:
	std::default_random_engine generator;

public:
	int random(int delta) {
		std::normal_distribution<double> distribution(0.0, (double)delta / 2.0);

		double number = distribution(this->generator);
		return int(number);
	}

	void vary_value(int& base, int& v1, int& v2, int delta) {
		int r_value = Spsa::random(delta);
		v1 = base + r_value;
		v2 = base - r_value;
	}

	void vary_value_static(int& base, int& v1, int& v2, int delta) {
		int sign = (rand() % 2) * 2 - 1;
		v1 = base + delta * sign;
		v2 = base - delta * sign;
	}

	void vary_weight(EvalArgs& base, EvalArgs& v1, EvalArgs& v2) {
		// Set init
		v1 = base;
		v2 = base;

		// Defence
		VARY_WEIGHT_STATIC_PARAMETER(board_weights.height, 10 + (rand() % 6));

		VARY_WEIGHT_STATIC_PARAMETER(board_weights.holes, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(board_weights.cells_coveredness, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(board_weights.row_transition, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(board_weights.col_transition, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(board_weights.well_depth, 10 + (rand() % 21));

		for (int i = 0; i < 4; ++i) {
			VARY_WEIGHT_STATIC_PARAMETER(board_weights.tslot[i], 20 + (rand() % 21));
		}

		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.back_to_back, 20 + (rand() % 6));

		// Attack
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.b2b_clear, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.clear1, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.clear2, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.clear3, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.clear4, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.tspin1, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.tspin2, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.tspin3, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.mini_tspin, 10 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.perfect_clear, 25 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.combo_garbage, 20 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.wasted_t, 20 + (rand() % 21));
		VARY_WEIGHT_STATIC_PARAMETER(placement_weights.soft_drop, 20 + (rand() % 11));
	}

	void approach_value(int& base, int& v, double ap_v) {
		double delta_raw = double(v) - double(base);
		double delta = delta_raw * ap_v;
		base += int(std::round(delta));
	}

	void approach_weight(EvalArgs& base, EvalArgs& v) {
		// Setting apply factor
		double ap_v = 0.1;

		// Defence
		APPROACH_WEIGHT_PARAMETER(board_weights.height, ap_v);

		APPROACH_WEIGHT_PARAMETER(board_weights.holes, ap_v);
		APPROACH_WEIGHT_PARAMETER(board_weights.cells_coveredness, ap_v);
		APPROACH_WEIGHT_PARAMETER(board_weights.row_transition, ap_v);
		APPROACH_WEIGHT_PARAMETER(board_weights.col_transition, ap_v);
		APPROACH_WEIGHT_PARAMETER(board_weights.well_depth, ap_v);

		for (int i = 0; i < 4; ++i) {
			APPROACH_WEIGHT_PARAMETER(board_weights.tslot[i], ap_v);
		}

		APPROACH_WEIGHT_PARAMETER(placement_weights.back_to_back, ap_v);

		// Attack
		APPROACH_WEIGHT_PARAMETER(placement_weights.b2b_clear, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.clear1, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.clear2, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.clear3, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.clear4, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.tspin1, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.tspin2, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.tspin3, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.mini_tspin, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.perfect_clear, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.combo_garbage, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.wasted_t, ap_v);
		APPROACH_WEIGHT_PARAMETER(placement_weights.soft_drop, ap_v);
	}

#undef VARY_WEIGHT_PARAMETER
#undef VARY_WEIGHT_STATIC_PARAMETER
#undef APPROACH_WEIGHT_PARAMETER
};


struct SaveData
{
    EvalArgs base;
	EvalArgs v1;
	EvalArgs v2;
	EvalArgs next;

	int win_v1 = 0;
	int win_v2 = 0;
	int total = 0;
};

using BoardWeights = EvalArgs::Evaluator::BoardWeights;
using PlacementWeights = EvalArgs::Evaluator::PlacementWeights;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BoardWeights,
	height,
	holes,
	cells_coveredness,
	row_transition,
	col_transition,
	well_depth,
	tslot[0],
	tslot[1],
	tslot[2],
	tslot[3]
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlacementWeights,
	back_to_back,
	b2b_clear,
	clear1,
	clear2,
	clear3,
	clear4,
	tspin1,
	tspin2,
	tspin3,
	mini_tspin,
	perfect_clear,
	combo_garbage,
	wasted_t,
	soft_drop
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EvalArgs, board_weights, placement_weights)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SaveData, base, v1, v2, next, win_v1, win_v2, total);


constexpr int COMPARE_MAX_BATTLE_FRAME = 2000;
constexpr int COMPARE_MAX_BATTLE = 10;// 500;

template<class Battle>
class Compare
{
	std::mutex mutex;
public:
	void save_json(SaveData& save_data, int gen_id) {
		std::string gen_name = std::string("gen/gen_") + std::to_string(gen_id) + std::string(".json");
		std::ofstream o(gen_name, std::ofstream::out | std::ofstream::trunc);
		nlohmann::json js;
		to_json(js, save_data);
		o << std::setw(4) << js << std::endl;
		o.close();
	}

	void load_json(SaveData& save_data, int gen_id){
		std::string gen_name = std::string("gen/gen_") + std::to_string(gen_id) + std::string(".json");
		std::ifstream file;
		file.open(gen_name);
		nlohmann::json js;
		file >> js;
		file.close();
		from_json(js, save_data);
	}

public:
	SaveData data;
public:
	void start(EvalArgs base, EvalArgs w1, EvalArgs w2, int total, int gen_id, int thread) {
		// Thread
		int thread_count = std::max(0, thread - 1);
		std::vector<std::thread*> threads;
		for (int i = 0; i < thread_count; ++i) {
			threads.push_back(nullptr);
		}

		// Data
		this->data.base = base;
		this->data.v1 = w1;
		this->data.v2 = w2;
		save_json(this->data, gen_id);

		// Func
		auto func = [&](int total_cnt, int gen_cnt) {
			// Set id
			int id = gen_cnt;

			// Init battle
			Battle battle;
			EvalArgs w1, w2;
			{
				std::unique_lock<std::mutex> lk(mutex);
				w1 = this->data.v1;
				w2 = this->data.v2;
			}

			// Main loop
			while (true)
			{
				std::cout << "\033[2J\033[1;1H";

				mutex.lock();
				double win = (double)this->data.win_v1;
				double loss = (double)this->data.win_v2;
				double draw = (double)(this->data.total - this->data.win_v1 - this->data.win_v2);
				mutex.unlock();

				// Draw to console prettily
				auto showStatus = [&]() {
					std::cout << std::fixed << std::setprecision(2);
					std::cout << "\nGEN[" << id;
					std::cout << "]\tPROGRESS: " << (double(this->data.total) / double(total_cnt) * 100.0) << "%\n";
					std::cout << "------------------------------------------\n";
					std::cout << "ELO: "
						<< Sprt::win_rate_to_elo_diff((win + draw / 2.0) / double(this->data.total))
						<< " +- " << Sprt::win_rate_to_elo_error_margin(win, draw, loss)
						<< " (95%)\n";
					std::cout << "LLR: "
						<< Sprt::log_likelihood_ratio_approximate(win, draw, loss, -5.0, 5.0)
						<< " (-1.09, 1.09) [-5, 5]\n";
					std::cout << "Total: " << this->data.total
						<< " W: " << this->data.win_v1
						<< " D: " << int(draw)
						<< " L: " << this->data.win_v2
						<< "\n";
				};

				// If sprt ok or reach max number of match, break
				if (Sprt::sprt(win, draw, loss, -5.0, 5.0, 0.25, 0.25) != Sprt::SPRT_NULL) {
					save_json(this->data, id);
					break;
				}
				if (this->data.total >= total_cnt) {
					save_json(this->data, id);
					break;
				}

				// Init battle
				battle.init();
				battle.player1.evalArgs = w1;
				battle.player2.evalArgs = w2;

				// Update battle
				bool draw_x = false;
				int frame = 0;

				//battle.match(showStatus);

				while (!battle.is_gameover()) {
					if (frame > COMPARE_MAX_BATTLE_FRAME) {
						draw_x = true;
						break;
					}
					++frame;
					battle.update(showStatus);
				}

				// End
				int winner_id = -1;
				if (!draw_x) 
					winner_id = battle.get_winner();
				else {
					winner_id = battle.get_winner_favor();
					draw_x = false;
				}
				{
					std::unique_lock<std::mutex> lk(mutex);
					this->data.total++;
					if (!draw_x) {
						if (winner_id == 1) this->data.win_v1++;
						if (winner_id == 2) this->data.win_v2++;
					}
					if (this->data.total % 10 == 0) {
						save_json(this->data, id);
					}
				}
			}
		};

		// Start sub threads
		for (int i = 0; i < thread_count; ++i) {
			threads[i] = new std::thread(func, total, gen_id);
			std::cout << "Thread #" << (i + 1) << " started!" << std::endl;
		}

		// Start main thread
		std::cout << "Thread #0 started!" << std::endl;
		func(total, gen_id);
		std::cout << "Thread #0 ended!" << std::endl;

		// Wait sub threads end
		for (int i = 0; i < thread_count; ++i) {
			threads[i]->join();
			delete threads[i];
			threads[i] = nullptr;
			std::cout << "Thread #" << (i + 1) << " ended!" << std::endl;
		}
	}
};

class Battle;
class MyTetris : public TetrisGame {
	friend class Battle;
public:

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
	};

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
		auto init_context = [&](auto && context) {
			context->createRoot(start, map, dp, hS.type, hS.able, !!gd.b2b, gd.combo, dySpawn);
		};
		init_context(ctx);
		using std::chrono::high_resolution_clock;
		using std::chrono::milliseconds;
		auto end = high_resolution_clock::now() + milliseconds(limitTime);
		auto taskrun = [&](auto && context) {
			do {
				context->run();
			} while (high_resolution_clock::now() < end);
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

class Battle {
public:
	void init() {
		std::cout << "\033[2J\033[1;1H";
		player1.restart();
		player2.restart();
		player1.opponent = &player2;
		player2.opponent = &player1;
	}

	void render() {
		std::stringstream os[2];
		player1.drawField_with(os[0]);
		player2.drawField_with(os[1]);
		std::cout << "\033[1;1H";
		for (std::string line; std::getline(os[0], line);) {
			std::cout << line;
			std::getline(os[1], line);
			std::cout << "\t" << line << "\n";
		}
	}

	template<class F>
	void update(F && f) {
		static ThreadPool threadPool{ 2 };
		std::vector<std::future<void>> futures;
		futures.emplace_back(threadPool.commit([&]() {player1.callBot(caculate_time); }));
		futures.emplace_back(threadPool.commit([&]() {player2.callBot(caculate_time); }));
		for (auto& run : futures)
			run.wait();
		player1.playStep();
		player2.playStep();
		render();
		f();
	}

	int get_winner() {
		return 1 * (!player1.gd.deaded) + 2 * (!player2.gd.deaded);
	}

	int get_winner_favor() {
		if (static_cast<double>(player1.gd.attack) / player1.gd.pieces >
			static_cast<double>(player2.gd.attack) / player2.gd.pieces) {
			return 1;
		}
		else
			return 2;
	}

	bool is_gameover() {
		return player1.gd.deaded || player2.gd.deaded;
	}

	MyTetris player1, player2;
	int caculate_time = 100;
};

void run(int mode) {
	int starting_batch = 0;
	Spsa rng;
	Compare<Battle> compare;
	if (mode == 1) {
		compare.data = SaveData{};
		compare.data.base = EvalArgs{};
		rng.vary_weight(compare.data.base, compare.data.v1, compare.data.v2);
		compare.save_json(compare.data, 0);
	}
	else if (mode == 2) {
		std::cout << "\033[2J\033[1;1H";
		std::cout << "What batch?" << std::endl;
		std::cin >> starting_batch;
		compare.load_json(compare.data, starting_batch);
	}

	int thread_cnt = 1;
	/*std::cout << "\033[2J\033[1;1H";
	std::cout << "How many threads?" << std::endl;
	std::cin >> thread_cnt;
	std::cout << "\033[2J\033[1;1H";*/

	// Main loop
	while (true)
	{
		// Compare weights
		compare.start(compare.data.base, compare.data.v1, compare.data.v2, COMPARE_MAX_BATTLE, starting_batch, thread_cnt);
		// Set new weight
		EvalArgs winner;
		if (compare.data.win_v1 >= compare.data.win_v2) {
			winner = compare.data.v1;
		}
		else {
			winner = compare.data.v2;
		}
		compare.data.next = compare.data.base;
		rng.approach_weight(compare.data.next, winner);
		compare.save_json(compare.data, starting_batch);
		// Restart data
		++starting_batch;
		compare.data.base = compare.data.next;
		compare.data.next = EvalArgs();
		rng.vary_weight(compare.data.base, compare.data.v1, compare.data.v2);
		compare.data.total = 0;
		compare.data.win_v1 = 0;
		compare.data.win_v2 = 0;
		compare.save_json(compare.data, starting_batch);
	}
}

int main()
{
	std::filesystem::create_directory("gen");
	int select{};
	while(true)
	{
	  std::cout << "1. restart train \n2. train specific batch\n";
	  std::cin >> select;
	  if(select == 1 || select == 2)
          break;
	}
	run(select);
}
