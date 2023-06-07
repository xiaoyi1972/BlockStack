#pragma once
#include"tetrisBase.h"
#include"tool.h"
#include"pdqsort.h"
#include<random>
#include<algorithm>
#include<functional>
#include<mutex>
#include<thread>
#include<atomic>
#include<shared_mutex>
#include<climits>

struct Search {
	template<std::size_t index = 0, class Container = trivail_vector<TetrisNode>>
	static Container search(TetrisNode& first, TetrisMap& map) {
		if constexpr (index == 0) {
			if (first.type == Piece::T)  return Search::search<1, Container>(first, map);
			else if (first.type == Piece::I || first.type == Piece::Z || first.type == Piece::S)
				return  Search::search<2, Container>(first, map);
		}

		auto softDrop_disable = true, fixed = false, fastMode = true;
		Container queue{}, result{};
		filterLookUp<TetrisNodeMoveEx> checked(map);
		std::size_t amount = 0, cals = 0;

		if (std::all_of(map.top, map.top + map.width, [&map, &first](int n) { return first.y - n > 0; })) {
			fastMode = true;
			for (auto& node : first.getHoriZontalNodes(map, queue)) {
				auto& moves = node.moves;
				auto drs = std::abs(node.rs - first.rs), dx = std::abs(node.x - first.x);
				drs -= (drs + 1 >> 2 << 1);
				moves = 1 + dx + drs;
				node.y -= node.step = node.getDrop(map);
				checked.set(node, {cals, moves, false});
				++cals;
			}
			amount = cals;
		}
		else {
			fastMode = false;
			queue.emplace_back(first);
			checked.set(first, {0, 0, false});
			amount = 1;
		}

		auto updatePosition = [&](TetrisNode& target, int moves, bool lastRotate) {
			if constexpr (index == 1) {
				if (lastRotate == true) {
					if (auto it = checked.get(target)) {
						queue[it->index].lastRotate = true;
					}
				}
			}
			if (fastMode && target.open(map)) return;
			if (!checked.contain(target)) {
				target.lastRotate = lastRotate;
				checked.set(target, { amount, moves, false });
				queue.emplace_back(target.type, target.x, target.y, target.rs);
				queue.back().moves = moves, queue.back().lastRotate = target.lastRotate, queue.back().step = target.step;
				++amount;
			}
		};

		for (std::size_t i = 0; i != amount; ++i) {
			auto cur = queue[i];
			auto& moves = cur.moves;
			if (!cur.check(map, cur.x, cur.y - 1)) {
				fixed = fastMode ? i < cals : cur.open(map);
				if constexpr (index == 2) {//Piece I Z S
					auto value = checked.get<false>(cur);
					if (value == nullptr) 
						value = checked.get<true>(cur);
					if (!value->locked) {
						result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
						result.back().step = fixed ? 0 : cur.step;
						checked.set<false>(cur, { result.size() - 1, moves, true });
					}
					else {
						auto& [seq, step, locked] = *value;
						if (cur.rs == first.rs || step > moves) {
							result[seq] = cur;
							result[seq].step = fixed ? 0 : cur.step;
						}
					}
				}
				else {
					result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
					if constexpr (index == 1) { //Piece T
					    result.back().template corner3<true>(map);
						result.back().lastRotate = cur.lastRotate;
					}
					result.back().step = fixed ? 0 : cur.step;
				}
			}
			else {
				if (auto next = TetrisNode::shift(cur, map, 0, -cur.getDrop(map))) { // Softdrop to bottom
					next->step += cur.step + cur.y - next->y;
					updatePosition(*next, moves + 1, false);
				}
				if (!softDrop_disable)
					if (auto next = TetrisNode::shift(cur, map, 0, -1)) { // Softdrop
						next->step += cur.step + 1;
						updatePosition(*next, moves + 1, false);
					}
			}
			if (auto next = TetrisNode::shift(cur, map, -1, 0)) { // Left
				next->step = cur.step;
				updatePosition(*next, moves + 1, false);
			}
			if (auto next = TetrisNode::shift(cur, map, 1, 0)) { // Right
				next->step = cur.step;
				updatePosition(*next, moves + 1, false);
			}
			if (auto next = TetrisNode::rotate<true>(cur, map)) { // Rotate Reverse
				next->step = cur.step;
				updatePosition(*next, moves + 1, true);
			}
			if (auto next = TetrisNode::rotate<false>(cur, map)) { // Rotate Normal
				next->step = cur.step;
				updatePosition(*next, moves + 1, true);
			}
		}
		return result;
	}

