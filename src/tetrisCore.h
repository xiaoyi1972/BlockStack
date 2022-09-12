#pragma once
#include"TetrisBase.h"
#include"pdqsort.h"
#include<random>
#include<algorithm>
#include<functional>
#include<mutex>
#include<thread>
#include<atomic>

template<class T>
T BitCount(T n) {
	n = n - ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	n = (n + (n >> 4)) & 0x0f0f0f0f;
	n = n + (n >> 8);
	n = n + (n >> 16);
	return n & 0x3f;
}
template<class T, class ... Ts>
T BitCount(T first, Ts ... rest) {
	return BitCount(first) + BitCount(rest...);
}

struct Search {
	template<std::size_t index = 0, class Container = Vec<TetrisNode>>
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

		if (std::all_of(map.top, map.top + map.width, [&map](int n) { return n < map.height - 4; })) {
			fastMode = true;
			for (auto& node : first.getHoriZontalNodes(map, queue)) {
				auto& moves = node.moves;
				auto drs = std::abs(node.rs - first.rs), dx = std::abs(node.x - first.x);
				drs -= (drs + 1 >> 2 << 1);
				moves = 1 + dx + drs;
				node.y -= node.step = node.getDrop(map);
				checked.set(node, { cals, moves, false });
				++cals;
			}
			amount = cals;
		}
		else {
			fastMode = false;
			queue.emplace_back(first);
			checked.set(first, { 0, 0, false });
			amount = 1;
		}

		auto updatePosition = [&](TetrisNode& target, int moves, bool lastRotate) {
			if constexpr (index == 1) {
				if (lastRotate == true) {
					if (auto it = checked.get(target); it) {
						queue[it->index].lastRotate = true;
					}
				}
			}
			if (fastMode && target.open(map)) return;
			if (!checked.contain(target)) {
				target.lastRotate = lastRotate;
				checked.set(target, { amount,moves,false });
				queue.emplace_back(target.type, target.x, target.y, target.rs);
				queue.back().moves = moves, queue.back().lastRotate = target.lastRotate, queue.back().step = target.step;
				++amount;
			}
		};