	static std::vector<Oper> make_path(const TetrisNode& startNode, TetrisNode& landPoint, TetrisMap& map,
		bool NoSoftToBottom = false) {
		using OperMark = std::pair<TetrisNode, Oper>;
		auto softDrop_disable = true;
		std::deque<TetrisNode> nodeSearch;
		filterLookUp<OperMark> nodeMark{ map };
		auto& landNode = landPoint;

		auto mark = [&nodeMark](TetrisNode& key, const OperMark& value) {
			if (!nodeMark.contain(key)) {
				nodeMark.set(key, value);
				return true;
			}
			else
				return false;
		};

		auto buildPath = [&nodeMark, &NoSoftToBottom](TetrisNode& lp) {
			std::deque<Oper> path;
			TetrisNode* node = &lp;
			while (nodeMark.contain(*node)) {
				auto& result = *nodeMark.get(*node);
				auto& next = std::get<0>(result);
				auto dropToSd =
					(path.size() > 0 && std::get<1>(result) == Oper::DropToBottom);
				auto softDropDis = next.y - node->y;
				node = &next;
				if (node->type == Piece::None)
					break;
				path.push_front(std::get<1>(result));
				if (dropToSd && NoSoftToBottom) {
					path.pop_front();
					path.insert(path.begin(), softDropDis, Oper::SoftDrop);
				}
			}
			while (path.size() != 0 && (path[path.size() - 1] == Oper::SoftDrop ||
				path[path.size() - 1] == Oper::DropToBottom))
				path.pop_back();
			path.push_back(Oper::HardDrop);
			return std::vector<decltype(path)::value_type>(
				std::make_move_iterator(path.begin()),
				std::make_move_iterator(path.end()));
		};

		nodeSearch.push_back(startNode);
		nodeMark.set(startNode, OperMark{ TetrisNode::spawn(Piece::None), Oper::None });

		auto disable_d = landNode.open(map);
		auto need_rotate =
			landNode.type == Piece::T && landNode.typeTSpin != TSpinType::None;
		static constexpr Oper opers[]{ Oper::Cw, Oper::Ccw, Oper::Left, Oper::Right, Oper::SoftDrop, Oper::DropToBottom };
		if (landNode.type == Piece::T && landNode.typeTSpin != TSpinType::None)
			disable_d = false;
		while (!nodeSearch.empty()) {
			auto next = nodeSearch.front();
			nodeSearch.pop_front();
			for (auto& oper : opers) {
				auto rotate = false;
				std::optional<TetrisNode> node{ std::nullopt };
				if (oper == Oper::SoftDrop && softDrop_disable)
					continue;
				bool res = false;
				switch (oper) {
				case Oper::Cw:
					res = (node = TetrisNode::rotate<true>(next, map)) &&
						mark(*node, OperMark{ next, oper });
					rotate = true;
					break;
				case Oper::Ccw:
					res = (node = TetrisNode::rotate<false>(next, map)) &&
						mark(*node, OperMark{ next, oper });
					rotate = true;
					break;
				case Oper::Left:
					res = (node = TetrisNode::shift(next, map, -1, 0)) &&
						mark(*node, OperMark{ next, oper });
					break;
				case Oper::Right:
					res = (node = TetrisNode::shift(next, map, 1, 0)) &&
						mark(*node, OperMark{ next, oper });
					break;
				case Oper::SoftDrop:
					res = (node = TetrisNode::shift(next, map, 0, 1)) &&
						mark(*node, OperMark{ next, oper });
					break;
				case Oper::DropToBottom:
					res = (node = next.drop(map)) && mark(*node, OperMark{ next, oper });
					break;
				default:
					break;
				}
				if (res && *node == landNode) {
					if (need_rotate ? rotate : true)
						return buildPath(landNode);
					else
						nodeMark.clear(*node);
				}
				else if ((oper == Oper::DropToBottom ? !disable_d : true) && node && res)
					nodeSearch.push_back(*node);
			}
		}
		return std::vector<Oper>{};
	}
};

template<class TreeContext>
struct Evaluator {
	std::array<int, 12> combo_table{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
	std::array<int, 5> clears{ 0, 0, 1, 2, 4 };
	TreeContext* context = nullptr;

	struct Reward {
		double value;
		int attack;
	};

	struct Value {
		double value;
		int spike;

		Value operator+(const Value & rhs) {
			return Value{
				value + rhs.value,
				spike + rhs.spike
			};
		}
		Value operator-(const Value& rhs) {
			return Value{
				value - rhs.value,
				spike - rhs.spike
			};
		}

		Value operator+(const Reward & rhs) {
			return Value{
				value + rhs.value,
				rhs.attack == -1 ? 0 : spike + rhs.attack
			};
		}

		bool operator!=(const Value& rhs) {
			return value != rhs.value || spike != rhs.spike;
		}

		void improve(const Value& new_result) {
			value = std::max(value, new_result.value);
			spike = std::max(spike, new_result.spike);
		}
	};

	struct BoardWeights {
		int height = -100;
		int holes = -80;
		int cells_coveredness = -40;
		int row_transition = -60;
		int col_transition = -40;
		int well_depth = -30;
		std::array<int, 4> tslot = { 60, 150, 400, 800 };

		static auto default_args () {
			BoardWeights p;
			memset(&p, 0, sizeof p);
			return p;
		}
	}board_weights;

	struct PlacementWeights {
		int back_to_back = 200;
		int b2b_clear = 200;
		int clear1 = -200;
		int clear2 = -100;
		int clear3 = -50;
		int clear4 = 200;
		int tspin1 = 120;
		int tspin2 = 470;
		int tspin3 = 690;
		int mini_tspin = -50;
		int perfect_clear = 1000;
		int combo_garbage = 160;
		int wasted_t = -350;
		int soft_drop = -10;

		static auto default_args() {
			PlacementWeights p;
			memset(&p, 0, sizeof p);
			return p;
		}
	} placement_weights;

	struct Result {
		int count;
		int clear;
		int combo;
		bool b2b;
		int attack;
		bool deaded;
		TSpinType tspin;
	};

	struct Evaluation {
		Value eval;
		Reward reward;
	};

	Evaluator(TreeContext* _ctx) :context(_ctx) {}

std::optional<TetrisNode> tslot_tsd(const TetrisMap& map) {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h1 <= h2 - 1 && map(x, h2) && !map(x, h2 + 1) && map(x, h2 + 2)) {
				return TetrisNode{ Piece::T, x, h2, 2 };
			}
		}
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h1 <= h0 - 1 && map(x + 2, h0) && !map(x + 2, h0 + 1) && map(x + 2, h0 + 2)) {
				return TetrisNode{ Piece::T, x, h0, 2 };
			}
		}
		return std::nullopt;
	}

	std::optional<TetrisNode> tslot_tst(const TetrisMap& map) {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h0 <= h1 && map(x - 1, h1 + 1) == map(x - 1, h1 + 2) &&
				!map(x + 1, h1 - 1) &&
				!map(x + 2, h1) && !map(x + 2, h1 - 1) && !map(x + 2, h1 - 2) && !map(x + 2, h1 + 1) && map(x + 2, h1 + 2)
				) {
				return TetrisNode{ Piece::T, x + 1, h1 - 2, 3 };
			}
			else if (h2 <= h1 && map(x + 3, h1 + 1) == map(x + 3, h1 + 2) &&
				!map(x + 1, h1 - 1) &&
				!map(x, h1) && !map(x, h1 - 1) && !map(x, h1 - 2) && !map(x, h1 + 1) && map(x, h1 + 2)
				) {
				return TetrisNode{ Piece::T, x - 1 ,h1 - 2,1 };
			}
		}
		return std::nullopt;
	}

	std::optional<TetrisNode> cave_tslot(const TetrisMap& map, const TetrisNode& piece) {
		auto node = piece.drop(map);
		auto& x = node.x, & y = node.y;
		if (node.rs == 1) {
			if (!map(x, y + 1) && map(x, y) && map(x, y + 2) && map(x + 2, y)) {
				return TetrisNode{ Piece::T, x, y, 2 };
			}
			else if (
				!map(x + 2, y) && !map(x + 3, y) && !map(x + 2, y - 1) &&
				map(x, y + 1) && map(x + 3, y + 1) && map(x + 1, y - 2) && map(x + 3, y - 2)
				) {
				return TetrisNode{ Piece::T, x + 1, y - 1, 2 };
			}
		}
		if (node.rs == 3) {
			if (!map(x + 2, y + 1) && map(x, y) && map(x + 2, y + 2) && map(x + 2, y)) {
				return TetrisNode{ Piece::T, x, y, 2 };
			}
			else if (
				!map(x, y) && !map(x - 1, y) && !map(x, y - 1) &&
				map(x - 1, y + 1) && map(x - 1, y - 2) && map(x + 1, y - 2) && map(x + 2, y + 1)
				) {
				return TetrisNode{ Piece::T, x - 1, y - 1, 2 };
			}
		}
		return std::nullopt;
	};

	int getComboAtt(int combo) {
		const auto& comboCfg = combo_table;
		return comboCfg[std::min<std::size_t>(comboCfg.size() - 1, std::max<int>(0, combo))];
	}

	Result get(const Result& status, TetrisNode& lp, TetrisMap& map, int clear) {
		auto result = status;
		auto safe = getSafe(map);
		result.deaded = (safe < 0);
		result.clear = clear;
		result.attack = 0;
		bool b2b = result.b2b && status.b2b;
		//get TSpinType
		if (lp.type == Piece::T && clear > 0 && lp.lastRotate) {
			lp.typeTSpin = lp.spin ? (clear == 1 && lp.mini ? TSpinType::TSpinMini : TSpinType::TSpin) : TSpinType::None;
		}
		result.tspin = lp.typeTSpin;
		result.b2b = (status.b2b && result.clear == 0) || result.clear == 4 || (result.clear != 0 && result.tspin != TSpinType::None);
		result.combo = result.clear > 0 ? result.combo + 1 : 0;
		result.count = map.count;
		if (result.clear) {
			switch (result.tspin) {
			case TSpinType::None:
				result.attack += clears[clear] + (clear == 4 ? b2b : 0); break;
			default:
				result.attack += clear * 2 - (result.tspin == TSpinType::TSpinMini) + b2b * (clear == 3 ? 2 : 1); break;
			}
			result.attack += getComboAtt(result.combo - 1);
			if (result.count == 0) result.attack += 6;
		}
		return result;
	}

	Evaluation evalute(const TetrisNode& node, TetrisMap& _map, const std::optional<Piece>& hold,
		std::vector<Piece>* nexts, int index, int needSd, Result& status, Result& prev_status) {
		TetrisMap* map_ptr = &_map;
		double transient_eval = 0, acc_eval = 0;
		auto highest_point = _map.roof;
		transient_eval += board_weights.height * highest_point;

		auto ts = std::max<int>(std::count(nexts->begin() + index, nexts->end(), Piece::T) + static_cast<int>(hold == Piece::T), 1);

		if (ts > 0) map_ptr = new TetrisMap(_map);
		auto& map = *map_ptr;

		if (highest_point <= 15)
		for (auto i = 0; i < ts; i++) {
			auto lines = -1;
			std::optional<TetrisNode> piece;
			if (piece = tslot_tst(map)) {
				auto piece_s = cave_tslot(map, *piece);
				if (piece_s) {
					piece = piece_s;
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
				}
				else if (
					map(piece->x, piece->y) +
					map(piece->x, piece->y + 2) +
					map(piece->x + 2, piece->y) +
					map(piece->x + 2, piece->y + 2) >= 3
					&& !piece->check(map, piece->x, piece->y - 1)) {
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
				}
				else {
					piece = std::nullopt;
				}
			}
			else if (piece = tslot_tsd(map)) {
				lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
			}
			else {
				break;
			}
			if (lines != -1) {
				transient_eval += board_weights.tslot[lines];
			}
			if (lines >= 2) {
				piece->attach(map);
			}
			else break;
		}

		if (board_weights.well_depth != 0) { //wells
			auto wells = 0;
			for (auto x = 0; x < map.width; x++) 
				for (auto y = map.roof - 1, fit = 0; y >= 0; y--, fit = 0) {
					if (x == 0 && !map(x, y) && map(x + 1, y) && ++fit) 
						wells++;
					else if (x == map.width - 1 && map(x - 1, y) && !map(x, y) && ++fit) 
						wells++;
					else if (map(x - 1, y) && !map(x, y) && map(x + 1, y) && ++fit) 
						wells++;
					if (fit) 
						for (auto y1 = y - 1; y1 >= 0; y1--) {
							if (!map(x, y1)) {
								wells++;
								y--;
							}
							else break;
						}
			}
			transient_eval += board_weights.well_depth * wells;
		}

		auto min_roof = *std::min_element(map.top, map.top + map.width);
		if (board_weights.row_transition != 0 || board_weights.col_transition != 0) {
			int row_trans = 0, col_trans = 0;
#define marco_side_fill(v) (v | (1 << map.width - 1))
			for (int y = map.roof - 1; y >= 0; y--) {
				row_trans += BitCount(marco_side_fill(map[y]) ^ marco_side_fill(map[y] >> 1));
				col_trans += BitCount(map[y] ^ map[bitwise::min(y + 1, map.height - 1)]);
			}
#undef marco_side_fill
			transient_eval += board_weights.row_transition * row_trans;
			transient_eval += board_weights.col_transition * col_trans;
		}

		int hole_top_y = -1, hole_top_bits = 0;
		if (board_weights.holes != 0) { //holes
			auto holes = 0, cover = 0;
			for (int y = map.roof - 1; y >= 0; y--) {
				cover |= map[y];
				int check = cover ^ map[y];
				if (check != 0) {
					holes += BitCount(check);
					if (hole_top_y == -1) {
						hole_top_y = y + 1;
						hole_top_bits = check;
					}
				}
			}
			transient_eval += board_weights.holes * holes;
		}

		if (board_weights.cells_coveredness != 0) { //covered_cells
			auto covered = 0;
			if (hole_top_y != -1)
				for (int y = hole_top_y; y < map.roof; y++) {
					int check = hole_top_bits & map[y];
					if (check == 0) {
						break;
					}
					covered += BitCount(check);
				}
			transient_eval += board_weights.cells_coveredness * covered;
		}

		if (map_ptr != &_map) {
			delete map_ptr;
		}

		//acc_value
		if (status.b2b)
			transient_eval += placement_weights.back_to_back;

		acc_eval -= std::max(highest_point - 10, 0) * 50;
		acc_eval += needSd * placement_weights.soft_drop;

		if (node.type == Piece::T && (node.typeTSpin == TSpinType::None || status.clear < 2)) {
			acc_eval += placement_weights.wasted_t;
		}

		int b2b_cut = (!status.b2b&& prev_status.b2b) ? placement_weights.back_to_back : 1;

		acc_eval += std::pow(std::max(0, status.clear - 1), 2) * 20;

		if (status.clear) {
			auto comboAtt = getComboAtt(status.combo - 1);
			acc_eval += (2 * comboAtt > status.attack ? status.combo * 0.5 : 1.) * comboAtt * placement_weights.combo_garbage;
			if (status.count == 0)
				acc_eval += placement_weights.perfect_clear;
			if (status.b2b && prev_status.b2b)
				acc_eval += placement_weights.b2b_clear;
			switch (status.clear) {
			case 1:
				if (status.tspin == TSpinType::TSpin)
					acc_eval += placement_weights.tspin1;
				else if (status.tspin == TSpinType::TSpinMini)
					acc_eval += placement_weights.mini_tspin;
				else
					acc_eval += placement_weights.clear1;
				break;
			case 2:
				if (status.tspin == TSpinType::TSpin)
					acc_eval += placement_weights.tspin2;
				else
					acc_eval += placement_weights.clear2;
				break;
			case 3:
				if (status.tspin == TSpinType::TSpin)
					acc_eval += placement_weights.tspin3;
				else
					acc_eval += placement_weights.clear3;
				break;
			case 4:
				acc_eval += placement_weights.clear4;
				break;
			}
		}

		return {
			Value{transient_eval, 0},
			Reward{acc_eval, status.clear > 0 ? status.attack : -1}
		};
	}

	int getSafe(const TetrisMap& map) {
		int safe = INT_MAX;
		for (auto type = 0; type < 7; type++) {
			auto& virtualNode = context->getNode(static_cast<Piece>(type));
			if (!virtualNode.check(map)) {
				safe = -1;
				break;
			}
			else {
				auto dropDis = virtualNode.getDrop(map);
				safe = std::min(dropDis, safe);
			}
		}
		return safe;
	}
};