		for (std::size_t i = 0; i != amount; ++i) {
			fixed = i < cals ? true : false;
			auto cur = queue[i];
			auto& moves = cur.moves;
			if (!cur.check(map, cur.x, cur.y - 1)) {
				if constexpr (index == 2) {//Piece I Z S
					auto value = checked.get<false>(cur);
					if (value == nullptr) value = checked.get<true>(cur);
					if (!value->locked) {
						result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
						result.back().step = fixed ? 0 : cur.step;
						checked.set<false>(cur, { result.size() - 1, moves, true });
					}
					else {
						auto& [index, step, locked] = *value;
						if (cur.rs == first.rs || step > moves) {
							result[index] = cur;
							result[index].step = fixed ? 0 : cur.step;
						}
					}
				}
				else {
					result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
					if constexpr (index == 1) { //Piece T
						auto [spin, mini] = result.back().corner3(map);
						result.back().lastRotate = cur.lastRotate;
						result.back().mini = mini, result.back().spin = spin;
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
#undef cur
#undef moves
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
		static constexpr Oper opers[]{ Oper::Cw,       Oper::Ccw,
									  Oper::Left,     Oper::Right,
									  Oper::SoftDrop, Oper::DropToBottom };
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
				else if ((oper == Oper::DropToBottom ? !disable_d : true) && node &&
					res)
					nodeSearch.push_back(*node);
			}
		}
		return std::vector<Oper>{};
	}
};

template<class TreeContext>
struct Evaluator {
	int col_mask_{}, row_mask_{}, full_count_{}, width{}, height{};
	std::array<int, 12> combo_table{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
	std::array<int, 5> clears{ 0, 0, 1, 2, 4 };
	TreeContext* context = nullptr;

	struct BoardWeights {
		int height = -50;
		int top_half = -150;
		int top_quarter = -511;
		int bumpiness = -50;
		int cavity_cells = -40;
		int covered_cells =  -100;
		int row_transition = -30;
		int well_depth = 50;
		int max_well_depth = -2;
		std::array<int, 4> tslot = { 10, 120, 350, 490 };
		std::array<int, 10> well_column = { 31, 16, -41, 37, 49, 30, 56, 48, -27, 22 };
	}board_weights;

	struct PlacementWeights {
		int back_to_back = 100;
		int b2b_clear = 200;
		int clear1 = -140;
		int clear2 = -100;
		int clear3 = -60;
		int clear4 = 0;
		int tspin1 = 120;
		int tspin2 = 410;
		int tspin3 = 630;
		int mini_tspin = -350;
		int perfect_clear = 1000;
		int combo_garbage = 200;
		int wasted_t = -150;
		int soft_drop = -25;
	} placement_weights;

	struct Result {
		int count;
		int clear;
		int combo;
		bool b2b;
		int attack;
		int under_attack;
		bool deaded;
		TSpinType tspin;
	};

	struct Evaluation {
		double transient_value;
		double acc_value;
	};

	Evaluator(TreeContext* _ctx) :context(_ctx) {

	}

	void init(const TetrisMap* map) {
		auto full = ((1 << map->width) - 1);
		col_mask_ = full & ~1;
		row_mask_ = full;
		full_count_ = map->width * 24;
		width = map->width;
		height = map->height;
	}

	std::optional<TetrisNode> tslot_tsd(const TetrisMap& map) {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto& [h0, h1, h2] = std::tuple{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h1 <= h2 - 1 &&
				map(x, h2) && !map(x, h2 + 1) && map(x, h2 + 2)
				) {
				return TetrisNode{ Piece::T, x, h2, 2 };
			}
			else if (h1 <= h0 - 1 &&
				map(x + 2, h0) && !map(x + 2, h0 + 1) && map(x + 2, h0 + 2)
				) {
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
		return comboCfg[std::min<std::size_t>(comboCfg.size() - 1, combo)];
	}

	Result get(const Result& status, TetrisNode& lp, TetrisMap& map, int clear) {
		auto result = status;
		auto safe = getSafe(map);
		result.deaded = (safe <= 0);
		result.clear = clear;
		result.attack = 0;
		//get TSpinType
		if (lp.type == Piece::T && clear > 0 && lp.lastRotate) {
			lp.typeTSpin = lp.spin ? (clear == 1 && lp.mini ? TSpinType::TSpinMini : TSpinType::TSpin) : TSpinType::None;
		}
		result.tspin = lp.typeTSpin;
		result.b2b = (status.b2b && result.clear == 0) || result.clear == 4 || (result.clear != 0 && result.tspin != TSpinType::None);
		result.combo = result.clear > 0 ? result.combo + 1 : 0;
		result.count = map.count;
		if (result.clear) {
			if (result.tspin != TSpinType::None) {
				switch (clear) {
				case 1: result.attack += (result.tspin == TSpinType::TSpinMini ? 1 : 2) + result.b2b; break;
				case 2: result.attack += (4 + result.b2b); break;
				case 3: result.attack += (result.b2b ? 6 : 8); break;
				default: break;
				}
			}
			else result.attack += (clears[result.clear] + (result.clear == 4 ? result.b2b : 0));
			if (result.count == 0)  result.attack += 6;
			result.attack += getComboAtt(result.combo);
			result.under_attack -= result.attack;
		}
		else {
			result.deaded = (result.under_attack - result.attack >= safe);
			result.under_attack = 0;
		}
		return result;
	}

	Evaluation evalute(const TetrisNode& node, TetrisMap& _map, const std::optional<Piece>& hold,
		std::vector<Piece>* nexts, int index, int needSd, Result& status) {
		TetrisMap* map_ptr = &_map;
		Evaluation result{ 0,0 };
		auto& [transient_eval, acc_eval] = result;

		auto highest_point = *std::max_element(_map.top, _map.top + _map.width);
		transient_eval += board_weights.height * highest_point;
		transient_eval += board_weights.top_half * std::max(highest_point - 10, 0);
		transient_eval += board_weights.top_quarter * std::max(highest_point - 15, 0);

		auto ts = std::max<int>(std::count(nexts->begin(),// + index, 
			nexts->end(), Piece::T) + static_cast<int>(hold == Piece::T), 0);
		ts = std::max(1, ts);
		if (ts > 0) map_ptr = new TetrisMap(_map);
		auto& map = *map_ptr;

		for (auto i = 0; i < ts; i++) {
			auto lines = -1;
			auto type = -1;
			std::optional<TetrisNode> piece;
			if (piece = tslot_tsd(map)) {
				lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
				type = 1;
			}
			else if (piece = tslot_tst(map)) {
				auto piece_s = cave_tslot(map, *piece);
				if (piece_s) {
					piece = piece_s;
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
					type = 2;
				}
				else if (
					map(piece->x, piece->y) +
					map(piece->x, piece->y + 2) +
					map(piece->x + 2, piece->y) +
					map(piece->x + 2, piece->y + 2) >= 3
					&& !piece->check(map, piece->x, piece->y - 1)) {
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);
					type = 3;
				}
				else {
					piece = std::nullopt;
				}
			}
			else break;
			if (lines != -1) transient_eval += board_weights.tslot[lines];
			if (lines >= 2) {
				/*if (type == 1) {
					std::cout << "\n" << piece->mapping(map, true);
					if (type != -1)
						std::cout << (type == 1 ? "sky_slot" : (type == 2 ? "cave_slot" : "tst_slot")) << "\n";
				}*/
				piece->attach(map);
			}
			else break;
		}

		auto well = 0;
		for (auto x = 1; x < map.width; x++)
			if (map.top[x] <= map.top[well]) well = x;

		auto depth = 0;
		for (auto y = map.top[well]; y < 20; y++) {
			for (auto x = 0; x < 10; x++) {
				if (x != well && !(map)(x, y)) {
					goto out;
				}
				depth += 1;
			}
		}
	out:

		depth = std::min(depth, board_weights.max_well_depth);
		transient_eval += board_weights.well_depth * depth;
		if (depth != 0)
			transient_eval += board_weights.well_column[well];

		if (board_weights.row_transition != 0) {
			int row_transition = 0;
			for (int i = map.roof; i >= 0; i--) {
				row_transition += BitCount(map[i] ^ (map[i] >> 1));
			}
			transient_eval += row_transition * board_weights.row_transition;
		}

		if (board_weights.bumpiness != 0) {
			auto bump = [&map, &well]() {  //bumpiness 
				auto bumpiness = -1;
				auto  prev = (well == 0 ? 1 : 0);
				for (auto i = 1; i < 10; i++) {
					if (i == well) continue;
					auto dh = std::abs(map.top[prev] - map.top[i]);
					bumpiness += dh;
					prev = i;
				}
				return  std::abs(bumpiness);
			}();
			transient_eval += bump * board_weights.bumpiness;
		}

		if (board_weights.cavity_cells != 0) {
			auto cavity_cells = [&map]() {  //cavities
				auto cavities = 0, overhangs = 0;
				for (auto y = 0; y < map.roof; y++) {
					for (auto x = 0; x < 10; x++) {
						if (map(x, y) || y >= map.top[x]) continue;
						cavities += 1;
					}
				}
				return cavities;
			}();
			transient_eval += board_weights.cavity_cells * cavity_cells;
		}

		if (board_weights.covered_cells != 0) {
			auto covered_cells = [&map]() {  //covered_cells
				auto covered = 0, covered_sq = 0;
				for (auto x = 0; x < 10; x++) {
					auto cells = 0;
					for (int y = map.top[x] - 1; y >= 0; y--) {
						if (!map(x, y)) {
							covered += cells;
							cells = 0;
						}
						else cells += 1;
					}
				}
				return covered;
			}();
			transient_eval += board_weights.covered_cells * covered_cells;
		}

		if (map_ptr != &_map) {
			delete map_ptr;
		}

		//acc_value
		if (status.b2b) 
			acc_eval += placement_weights.back_to_back;

		acc_eval += needSd * placement_weights.soft_drop;

		if (node.type == Piece::T && node.typeTSpin == TSpinType::None) {
			acc_eval += placement_weights.wasted_t;
		}

		if (status.clear) {
			if (status.count == 0) {
				acc_eval += placement_weights.perfect_clear;
			}
			if (status.b2b) {
				acc_eval += placement_weights.b2b_clear;
			}
			int comboAtt = getComboAtt(status.combo);
			acc_eval += comboAtt * placement_weights.combo_garbage;
			switch (status.clear) {
			case 1:
				if (status.tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin1;
				else if (status.tspin == TSpinType::TSpinMini) acc_eval += placement_weights.mini_tspin;
				else acc_eval += placement_weights.clear1;
				break;
			case 2:
				if (status.tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin2;
				else acc_eval += placement_weights.clear2;
				break;
			case 3:
				if (status.tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin3;
				else acc_eval += placement_weights.clear3;
				break;
			case 4: 
				acc_eval += placement_weights.clear4; 
				break;
			}
		}
		return result;
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
	using Result = std::tuple<TetrisNode, bool>;

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
	std::mutex mtx;

	TreeContext() :root(nullptr, TreeContext::treeDelete) {
	};

	~TreeContext() {
		root.reset(nullptr);
		return;
	}

	void createRoot(const TetrisNode& node, const TetrisMap& map, std::vector<Piece>& dp, std::optional<Piece> hold, bool canHold,
		int b2b, int combo, int underAttack, int dySpawn) {
		evalutator.init(&map);
		count = 0;
		nexts = dp;
		nodes.clear();
		for (auto i = 0; i < 7; i++) nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i), &map, dySpawn));

		TreeNode* tree = new TreeNode;
		{//init for root
			tree->context = this;
			tree->nexts = &nexts;
			tree->cur = node.type;
			tree->map = map;
			tree->hold = hold;
			tree->props.status.under_attack = underAttack;
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
		if (!generator) generator.reset(new std::mt19937(clock() + std::hash<std::thread::id>{}(std::this_thread::get_id())));

		TreeNode* branch = root.get();
		while (true) {
			std::lock_guard<std::mutex> guard{ branch->mtx };
			auto& branches = branch->children;
			if (branches.empty()) break;
			std::vector<double> tmp{};
			tmp.reserve(branches.size());
			auto min = branches.back()->props.evaluation;
			for (auto index = 0; index < branches.size(); index++) {
				auto value = branches[index]->props.evaluation;
				auto e = value - min + 10;
				tmp.emplace_back(e * e / (index + 1.));
			}
			std::discrete_distribution<> d{ std::begin(tmp),std::end(tmp) };
			branch = branch->children[d(*generator)];
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
		return { TetrisNode::spawn(*root->cur), false };
	}

	Result getBest() {
		return root->getBest();
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
		Evaluator::Evaluation raw_eval;
		double evaluation;
		int needSd = 0;

		bool operator<(const  EvalArgs& other) const {
			return this->evaluation < other.evaluation;
		}

		bool operator>(const  EvalArgs& other) const {
			return this->evaluation > other.evaluation;
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
	std::mutex mtx;
	std::atomic<bool> extended = false;
	bool /*extended = false,*/ isHold = false, isHoldLock = false;
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
			if (it->extended) arr.push_back(it);
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
		auto raw_eval = context->evalutator.evalute(land_node, map_, _hold, nexts, index, needSd, status);
		EvalArgs props_ = {
			land_node, static_cast<int>(clear),
			status,
			raw_eval,
			raw_eval.acc_value + raw_eval.transient_value,
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
				std::lock_guard<std::mutex> guard{ this->mtx };
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
	void search_hold(bool op = false, bool noneFirstHold = false) {
		if constexpr (select == 0) {
			if (!hold.has_value() && nexts->size() > 0) return search_hold<1>(false, true);
			else return search_hold<2>(false, true);
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

	bool eval() {
		if (!extended) {
			if (cur) {
				if (children.size() == 0 || (parent == nullptr)) {
					search();
					extended = true;
					return true;
				}
			}
			else {
				return false;
			}
		}
		else return false;
	}

	void repropagate() {
		auto it = this, updated = it;
		if (it->children.empty()) return;
		while (it != nullptr) {
			{
				std::lock_guard<std::mutex> guard{ it->mtx };
				pdqsort(it->children.begin(), it->children.end(), TreeNode::TreeNodeCompare);
			}
			auto eval = it->props.raw_eval.acc_value + it->children[0]->props.evaluation;
			if (it->props.evaluation != eval) {
				it->props.evaluation = eval;
			}
			else break;
			it = it->parent;
		}
	}

	int depth() {
		auto it = this;
		int depth = 0;
		while (!it->children.empty()) {
			pdqsort(it->children.begin(), it->children.end(), TreeNode::TreeNodeCompare);
			it = it->children.front();
			depth++;
		}
		return depth;
	}

	void run() {}

	TreeContext::Result getBest() {
		if (children.empty()) return context->empty();
		pdqsort(children.begin(), children.end(), TreeNode::TreeNodeCompare);
		return { children.front()->props.land_node, children.front()->isHold };
	}
};