template<class TreeNode>
class TreeContext {
public:
	using Evaluator = Evaluator<TreeContext>;
	friend TreeNode;
	using Result = std::tuple<TetrisNode*, bool>;

public:
	std::vector<TetrisNode> nodes;
	Evaluator evalutator{ this };
	bool isOpenHold = true;
	std::atomic<int> count = 0;
	int flag = 0, isAdvanceNext{}; //detect none hold convert to be used
	std::vector<Piece>nexts;
	std::random_device rd;
	std::mt19937 gen{ rd() };
	std::string info{};
	std::shared_mutex mtx;

	TreeContext() :root(nullptr, TreeContext::treeDelete) {
	};

	~TreeContext() {
		root.reset(nullptr);
		return;
	}

	void createRoot(const TetrisNode& node, const TetrisMap& map, std::vector<Piece>& dp, std::optional<Piece> hold, bool canHold,
		int b2b, int combo, int dySpawn) {
		count = 0;
		nexts = dp;
		nodes.clear();
		for (auto i = 0; i < 7; i++) 
			nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i), &map, dySpawn));

		TreeNode* tree = new TreeNode;
		{//init for root
			tree->context = this;
			tree->nexts = &nexts;
			tree->cur = node.type;
			tree->map = map;
			tree->hold = hold;
			tree->props.status.b2b = b2b;
			tree->props.status.combo = combo;
			tree->isHoldLock = !canHold;
		}
		update(tree);
		//root.reset(tree);
	}

	void generator() {
		static thread_local std::unique_ptr<std::mt19937> generator = nullptr;
		if (!generator) generator.reset(new std::mt19937(clock() + std::hash<std::thread::id>{}(std::this_thread::get_id())));
	}

	void run() {
		static thread_local std::unique_ptr<std::mt19937> generator = nullptr;
		static thread_local std::uniform_real_distribution<>d(0, 1);
		if (!generator)
			generator.reset(new std::mt19937(clock() + std::hash<std::thread::id>{}(std::this_thread::get_id())));
		TreeNode* branch = root.get();
		while (true) {
			std::shared_lock guard{ branch->mtx };
			const auto& branches = branch->children;
			if (branches.empty()) break;
			const double exploration = 0.6931471805599453;
			branch = branch->children[static_cast<std::size_t>(std::fmod(-std::log(d(*generator)) / exploration, branch->children.size()))];
		}
		if (branch->eval()) {
			branch->repropagate();
		}
	}

	void save(TreeNode* t) {
		if (t == nullptr) return;
		t->ndx--, t->nth--;
		if (flag == 1) {
			t->ndx -= 1;
		}
		if (!t->cur.has_value()) {
			auto val = 0;
			if (flag == 1) val = t->nexts->size() - 2;
			else val = t->nexts->size() - 1;
			t->cur = decltype(t->cur){t->nexts->operator[](val)};
		}
		for (auto& child : t->children) {
			if (child != nullptr)
				save(child);
		}
	}

	void update(TreeNode* node) {
		count = 0;
		if (root == nullptr) {
			root.reset(node);
			return;
		}

		auto it = std::find_if(root->children.begin(), root->children.end(), [&](auto val) {return (*node) == (*val); });
		if (it != root->children.end()) {
			info = "found";
			auto selected = *it;
			if (!selected->parent->hold.has_value() && selected->hold && isAdvanceNext == 0) {
				isAdvanceNext = 1;
				info += "  flag = 1";
				flag = 1;
			}
			selected->parent = nullptr;
			save(selected);
			if (isAdvanceNext == 1 && flag < 2) flag++;
			root->children.erase(it);
			root.reset(selected);
			delete node;
			node = nullptr;
		}
		else {
			info = "not found";
			flag = 0;
			isAdvanceNext = node->hold.has_value() ? -1 : 0;
			root.reset(node);
		}
	}

	Result empty() {
		return { nullptr, false };
	}

	Result getBest(int upAtt) {
		return root->pick(upAtt);
	}

	auto& getNode(Piece type) {
		return nodes[static_cast<int>(type)];
	}

	static void treeDelete(TreeNode* t) {
		if (t == nullptr) return;
		for (auto& child : t->children) {
			if (child != nullptr) treeDelete(child);
		}
		delete t;
	}

	std::unique_ptr<TreeNode, decltype(TreeContext<TreeNode>::treeDelete)*> root;
};


class TreeNode {
	using TreeContext = TreeContext<TreeNode>;
	using Evaluator = Evaluator<TreeContext>;
	friend TreeContext;
public:
	struct EvalArgs {
		TetrisNode land_node;
		int clear = 0;
		Evaluator::Result status;
		Evaluator::Evaluation raw;
		Evaluator::Value evaluation;
		int needSd = 0;

		bool operator<(const  EvalArgs& other) const {
			if (this->evaluation.value == other.evaluation.value)
				this->evaluation.spike < other.evaluation.spike;
			return this->evaluation.value < other.evaluation.value;
		}

		bool operator>(const  EvalArgs& other) const {
			if (this->evaluation.value == other.evaluation.value)
				this->evaluation.spike > other.evaluation.spike;
			return this->evaluation.value > other.evaluation.value;
		}
	};

	constexpr static struct {
		bool operator()(TreeNode* const& lhs, TreeNode* const& rhs) const {
			return lhs->props > rhs->props;
		}
	}TreeNodeCompare{};

private:
	TreeNode* parent = nullptr;
	TreeContext* context = nullptr;
	std::vector<Piece>* nexts = nullptr;
	int ndx = 0, nth = 0;
	TetrisMap map;
	std::shared_mutex mtx;
	std::atomic<bool> leaf = true, occupied = false;
	bool isHold = false, isHoldLock = false;
	std::optional<Piece> cur = std::nullopt, hold = std::nullopt;
	EvalArgs props;
	std::vector<TreeNode*>children;

public:
	TreeNode() {}
	template<class TetrisMap>
	TreeNode(TreeContext* _ctx, TreeNode* _parent, std::optional<Piece> _cur, TetrisMap&& _map,
		int _ndx, std::optional<Piece> _hold, bool _isHold, const EvalArgs& _props) :
		parent(_parent), context(_ctx), nexts(_parent->nexts), ndx(_ndx),
		map(std::forward<TetrisMap>(_map)), isHold(_isHold), cur(_cur), hold(_hold), props(_props) {
	}

	bool operator==(TreeNode& p) {
		return cur == p.cur &&
			map == p.map &&
			hold == p.hold;
	}

	auto branches() {
		std::vector<TreeNode*> arr;
		for (auto& it : children) {
			if (it->leaf) arr.push_back(it);
		}
		return arr;
	}

	TreeNode* generateChildNode(TetrisNode& land_node, bool _isHoldLock,
		std::optional<Piece> _hold, bool _isHold, int index) {
		auto next_node = (index >= nexts->size() ? std::nullopt : decltype(cur){(*nexts)[index]});
		auto map_ = map;
		auto clear = land_node.attach(map_);
		auto needSd = static_cast<int>(land_node.type != Piece::T ? land_node.step : (!clear ? land_node.step : 0));
		auto status = context->evalutator.get(props.status, land_node, map_, clear);
		auto raw = context->evalutator.evalute(land_node, map_, _hold, nexts, index, needSd, status, props.status);
		EvalArgs props_ = {
			land_node, static_cast<int>(clear),
			status,
			raw,
			raw.eval  + raw.reward,
			needSd
		};
		TreeNode* new_tree = nullptr;
		if (!status.deaded) {
			context->count++;
			new_tree = new TreeNode{
				context, this, next_node, std::move(map_), static_cast<int>(index) + 1, _hold, _isHold, props_
			};
			new_tree->nth = nth + 1;
			new_tree->isHoldLock = _isHoldLock;
			{
				std::scoped_lock guard{ this->mtx };
				children.push_back(new_tree);
			}
		}
		return new_tree;
	}

	void search() {
		if (context->isOpenHold && !isHoldLock) {
			search_hold();
			return;
		}
		for (auto& node : Search::search(context->getNode(*cur), map)) {
			generateChildNode(node, false, hold, bool(isHold), ndx);
		}
	}

	template<size_t select = 0>
	void search_hold() {
		if constexpr (select == 0) {
			if (!hold.has_value() && nexts->size() > 0) return search_hold<1>();
			else return search_hold<2>();
		}
		else if constexpr (select == 1) { //hold is none
			if (cur) {
				for (auto& node : Search::search(context->getNode(*cur), map))
					generateChildNode(node, false, hold, false, ndx);
			}
			if (std::size_t after = ndx; !hold && after < nexts->size()) {
				auto take = (*nexts)[after];
				for (auto& node : Search::search(context->getNode(take), map))
					generateChildNode(node, false, cur, true, ndx + 1);
			}
		}
		else if constexpr (select == 2) { //has hold
			switch (cur == hold) {
			case false:
				for (auto& node : Search::search(context->getNode(*hold), map))
					generateChildNode(node, false, cur, true, ndx);
				[[fallthrough]];
			case true:
				for (auto& node : Search::search(context->getNode(*cur), map))
					generateChildNode(node, false, hold, false, ndx);
				break;
			}
		}
	}

	void repropagate() {
		auto it = this;
		if (it->children.empty()) 
			return;
		while (it != nullptr) {
			std::scoped_lock guard{ it->mtx };
			pdqsort(it->children.begin(), it->children.end(), TreeNode::TreeNodeCompare);
			auto improved = it->children[0]->props.evaluation + it->props.raw.reward;
			for (auto c = it->children.begin() + 1; c != it->children.end(); ++c) {
				improved.improve((*c)->props.evaluation + it->props.raw.reward);
			}
			if (it->props.evaluation != improved || it == this) {
				std::scoped_lock guard{ it->parent == nullptr ? context->mtx : it->parent->mtx };
				it->props.evaluation = improved;
			}
			else break;
			it = it->parent;
		}
	}

	bool eval() {
		bool result = false, expected = false;
		do {
			if (expected)
				return false;
		} while (!occupied.compare_exchange_weak(expected, true, std::memory_order_acquire));
		if (leaf) {
			if (cur) {
				if (children.size() == 0 || (parent == nullptr)) {
					search();
					leaf = false;
					result = true;
					goto out;
				}
			}
			else {
				goto out;
			}
		}
	out:
		occupied.store(false, std::memory_order_release);
		return result;
	}

	void showPlan(TreeNode* tree) {
		while (true) {
			std::cout << "index:" << tree->nth << "\n" << tree->map << "\n";
			if (tree->children.empty()) 
				return;
			tree = tree->children.front();
		}
	}

	TreeContext::Result pick(int upAtt) {
		TreeNode * backup = nullptr;
		for (auto& mv : children) {
			if (upAtt == 0 || std::all_of(mv->map.top + 3, mv->map.top + 7, [&](int h) {
				return upAtt - mv->props.status.attack + h <= 20;
				})) {
				backup = mv;
				break;
			}
			if (backup == nullptr)
				backup = mv;
			else if (backup->props.evaluation.spike < mv->props.evaluation.spike)
				backup = mv;
		}
		if (backup == nullptr)
			return context->empty();
		if (static_cast<double>(backup->map.count) / (backup->map.width * (backup->map.height - 20)) > 0.7) {
			std::cout << "!";
		}
		return { &backup->props.land_node, backup->isHold };
	}
};
