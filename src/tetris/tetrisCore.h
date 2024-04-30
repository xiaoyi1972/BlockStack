#pragma once
#include"tetrisBase.h"
#include"tool.h"
#include"pdqsort.h"
#include"threadPool.h"

//#define TIMER_MEASURE
#ifdef TIMER_MEASURE
#include <timer.hpp>
#endif

#include<random>
#include<algorithm>
#include<thread>
#include<limits>
#include<memory_resource>
#include<memory>
#include<shared_mutex>
#include<span>
#include<queue>
#include<functional>

#ifdef TEST_REQUIRE
#include<unordered_set>
#include<set>
#include<syncstream>
#endif

#include<syncstream>

struct Search {
#ifdef CLASSIC
	template<class Container = trivail_vector<TetrisNode>>
	static Container search(const TetrisNode& first, const TetrisMap& map) {
		auto softDrop_disable = true, fixed = false, fastMode = true;
		Container result{};
		auto trans = 1 << 2 * (first.type != Piece::O);
		result.reserve(trans * map.width);
		auto node = first;
		for (auto i = 0; i++ < 4;) {
			const auto& points = node.data(node.rs).points;
			auto [p0, p1] = std::minmax_element(points.begin(), points.end(), [](auto& p0, auto& p1) {return p0.x < p1.x; });
			if (node.check(map, node.x, node.y, node.rs)) {
				result.emplace_back(node.type, node.x, node.y, node.rs);
				result.back().y -= result.back().getDrop(map);

				for (int n = node.x - 1; n >= -p0->x; --n) {
					if (node.check(map, n, node.y, node.rs)) {
						result.emplace_back(node.type, n, node.y, node.rs);
						result.back().y -= result.back().getDrop(map);
					}
				}
				for (int n = node.x + 1; n < map.width - p1->x; ++n) {
					if (node.check(map, n, node.y, node.rs)) {
						result.emplace_back(node.type, n, node.y, node.rs);
						result.back().y -= result.back().getDrop(map);
					}
				}
			}
			if (node.type == Piece::O)
				break;
			node.rotate();
		}
		return result;
	}
#else
	template<std::size_t index = 0, class Container = trivail_vector<TetrisNode>>
	static Container search(const TetrisNode& first, const TetrisMap& map) {
		if constexpr (index == 0) {
			if (first.type == Piece::T)
				return Search::search<1, Container>(first, map);
			else if (first.type == Piece::I || first.type == Piece::Z || first.type == Piece::S)
				return Search::search<2, Container>(first, map);
		}

		auto softDrop_disable = true, fixed = false, fastMode = true;
		Container queue{}, result{};
		filterLookUp<TetrisNodeMoveEx> checked(map);
		std::size_t amount = 0, cals = 0;

		if (first.y - map.roof >= 4) {
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
			if (!first.check(map))
				return result;
			fastMode = false;
			queue.emplace_back(first);
			checked.set(first, { 0, 0, false });
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
			if (fastMode && target.open(map))
				return;
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
				if constexpr (index == 2) { //Piece I Z S
					auto value = checked.get<false>(cur);
					if (value == nullptr)
						value = checked.get<true>(cur);
					if (!value->locked) {
						result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
						result.back().step = cur.step * !fixed;
						checked.set<false>(cur, { result.size() - 1, moves, true });
					}
					else {
						auto& [seq, step, locked] = *value;
						if (cur.rs == first.rs || step > moves) {
							result[seq] = cur;
							result[seq].step = cur.step * !fixed;
						}
					}
				}
				else {
					result.emplace_back(cur.type, cur.x, cur.y, cur.rs);
					if constexpr (index == 1) { //Piece T
						result.back().template corner3<true>(map);
						result.back().lastRotate = cur.lastRotate;
					}
					result.back().step = cur.step * !fixed;
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
#endif

	static std::vector<Oper> make_path_maybe_has_error(const TetrisNode& startNode, const TetrisNode& landNode, const TetrisMap& map,
		bool NoSoftToBottom = false) {
		auto softDrop_disable = true;
		std::deque<TetrisNode> nodeSearch;
		filterLookUp<OperMark> nodeMark(map);

		auto mark = [&nodeMark](TetrisNode& key, const OperMark& value) {
			if (!nodeMark.contain(key)) {
				nodeMark.set(key, value);
				return true;
			}
			else
				return false;
			};

		auto buildPath = [&nodeMark, &NoSoftToBottom](const TetrisNode& lp) {
			std::deque<Oper> path;
			auto* node = &lp;
			while (nodeMark.contain(*node)) {
				auto& result = *nodeMark.get(*node);
				auto& next = result.node;
				auto dropToSd = (path.size() > 0 && result.oper == Oper::DropToBottom);
				auto softDropDis = next.y - node->y;
				node = &next;
				if (node->type == Piece::None)
					break;
				path.push_front(result.oper);
				if (dropToSd && NoSoftToBottom) {
					path.pop_front();
					path.insert(path.begin(), softDropDis, Oper::SoftDrop);
				}
			}
			while (path.size() != 0 && (path[path.size() - 1] == Oper::SoftDrop || path[path.size() - 1] == Oper::DropToBottom))
				path.pop_back();
			path.push_back(Oper::HardDrop);
			return std::vector<decltype(path)::value_type>(
				std::make_move_iterator(path.begin()),
				std::make_move_iterator(path.end()));
			};

		Oper opers[] = { Oper::Cw, Oper::Ccw, Oper::Left, Oper::Right, Oper::SoftDrop, Oper::DropToBottom };
		auto disable_d = landNode.open(map) && startNode.y >= landNode.y && map.roof < startNode.y;
		auto need_rotate = landNode.type == Piece::T && landNode.typeTSpin != TSpinType::None;

		nodeSearch.emplace_back(startNode.type, startNode.x, startNode.y, startNode.rs);
		nodeMark.set(startNode, OperMark{ TetrisNode::spawn(Piece::None), Oper::None });

		if (need_rotate)
			disable_d = false;

		for (std::size_t i = 0; i != nodeSearch.size(); ++i) {
			auto& next = nodeSearch[i];
			for (auto oper : opers) {
				auto rotate = false;
				std::optional<TetrisNode> node{ std::nullopt };
				if (oper == Oper::SoftDrop && softDrop_disable)
					continue;
				switch (oper) {
				case Oper::Cw:
					node = TetrisNode::rotate<true>(next, map);
					rotate = true;
					break;
				case Oper::Ccw:
					node = TetrisNode::rotate<false>(next, map);
					rotate = true;
					break;
				case Oper::Left:
					node = TetrisNode::shift(next, map, -1, 0);
					break;
				case Oper::Right:
					node = TetrisNode::shift(next, map, 1, 0);
					break;
				case Oper::SoftDrop:
					node = TetrisNode::shift(next, map, 0, -1);
					break;
				case Oper::DropToBottom:
					node = next.drop(map);
					break;
				default:
					break;
				}
				if (node) {
					auto mark_v = mark(*node, OperMark{ next, oper });
					if (*node == landNode) {
						if (need_rotate ? rotate : true)
							return buildPath(landNode);
						else if (mark_v)
							nodeMark.clear(*node);
					}
					else if ((oper == Oper::DropToBottom ? !disable_d : true) && mark_v)
						nodeSearch.emplace_back(node->type, node->x, node->y, node->rs);
				}
			}
		}
		return {};
	}

	static std::vector<Oper> make_path(const TetrisNode& startNode, const TetrisNode& landPoint, const TetrisMap& map,
		bool NoSoftToBottom = false) {

		struct MoveNode {
			MoveNode* parent;
			TetrisNode node;
			Oper oper;
			int moves;

			bool operator<(const MoveNode& other) const {
				return this->moves > other.moves;
			}
		};

		std::deque<MoveNode> nodeSearch;
		filterLookUp<bool> nodeMark(map);
		std::priority_queue<MoveNode> alternatives;

		auto softDrop_disable = true;
		auto need_rotate = landPoint.type == Piece::T && landPoint.typeTSpin != TSpinType::None;

		auto buildPath = [&nodeMark, &NoSoftToBottom](const MoveNode* lp) {
			std::deque<Oper> path;
			auto cur = lp;
			while (cur->parent) {
				auto node = cur;
				auto next = cur->parent;
				auto dropToSd = (path.size() > 0 && cur->oper == Oper::DropToBottom);
				auto softDropDis = next->node.y - cur->node.y;
				cur = next;
				if (node->oper == Oper::None)
					break;
				path.push_front(node->oper);
				if (dropToSd && NoSoftToBottom) {
					path.pop_front();
					path.insert(path.begin(), softDropDis, Oper::SoftDrop);
				}
			}
			while (path.size() != 0 && (path[path.size() - 1] == Oper::SoftDrop || path[path.size() - 1] == Oper::DropToBottom))
				path.pop_back();
			path.push_back(Oper::HardDrop);
			return std::vector<decltype(path)::value_type>(
				std::make_move_iterator(path.begin()),
				std::make_move_iterator(path.end()));
			};

		nodeSearch.push_back({ nullptr, startNode, Oper::None, 0 });

		for (std::size_t i = 0; i != nodeSearch.size(); ++i) {
			auto cur = &nodeSearch[i];
			auto droped_dis = 0;
			if (cur->oper == Oper::DropToBottom) {
				droped_dis = cur->parent->node.y - cur->node.y;
			}
			auto& next = cur->node;
			for (auto oper : { Oper::Cw, Oper::Ccw, Oper::Left, Oper::Right, Oper::SoftDrop, Oper::DropToBottom }) {
				auto rotate = false;
				auto moves = 0;
				std::optional<TetrisNode> node{ std::nullopt };
				if (oper == Oper::SoftDrop && softDrop_disable)
					continue;
				switch (oper) {
				case Oper::Cw:
					node = TetrisNode::rotate<true>(next, map);
					rotate = true;
					moves += 1;
					break;
				case Oper::Ccw:
					node = TetrisNode::rotate<false>(next, map);
					rotate = true;
					moves += 1;
					break;
				case Oper::Left:
					node = TetrisNode::shift(next, map, -1, 0);
					moves += 1;
					break;
				case Oper::Right:
					node = TetrisNode::shift(next, map, 1, 0);
					moves += 1;
					break;
				case Oper::SoftDrop:
					node = TetrisNode::shift(next, map, 0, -1);
					moves += 1;
					break;
				case Oper::DropToBottom:
					node = next.drop(map);
					moves += 1;
					break;
				default:
					break;
				}
				moves *= droped_dis;
				moves += cur->moves;
				if (node) {
					if (*node == landPoint && (need_rotate ? rotate == need_rotate : true)) {
						alternatives.emplace(cur, *node, oper, moves);
					}
					if (!nodeMark.contain(*node)) {
						nodeMark.set(*node, true);
						nodeSearch.emplace_back(cur, *node, oper, moves);
					}
				}
			}
		}

		if (!alternatives.empty()) {
			return buildPath(&alternatives.top());
		}

		return {};

	}
};

template<class TreeContext>
struct Evaluator {
	std::array<int, 12> combo_table = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
	std::array<int, 5> clears = { 0, 0, 1, 2, 4 };
	TreeContext* context = nullptr;

	struct Reward {
		double value;
		int attack;
	};

	struct Value {
		double value;
		int spike;

		static Value lowest() {
			return {
				std::numeric_limits<double>::lowest(),
				std::numeric_limits<int>::lowest()
			};
		}

		Value operator+(const Value& rhs) {
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

		Value operator+(const Reward& rhs) {
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
#define Factor * 6 //3
		double height[3] = { -10.4699 Factor, 0 Factor, 0 Factor };
		double holes = 0 Factor;
		double hole_lines = -23.3727 Factor;
		double cells_coveredness = -6.11707 Factor;
		double row_transition = -7.52108 Factor;
		double col_transition = -8.00883 Factor;
		double well_depth = -12.1257 Factor;
		double hole_depth = -4.91563 Factor;
#undef Factor
		double tslot[4] = { 120, 240, 480, 960 };

		static auto default_args() {
			BoardWeights p{
				.height = {-50, -50, -50},
				.holes = -50,
				.hole_lines = -50,
				.cells_coveredness = -50,
				.row_transition = -50,
				.col_transition = -50,
				.well_depth = -50,
				.hole_depth = -50,
				.tslot = {50, 50, 50, 50}
			};
			return p;
		}

		template<class T>
		static BoardWeights& from_vecparms(BoardWeights& weight, T& p) {
			weight.height[0] = p[0];
			weight.height[1] = p[1];
			weight.height[2] = p[2];
			weight.holes = p[3];
			weight.hole_lines = p[4];
			weight.cells_coveredness = p[5];
			weight.row_transition = p[6];
			weight.col_transition = p[7];
			weight.well_depth = p[8];
			weight.hole_depth = p[9];
			return weight;
		}


		static std::tuple<std::string_view, std::string_view> vecparms_desccription(int index) {
			static std::array description{
				std::tuple{"double height[3] = {", ", "},
				std::tuple{ "", ", " },
				std::tuple{ "", "};\n" },
				std::tuple{ "double holes = " , ";\n" },
				std::tuple{ "double hole_lines = " , ";\n" },
				std::tuple{ "double cells_coveredness = ", ";\n" },
				std::tuple{ "double row_transition = ", ";\n" },
				std::tuple{ "double col_transition = ", ";\n" },
				std::tuple{ "double well_depth = ",";\n" },
				std::tuple{ "double hole_depth = ",";\n" }
			};
			if (index < 0 || index >= description.size())
				return { "","" };
			return description[index];
		}

		BoardWeights backup() {
			return BoardWeights{
			.height = {-12, 0, 0},
			.holes = -16,
			.hole_lines = -48,
			.cells_coveredness = -16,
			.row_transition = -16,
			.col_transition = -16,
			.well_depth = -16,
			.hole_depth = -16,
			};

			/*
			clear: 11071811  parms:
			[
			double height[3] = {-10.4699, 0, 0};
			double holes = 0;
			double hole_lines = -23.3727;
			double cells_coveredness = -6.11707;
			double row_transition = -7.52108;
			double col_transition = -8.00883;
			double well_depth = -12.1257;
			double hole_depth = -4.91563;
			]
			*/
		}


	}board_weights;

	struct PlacementWeights {
		double b2b_clear = 250;
		double clear1 = -400;
		double clear2 = -380;
		double clear3 = -350;
		double clear4 = 360;
		double tspin1 = 120;
		double tspin2 = 480;
		double tspin3 = 910;
		double mini_tspin = -120;
		double perfect_clear = 1200;
		double combo_garbage = 250;
		//
		double back_to_back = 150;
		double wasted_t = -350;
		double jeopardy = -400;
		double soft_drop = -10;

		static auto default_args() {
			PlacementWeights p{
				.b2b_clear = 50,
				.clear1 = 50,
				.clear2 = 50,
				.clear3 = 50,
				.clear4 = 50,
				.tspin1 = 50,
				.tspin2 = 50,
				.tspin3 = 50,
				.mini_tspin = 50,
				.perfect_clear = 250,
				.combo_garbage = 50,
				.back_to_back = 50,
				.wasted_t = -50,
				.jeopardy = -50,
				.soft_drop = -5
			};

			return p;
		}

	} placement_weights;

	struct Result {
		int clear = 0;
		int combo = 0;
		bool b2b = false;
		int attack = 0;
		int deaded = 1;
		TSpinType tspin = TSpinType::None;

#ifdef USE_ATOMIC_DEADED
		void trival_attribute(const Result& p) {
			count = p.count;
			clear = p.clear;
			combo = p.combo;
			b2b = p.b2b;
			attack = p.attack;
			tspin = p.tspin;
		}

		Result() noexcept {
			count = {};
			clear = {};
			combo = {};
			b2b = {};
			attack = {};
			deaded = {};
			tspin = {};
		}

		Result(const Result& p) noexcept {
			trival_attribute(p);
			deaded = p.deaded.load();
		}

		Result(Result&& p) noexcept {
			trival_attribute(p);
			deaded.exchange(p.deaded);
		}

		Result& operator=(const Result& p) noexcept {
			if (this != &p) {
				trival_attribute(p);
				deaded = p.deaded.load();
			}
			return *this;
		}

		Result& operator=(Result&& p) noexcept {
			if (this != &p) {
				trival_attribute(p);
				deaded.exchange(p.deaded);
			}
			return *this;
		}
#endif
	};

	struct Evaluation {
		Value eval;
		Reward reward;
	};

	Evaluator(TreeContext* _ctx) :context(_ctx) {}

	std::optional<TetrisNode> tslot_tsd(const TetrisMap& map) {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto h0 = map.top[x] - 1, h1 = map.top[x + 1] - 1, h2 = map.top[x + 2] - 1;
			if (h1 <= h2 - 1 && map(x, h2) && !map(x, h2 + 1) && map(x, h2 + 2)) {
				return std::make_optional<TetrisNode>(Piece::T, x, h2, 2);
			}
		}
		for (auto x = 0; x < map.width - 2; x++) {
			const auto h0 = map.top[x] - 1, h1 = map.top[x + 1] - 1, h2 = map.top[x + 2] - 1;
			if (h1 <= h0 - 1 && map(x + 2, h0) && !map(x + 2, h0 + 1) && map(x + 2, h0 + 2)) {
				return std::make_optional<TetrisNode>(Piece::T, x, h0, 2);
			}
		}
		return std::nullopt;
	}

	std::optional<TetrisNode> tslot_tst(const TetrisMap& map) {
		for (auto x = 0; x < map.width - 2; x++) {
			const auto h0 = map.top[x] - 1, h1 = map.top[x + 1] - 1, h2 = map.top[x + 2] - 1;
			if (h0 <= h1 && h1 < h2) {
				if (map(x - 1, h1 + 1) == map(x - 1, h1 + 2) &&
					!map(x + 1, h1 - 1) &&
					!map(x + 2, h1) && !map(x + 2, h1 - 1) && !map(x + 2, h1 - 2) && !map(x + 2, h1 + 1) && map(x + 2, h1 + 2)
					) {
					return std::make_optional<TetrisNode>(Piece::T, x + 1, h1 - 2, 3);
				}
			}
			else if (h2 <= h1 && h1 < h0) {
				if (map(x + 3, h1 + 1) == map(x + 3, h1 + 2) &&
					!map(x + 1, h1 - 1) &&
					!map(x, h1) && !map(x, h1 - 1) && !map(x, h1 - 2) && !map(x, h1 + 1) && map(x, h1 + 2)
					) {
					return std::make_optional<TetrisNode>(Piece::T, x - 1, h1 - 2, 1);
				}
			}
		}
		return std::nullopt;
	}

	std::optional<TetrisNode> cave_tslot(const TetrisMap& map, const TetrisNode& piece) {
		auto node = piece.drop(map);
		auto x = node.x, y = node.y;
		if (node.rs == 1) {
			if (!map(x, y + 1) && map(x, y) && map(x, y + 2) && map(x + 2, y)) {
				return std::make_optional<TetrisNode>(Piece::T, x, y, 2);
			}
			else if (
				!map(x + 2, y) && !map(x + 3, y) && !map(x + 2, y - 1) &&
				map(x, y + 1) && map(x + 3, y + 1) && map(x + 1, y - 1) && map(x + 3, y - 1)
				) {
				return std::make_optional<TetrisNode>(Piece::T, x + 1, y - 1, 2);
			}
		}
		else if (node.rs == 3) {
			if (!map(x + 2, y + 1) && map(x, y) && map(x + 2, y + 2) && map(x + 2, y)) {
				return std::make_optional<TetrisNode>(Piece::T, x, y, 2);
			}
			else if (
				!map(x, y) && !map(x - 1, y) && !map(x, y - 1) &&
				map(x - 1, y + 1) && map(x - 1, y - 1) && map(x + 1, y - 1) && map(x + 2, y + 1)
				) {
				return std::make_optional<TetrisNode>(Piece::T, x - 1, y - 1, 2);
			}
		}
		return std::nullopt;
	};

	int getComboAtt(int combo) {
		const auto& comboCfg = combo_table;
		return comboCfg[std::min(comboCfg.size() - 1, static_cast<std::size_t>(std::max(0, combo)))];
	}

	Result get(const Result& status, TetrisNode& lp, const TetrisMap& map, int clear, Piece next_node, int upAtt) {
		auto result = status;
		auto safe = getSafe(map, next_node);
		result.clear = clear;
		result.attack = 0;
		//get TSpinType
		if (lp.type == Piece::T && clear > 0 && lp.lastRotate) {
			lp.typeTSpin = lp.spin ? (clear == 1 && lp.mini ? TSpinType::TSpinMini : TSpinType::TSpin) : TSpinType::None;
		}
		result.tspin = lp.typeTSpin;
		result.b2b = (status.b2b && result.clear == 0) || result.clear == 4 || (result.clear != 0 && result.tspin != TSpinType::None);
		result.combo = result.clear > 0 ? result.combo + 1 : 0;
		bool b2b = result.b2b && status.b2b;
		if (result.clear) {
			switch (result.tspin) {
			case TSpinType::None:
				result.attack += clears[clear] + (clear == 4 ? b2b : 0); break;
			default:
				result.attack += clear * 2 - (result.tspin == TSpinType::TSpinMini) + b2b * (clear == 3 ? 2 : 1); break;
			}
			result.attack += getComboAtt(result.combo - 1);
			if (map.count == 0)
				result.attack += 6;
		}
		auto map_rise = result.clear > 0 ? 0 : std::max(0, upAtt);
		result.deaded = safe < 0 ? 0 : (safe - map_rise < 0 ? -1 : 1);
		//result.deaded = 1;
		return result;
	}

	Evaluation evalute(const TetrisNode& node, TetrisMap& _map, Piece hold, std::vector<Piece>* nexts,
		int index, int needSd, Result& status, Result& prev_status) {

		TetrisMap* map_ptr = &_map;
		double transient_eval = 0, acc_eval = 0;
		auto highest_point = _map.roof;

		transient_eval += status.deaded == -1 ? std::numeric_limits<double>::lowest() / 1000 : 0;

		transient_eval += board_weights.height[0] * highest_point;
		transient_eval += board_weights.height[1] * bitwise::max(highest_point - 10, 0);
		transient_eval += board_weights.height[2] * bitwise::max(highest_point - 15, 0);

		auto ts = std::max(std::count(nexts->begin() + index, nexts->end(), Piece::T) + static_cast<int>(hold == Piece::T), 1);

		if (_map.roof >= 15)
			ts = 0;

		if (ts > 0)
			map_ptr = new TetrisMap(_map);

		auto& map = *map_ptr;

#ifndef CLASSIC
		for (auto i = 0; i < ts; i++) {
			auto lines = -1;
			double rate = 0.;
			std::optional<TetrisNode> piece;
			if (piece = tslot_tsd(map)) {
				lines = piece->fillRows(map, piece->x, piece->y, piece->rs);

				//rate = double(BitCount(map[piece->x], map[piece->x + 1]) + 4) / (map.width * 2);

			}
			else if (piece = tslot_tst(map)) {
				auto piece_cave = cave_tslot(map, *piece);
				if (piece_cave) {
					piece = piece_cave;
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);

					//rate = double(BitCount(map[piece->x], map[piece->x + 1]) + 4) / (map.width * 2);
				}
				else if (
					map(piece->x, piece->y) +
					map(piece->x, piece->y + 2) +
					map(piece->x + 2, piece->y) +
					map(piece->x + 2, piece->y + 2) >= 3
					&& !piece->check(map, piece->x, piece->y - 1)) {
					if (auto imperial = *piece; (imperial.rs ^= 2, true) && imperial.check(map) && imperial.getDrop(map) == 0) {
						piece = imperial;
					}
					lines = piece->fillRows(map, piece->x, piece->y, piece->rs);

					//rate = double(BitCount(map[piece->x], map[piece->x + 1], map[piece->x + 2]) + 4) / (map.width * 2);
				}
				else {
					piece = std::nullopt;
				}
			}
			else {
				break;
			}

			switch (lines)
			{
			case 0: [[fallthrough]];
			case 1:
				transient_eval += board_weights.tslot[lines];// *rate;
				goto detect_tspine_out;
			case 2: [[fallthrough]];
			case 3:
				transient_eval += board_weights.tslot[lines];// *rate;
				piece->attach(map);
				break;
			default:
				goto detect_tspine_out;
			}
		}
	detect_tspine_out:
#endif

		if (board_weights.well_depth != 0 || board_weights.hole_depth != 0) { //wells
			auto well_depth = 0, hole_depth = 0;
#define macro_row_bit(v, mask) (((2 << map.width) + 1 | (v) << 1) >> x & mask)
			for (auto x = 0; x < map.width; x++)
				for (auto y = map.roof - 1, cy = map.top[x]; y >= 0; y--) {
					auto c = 0, cu = 0, bits = 0;
					for (auto y1 = y; y1 >= 0; y1--) {
						if (y1 >= cy && macro_row_bit(map[y1], 7) == 5) {
							cu++;
							y--;
						}
						else if (y1 < cy && macro_row_bit(map[y1], 3) >> 1 == 0) {
							c++;
							y--;
						}
						else
							break;
					}
					well_depth += cu;
					hole_depth += c;
				}
#undef marco_row_bit
			transient_eval += board_weights.well_depth * well_depth;
			transient_eval += board_weights.hole_depth * hole_depth;
		}

		/*if (board_weights.bumpiness != 0) {
			auto bumpiness = 0;
			for (auto x = 0; x + 1 < map.width; x++) {
				 auto sq = std::abs(map.top[x] - map.top[x + 1]);
				 bumpiness += sq;
			}
			transient_eval += board_weights.bumpiness * bumpiness;
		}*/

		if (board_weights.row_transition != 0 || board_weights.col_transition != 0)
		{
			int row_trans = 0, col_trans = 0, row_bits = (1 << map.width) - 1;
#define macro_side_fill(v) (v | (1 << map.width - 1))
			for (int y = map.roof - 1; y >= 0; y--) {
				row_trans += BitCount(macro_side_fill(map[y]) ^ macro_side_fill(map[y] >> 1));
				col_trans += BitCount(map[y] ^ map[bitwise::min(y + 1, map.height - 1)]);
				row_trans += ~map(0, y) & 1;
				row_trans += ~map(map.width - 1, y) & 1;
			}
			col_trans += map.roof == map.height ? BitCount(map[map.height - 1] ^ row_bits) : map.width;
			col_trans += BitCount(map[0] ^ row_bits);
			row_trans += 2 * (map.height - map.roof);
#undef marco_side_fill
			transient_eval += board_weights.row_transition * row_trans;
			transient_eval += board_weights.col_transition * col_trans;
		}

		double covered = 0;
		{ //holes
			auto get_coveredcells_on = [&map](int hole_top_y, int hole_top_bits) {
				auto covered = 0, hy = map.roof;
				for (int y = hole_top_y; y < bitwise::min(hy, map.roof); y++) {
					int check = hole_top_bits & map[y];
					if (check == 0)
						break;
					covered += BitCount(check);
				}
				return covered;
				};

			auto holes = 0, cover_bits = 0, hole_lines = 0;
			for (int y = map.roof - 1; y >= 0; y--) {
				cover_bits |= map[y];
				int check = cover_bits ^ map[y];
				if (check != 0) {
					holes += BitCount(check);
					hole_lines++;
					covered += get_coveredcells_on(y + 1, check) * double(y + 1) / map.roof;
				}
			}

			transient_eval += board_weights.hole_lines * hole_lines;
			transient_eval += board_weights.holes * holes;
			transient_eval += board_weights.cells_coveredness * covered;
		}

		if (map_ptr != &_map) {
			delete map_ptr;
		}

#ifndef CLASSIC
		if (status.b2b)
			transient_eval += placement_weights.back_to_back;
#endif

		if (status.clear == 0) {
			double hy = node.mean_y();// _map.roof;
			acc_eval += placement_weights.jeopardy * hy / bitwise::min(20, _map.height);
		}

		//acc_value
		acc_eval += needSd * placement_weights.soft_drop;

		if (node.type == Piece::T && !(/*(ts > 1 && status.combo >= 3) ||*/
			(node.typeTSpin == TSpinType::TSpin && status.clear > 1))) {
			acc_eval += placement_weights.wasted_t;
		}

		if (status.clear) {
			auto comboAtt = getComboAtt(status.combo - 1);

			double comboRatio = 1 + (comboAtt ? std::log1p(static_cast<double>(comboAtt) / status.attack) : 0);
			acc_eval += comboAtt * comboRatio * placement_weights.combo_garbage;

			if (_map.count == 0)
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
			Reward{
#ifdef CLASSIC
			0
#else
			acc_eval
#endif
			, status.clear > 0 ? status.attack : -1}
		};
	}

	int getSafe(const TetrisMap& map, Piece next_node) {
		int safe = std::numeric_limits<int>::max();
		if (next_node != Piece::None) [[unlikely]] {
			safe = context->getNode(next_node).getDrop(map);
			}
		else {
			for (auto type = 0; type < 7; type++) {
				auto& virtualNode = context->getNode(static_cast<Piece>(type));
				auto dropDis = virtualNode.getDrop(map);
				safe = bitwise::min(dropDis, safe);
				if (safe == -1)
					break;
			}
		}
		return safe;
	}
};

//#define USE_MEMORY_BUFFER

template<class TreeNode>
class TreeContext {
	friend TreeNode;
public:
	using Evaluator = Evaluator<TreeContext>;
	using Result = std::tuple<TetrisNode*, bool>;
	using NodeDeleter = std::function<void(TreeNode*)>;

private:
#ifdef USE_MEMORY_BUFFER
	//memory resource pool
	std::size_t nodes_reserved = 100000;
	std::vector<std::byte> buf;
	std::pmr::monotonic_buffer_resource res;
#endif
	std::pmr::synchronized_pool_resource pool;
	std::pmr::polymorphic_allocator<TreeNode> alloc{ &pool };

	std::shared_mutex mtx;
	std::vector<TetrisNode> nodes;

public:
	std::unique_ptr<TreeNode, NodeDeleter> root;
	Evaluator evalutator{ this };
	bool isOpenHold = true;
	std::atomic<int> count = 0;
	std::vector<Piece> nexts;
	std::string info;
	std::vector<std::unique_ptr<lock_free_stack<TreeNode*>>> deads;

	TreeContext(bool delete_byself = false) :
		root(nullptr, delete_byself ? TreeContext::treeDelete_donothing : get_delefn())
	{
		for (int i = 0; i < 2; i++)
			deads.emplace_back(new lock_free_stack<TreeNode*>);
#ifdef USE_MEMORY_BUFFER
		buf.resize(sizeof(TreeNode) * nodes_reserved);
		std::destroy_at(&res);
		std::destroy_at(&pool);
		std::destroy_at(&alloc);
		new (&res) std::pmr::monotonic_buffer_resource{ buf.data(), buf.size() };
		new (&pool) std::pmr::synchronized_pool_resource{ &res };
		new (&alloc) std::pmr::polymorphic_allocator<TreeNode>{ &pool };
#endif
	};

	~TreeContext() {
		root.get_deleter() = get_delefn();
		root.reset(nullptr);
		return;
	}

	auto& get_alloc() {
		return alloc;
	}

	NodeDeleter get_delefn() {
		return std::bind(TreeContext::treeDelete, this, std::placeholders::_1);
	}

	template<class... Args>
	TreeNode* alloc_node(Args&&... args) {
		TreeNode* ptr = get_alloc().template new_object<TreeNode>(std::forward<Args>(args)...);
		return ptr;
	}

	void dealloc_node(TreeNode* ptr, int type = 0) {
		get_alloc().delete_object(ptr);
		return;
	}

	void save(TreeNode* t, bool used) {
		{
			auto& dead = this->deads.front();
			while (!dead->empty()) {
				if (auto node = dead->pop(); node) {
					this->dealloc_node(*node);
				}
			}
		}
		while (true) {
			auto tree = t->list.pop();
			if (!tree)
				break;
			for (auto p : (*tree)->children) {
				if (p == nullptr)
					continue;
				if (p->cur == Piece::None) {
					auto k = nexts.size() - 1 - used;
					p->cur = nexts[k];
				}
			}
		}
	}

	using SaveFnCaller = self_args_func_t<&save>;

	template<class F = SaveFnCaller>
	TreeNode* createRoot(const TetrisNode& node, const TetrisMap& map, std::vector<Piece>& dp, Piece hold, bool canHold,
		int b2b, int combo, int dySpawn, F&& fn = [](auto...args) { return std::invoke(args...); }) {
		count = 0;
		auto queue = nexts;
		nexts = dp;
		nodes.clear();
		for (auto i = 0; i < 7; i++)
			nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i), &map, dySpawn));
		TreeNode* tree = alloc_node();
		{//init for root
			tree->context = this;
			tree->cur = node.type;
			tree->map = map;
			tree->hold = hold;
			tree->isHoldLock = !canHold;
			std::memset(&tree->props, 0, sizeof tree->props);
			tree->props.status.deaded = 1;
			tree->props.status.b2b = b2b;
			tree->props.status.combo = combo;
			tree->props.land_node.type = Piece::None;
			tree->props.land_node.typeTSpin = TSpinType::None;
		}
		return update(tree, queue, fn);
	}

	void debug_print(const TetrisMap& map, Piece cur, std::vector<Piece>& nexts, Piece hold) {
		std::ostringstream os;
		os << "{\n";
		os << " //TetrisMap map(" << map.width << ", " << map.height << ");\n";
		os << " map[{0}] = {";
		for (auto i = 0; i < map.roof; i++) {
			if (i != 0) {
				os << ", ";
			}
			os << map.data[i];
		}
		os << "};\n";
		os << " tn = TetrisNode::spawn(Piece::" << cur << ", &map, dySpawn);\n";
		os << " rS.displayBag = {";
		for (auto i = 0; i < nexts.size(); i++) {
			if (i != 0) {
				os << ", ";
			}
			os << "Piece::" << nexts[i];
		}
		os << "};\n";
		os << " hS.type = Piece::";
		if (hold == Piece::None) {
			os << "None";
		}
		else {
			os << hold;
		}
		os << ";\n";
		os << "}\n\n";
		std::cout << os.str();
	}

	void run(int upAtt, int thread_idx = 0) {
		using rand_type = std::mt19937; //std::minstd_rand;
#define init_seed() (static_cast<rand_type::result_type>(std::chrono::system_clock::now().time_since_epoch().count()) \
		^ std::hash<std::thread::id>{}(std::this_thread::get_id()))
		static thread_local rand_type rng(init_seed());
#undef init_seed
		static thread_local std::uniform_real_distribution<> d(0, 1);
		static thread_local const double exploration = std::log(2);
		TreeNode* branch = root.get();
#ifdef USE_SERIS
#define select(vec) vec[static_cast<std::size_t>(std::fmod(-std::log(1 - d(rng)) / exploration, vec.size()))];
		int i = 0, m = 0;
		TreeNode* apex = branch;
		while (branch->cur != Piece::None) {
			if (branch->leaf)
			{
				branch->extend(apex, i, upAtt);
			}
			{
				std::shared_lock guard{ branch->mtx };
				const auto& branches = branch->children;
				if (!branches.empty()) {
					branch = select(branch->children);
					upAtt -= branch->props.status.attack;
					++i += (branch->parent->hold == Piece::None && branch->hold != Piece::None);
					switch (m) {
					case 0: apex = branch; m = 1; break;
					case 1: break;
					}
				}
				else {
					break;
				}
			}
		}
		//std::cout << "complete in a turn;\n";
#else

#define select(vec) vec[static_cast<std::size_t>(std::fmod(-std::log(1 - d(rng)) / exploration, vec.size()))];
		int i = 0, flag = 0;
		TreeNode* apex = branch;
		while (true) {
			std::scoped_lock guard(branch->mtx);
			std::span childs(branch->children.begin(), branch->children.end());
			if (branch->leaf || childs.empty())
				break;
#ifdef USE_TEST_CHILDREN_SORTED
			if (!std::is_sorted(branch->children.begin(), branch->children.end(), TreeNode::TreeNodeCompare)) {
				std::osyncstream(std::cout) << branch << "\n";
				exit(-1);
			}
#endif
			branch = select(childs);
			upAtt -= branch->props.status.attack;
			++i += (branch->parent->hold == Piece::None && branch->hold != Piece::None);
			switch (flag) {
			case 0: apex = branch; flag = 1; break;
			default: break;
			}
		}
		branch->extend(apex, i, upAtt);
#endif
#undef select
	}

	template<class F>
	TreeNode* update(TreeNode* node, std::vector<Piece>& queue, F&& outer_call) {
		TreeNode* last = nullptr;
		count = 0;
		info.clear();
		if (root == nullptr) {
			root.reset(node);
			goto end;
		}
		else if (*node == *root.get()) {
			info = "undo+";
			nexts.swap(queue);
			dealloc_node(node);
			goto end;
		}
		last = root.get();
		if (auto it = std::find_if(root->children.begin(), root->children.end(), [&](auto val) {return *node == *val; });
			it != root->children.end()) {
			info = "found";
			auto selected = *it;
			auto used_flag = selected->parent->hold == Piece::None && selected->hold != Piece::None;
			outer_call(&TreeContext::save, this, selected, used_flag);
			selected->parent = nullptr;
			root->children.erase(it);
			root.reset(selected);
			dealloc_node(node);
			node = nullptr;
			goto end;
		}
		else {
			info = "not found";
			root.reset(node);
			goto end;
		}
	end:
		deads.front().swap(deads.back());
		return last;
	}

	void print_TreeNode(TreeNode* t) {
		if (t->cur) {
			std::cout << *t->cur;
		}
		else {
			std::cout << " ";
		}
		std::cout << "[";
		if (t->hold) {
			std::cout << *t->hold;
		}
		else {
			std::cout << " ";
		}
		std::cout << "] land:" << t->props.land_node << "\n";
	}

	Result empty() {
		return { nullptr, false };
	}

	void test_sorted_all(TreeNode* _t) {
		auto test_sorted = [](auto branch) {
			if (!std::is_sorted(branch->children.begin(), branch->children.end(), TreeNode::TreeNodeCompare)) {
				std::osyncstream(std::cout) << branch << "\n";
				exit(-1);
			}
		};
		trivail_vector<TreeNode*> queue;
		queue.emplace_back(_t);
		for (std::size_t i = 0; i < queue.size(); i++) {
			auto t = queue[i];
			if (t == nullptr)
				continue;
			else {
				for (auto v : t->children) {
					queue.emplace_back(v);
				}
			}
			test_sorted(t);
		}
		std::cout << queue.size() << " node:counts:" << count << "\n";
	}

	Result getBest(int upAtt) {
		//test_sorted_all(root.get());
		return root->pick(upAtt);
	}

	auto& getNode(Piece type) {
		return nodes[static_cast<int>(type)];
	}

	static void treeDelete(TreeContext* pThis, TreeNode* _t) {
		if (_t != nullptr) {
			trivail_vector<TreeNode*> queue;
			queue.emplace_back(_t);
			for (std::size_t i = 0; i < queue.size(); i++) {
				auto t = queue[i];
				if (t == nullptr)
					continue;
				else {
					for (auto v : t->children) {
						queue.emplace_back(v);
					}
				}
				pThis->dealloc_node(t);
			}
		}
	}

	static void treeDelete_donothing(TreeNode* t) {
	}
};

class TreeNode {
	using TreeContext = TreeContext<TreeNode>;
	using Evaluator = Evaluator<TreeContext>;
	friend TreeContext;

public:
	struct EvalArgs {
		TetrisNode land_node;
		std::size_t clear = 0;
		Evaluator::Result status;
		Evaluator::Evaluation raw;
		Evaluator::Value evaluation;
		int needSd = 0;

		bool operator>(const EvalArgs& other) const {
			return this->evaluation.value > other.evaluation.value;
		}
	};

	constexpr static struct {
		bool operator()(TreeNode* const& lhs, TreeNode* const& rhs) const {
			return lhs->props > rhs->props;
#define USE_BRANCH_LESS
#ifndef USE_BRANCH_LESS
			auto a = lhs->leaf || !lhs->children.empty(); //branch[false] -> true/false
			auto b = rhs->leaf || !rhs->children.empty(); //leaf[true] -> false
			if (a == b) [[likely]]
				return lhs->props > rhs->props;
			else [[unlikely]]
				return a > b;
#else
			auto a = lhs->leaf || !lhs->children.empty(); //branch[false] -> true/false
			auto b = rhs->leaf || !rhs->children.empty(); //leaf[true] -> false
			int tag = a == b, se0 = lhs->props > rhs->props, se1 = a > b;
			return (tag & se0) | (~tag & se1);
#endif
#undef USE_BRANCH_LESS
		}
	}TreeNodeCompare{};

private:
	TreeNode* parent = nullptr;
	TreeContext* context = nullptr;
	TetrisMap map;
	std::shared_mutex mtx;
	bool leaf = true;
	std::atomic<bool> occupied = false;
	bool isHold = false, isHoldLock = false;
	Piece cur = Piece::None, hold = Piece::None;
	EvalArgs props;
	std::vector<TreeNode*> children;
	lock_free_stack<TreeNode*> list;
public:
	TreeNode() {}

	template<class TetrisMap>
	TreeNode(TreeContext* _ctx, TreeNode* _parent, Piece _cur, TetrisMap&& _map, Piece _hold, bool _isHold, const EvalArgs& _props) :
		parent(_parent), context(_ctx), map(std::forward<TetrisMap>(_map)), isHold(_isHold), cur(_cur), hold(_hold), props(_props) {
	}

	bool operator==(TreeNode& p) {
		return cur == p.cur &&
			map == p.map &&
			hold == p.hold &&
			props.status.b2b == p.props.status.b2b &&
			props.status.combo == p.props.status.combo;
	}

	auto branches() {
		std::vector<TreeNode*> arr;
		for (auto& it : children) {
			if (it->leaf) arr.push_back(it);
		}
		return arr;
	}

	template<class T, class...Ts>
	TreeNode* generateChildNodeAll(T&& nodes, Ts&&...ts) {
		for (auto& node : nodes) {
			generateChildNode(node, std::forward<Ts>(ts)...);
		}
		return nullptr;
	}

	TreeNode* generateChildNode(TetrisNode& land_node, bool _isHoldLock, Piece _hold, bool _isHold, int index, int upAtt) {
		auto next_node = (index >= context->nexts.size() ? Piece::None : context->nexts[index]);
		auto map_ = map;
		auto clear = land_node.attach(map_);
		auto status = context->evalutator.get(props.status, land_node, map_, clear, next_node, upAtt);
		TreeNode* new_tree = nullptr;
		if (status.deaded != 0) {
			auto needSd = land_node.type != Piece::T ? land_node.step : (land_node.typeTSpin == TSpinType::None ? land_node.step : 0);
			auto raw = context->evalutator.evalute(land_node, map_, _hold, &context->nexts, index, needSd, status, props.status);
			EvalArgs props_{
				land_node,
				clear,
				status,
				raw,
				raw.eval + raw.reward,
				needSd
			};
			context->count++;
			new_tree = context->alloc_node(context, this, next_node, std::move(map_), _hold, _isHold, props_);
			new_tree->isHoldLock = _isHoldLock;
			{
				std::scoped_lock guard(mtx);
				children.push_back(new_tree);
			}
		}
		return new_tree;
	}

	bool search(int ndx, int upAtt) {
		if (context->isOpenHold && !isHoldLock) {
			return search_hold(ndx, upAtt);
		}
		generateChildNodeAll(Search::search(context->getNode(cur), map),
			false, hold, bool(isHold), ndx, upAtt);
		return ndx == context->nexts.size();
	}

	template<size_t select = 0>
	bool search_hold(int ndx, int upAtt) {
		if constexpr (select == 0) {
			if (hold == Piece::None && context->nexts.size() > 0) return search_hold<1>(ndx, upAtt);
			else return search_hold<2>(ndx, upAtt);
		}
		else if constexpr (select == 1) { //hold is none
			if (cur != Piece::None) {
				generateChildNodeAll(Search::search(context->getNode(cur), map),
					false, hold, false, ndx, upAtt);
			}
			if (std::size_t after = ndx; hold == Piece::None && after < context->nexts.size()) {
				auto take = context->nexts[after];
				if (take != cur) {
					generateChildNodeAll(Search::search(context->getNode(take), map),
						false, cur, true, ndx + 1, upAtt);
				}
			}
		}
		else if constexpr (select == 2) { //has hold
			switch (cur == hold) {
			case false:
				generateChildNodeAll(Search::search(context->getNode(hold), map),
					false, cur, true, ndx, upAtt);
				[[fallthrough]];
			case true:
				generateChildNodeAll(Search::search(context->getNode(cur), map),
					false, hold, false, ndx, upAtt);
				break;
			}
		}
		return ndx == context->nexts.size();
	}


	static void modify_sorted_batch(std::vector<TreeNode*>& s, TreeNode* update_one, bool flag) {
		auto pos = std::find(s.begin(), s.end(), update_one);
		auto n = std::distance(s.begin(), pos);
		if (flag) {
			for (auto i = n; i + 1 < s.size(); ++i) {
				if (TreeNode::TreeNodeCompare(s[i + 1], s[i]))
					std::swap(s[i], s[i + 1]);
				else
					break;
			}
		}
		else {
			for (auto i = n; i > 0; --i) {
				if (TreeNode::TreeNodeCompare(s[i], s[i - 1]))
					std::swap(s[i], s[i - 1]);
				else
					break;
			}
		}
		return;
	}

#ifdef TEST_UNIT_GG_EXIST
	struct GG {
		std::atomic<bool>& target;
		GG(std::atomic<bool>& _target) : target(_target) { target = true; }
		~GG() { target = false; }
	};
#endif


	void repropagate(TreeNode* from) {
		if (!from)
			return;
		auto it = from;
		for (std::unique_lock base(it->mtx); it; it = it->parent) {
			if (it->children.empty())
				continue;
			pdqsort_branchless(it->children.begin(), it->children.end(), TreeNode::TreeNodeCompare);
			auto improved = Evaluator::Value::lowest();
			for (auto c = it->children.begin(); c != it->children.end(); ++c)
				improved.improve((*c)->props.evaluation + it->props.raw.reward);
			if (it->props.evaluation != improved || it == from) {
				std::unique_lock guard(it->parent ? it->parent->mtx : context->mtx);
				it->props.evaluation = improved;
				base = std::move(guard);
			}
			else {
				break;
			}
		}
	}

	void prune_death_path(TreeNode*& it) {
		for (std::unique_lock base(it->mtx); it; it = it->parent) {
			std::unique_lock guard(it->parent ? it->parent->mtx : it->context->mtx);
			if (it->parent) {
				std::erase(it->parent->children, it);
				/*if (it->parent->children.empty()) {
					std::cout << "parent:addr(" << it->parent << ")->children.empty() is true\n" << it->parent->map << "\n";
				}*/
				it->context->count--;
				it->context->deads.back()->push(it);
				if (it->parent->children.empty()) {
					base = std::move(guard);
				}
				else {
					it = it->parent;
					break;
				}
			}
		}
	}

	bool extend(TreeNode* apex, int ndx, int upAtt) {
		bool result = false, expected = false;
		while (!occupied.compare_exchange_weak(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
			if (expected) {
				//occupied.wait(true);
				std::this_thread::yield();
				return false;
			}
		}
		if (auto it = this; leaf && cur != Piece::None) {
			if (auto tail = search(ndx, upAtt); true) {
				if (children.empty()) {
					prune_death_path(it);
				}
				else if (tail)
					apex->list.push(it);
			}
			result = true;
			repropagate(it);
			leaf = false;
			goto out;
		}
		else {
			goto out;
		}
	out:
		occupied.store(false, std::memory_order_release);
		//occupied.notify_all();
		return result;
	}

	void show_plan(TreeNode* tree) {
		while (true) {
			if (tree->children.empty())
				return;
			tree = tree->children.front();
			std::cout << tree->map << "\n";
		}
	}

	void show_children_maps(TreeNode* tree) {
		for (auto i = 0; i < tree->children.size(); i++) {
			std::cout << "\n" << tree->children[i]->map
				<< "index:" << i
				<< "raw{" << tree->children[i]->props.raw.eval.value << " , " << tree->children[i]->props.raw.reward.value << "}\n";
		}
	}

	int depth(TreeNode* tree) {
		auto nth = 0;
		while (true) {
			if (tree->children.empty())
				return nth;
			tree = tree->children.front();
			nth++;
		}
	}

	TreeContext::Result pick(int upAtt) {
		TreeNode* backup = nullptr;

		for (auto mv : std::span{ children.begin(), children.size()}) {
			if (upAtt == 0 || std::all_of(mv->map.top + 3, mv->map.top + 7, [&](auto h) {
				return upAtt - mv->props.status.attack + h <= 20;
				})) {
				backup = mv;
				break;
			}
			if (backup == nullptr)
				backup = mv;
			else if (prefer_select(backup, mv))
				backup = mv;
		}

		if (backup == nullptr) {
			//context->debug_print(map, cur, context->nexts, hold);
			return context->empty();
		}
		/*else {
			std::cout << "Ã»ËÀ " << "leaf:" << std::boolalpha << backup->leaf << " children.size():" << backup->children.size() << "\n";
		}*/
		return { &backup->props.land_node, backup->isHold };
	}

	static bool prefer_select(const TreeNode* lhs, const TreeNode* rhs) {
		if (lhs->props.evaluation.spike == 0 && rhs->props.evaluation.spike == 0) {
			return lhs->props.raw.reward.attack < rhs->props.raw.reward.attack;
		}
		else if (lhs->props.evaluation.spike < rhs->props.evaluation.spike) {
			return true;
		}
		else
			return false;
	}
};


class TetrisDelegator {
public:
	using TreeContext_t = TreeContext<TreeNode>;

	struct GameState {
		int dySpawn;
		int upAtt;
		type_all_t<std::vector<Piece>> bag;
		type_all_t<TetrisNode> cur;
		type_all_t<TetrisMap> field;
		bool canHold;
		Piece hold;
		bool b2b;
		std::size_t combo;
		bool path_sd;

		GameState& operator=(const GameState& p) {
			dySpawn = p.dySpawn;
			upAtt = p.upAtt;
			bag = p.bag;
			cur = p.cur;
			field = p.field;
			canHold = p.canHold;
			hold = p.hold;
			b2b = p.b2b;
			combo = p.combo;
			path_sd = p.path_sd;
			return *this;
		}
	};

	struct OutInfo {
		std::optional<TetrisNode> migrate;
		std::optional<TetrisNode> landpoint;
		std::string info;
		std::size_t nodeCounts;
		int pathCost;
	};

	struct Config {
		bool use_static = false;
		bool delete_byself = true;
		unsigned int thread_num = 1;
	};

	struct EvalArgs {
		using Eval = Evaluator<TreeContext_t>;
		Eval::BoardWeights board_weights;
		Eval::PlacementWeights placement_weights;

		static EvalArgs default_args() {
			return{
				Eval::BoardWeights::default_args(),
				Eval::PlacementWeights::default_args()
			};
		}
	};

private:
	Config cfg;
	std::optional<EvalArgs> eval_args;
	std::unique_ptr<TreeContext_t> context;
	std::unique_ptr<dp::thread_pool<>> thread_pool;
	bool eval_args_freshed;

	struct Task_manage {
		std::atomic<bool> complete;
		std::atomic<unsigned int> count;

		Task_manage(unsigned int task_count) :complete(false), count(task_count) {}

		void arrive() {
			--count;
			if (count.load() == 0) {
				complete = true;
				complete.notify_one();
			}
		}

		void wait() {
			complete.wait(false);
		}
	};

	TetrisDelegator(const Config& _cfg, const std::optional<EvalArgs>& _eval_args = EvalArgs{}) :
		cfg(_cfg),
		eval_args(_eval_args),
		context(std::make_unique<TreeContext<TreeNode>>(cfg.delete_byself)),
		thread_pool(std::make_unique<dp::thread_pool<>>(cfg.thread_num)),
		eval_args_freshed(true)
	{}

public:
	TetrisDelegator() noexcept = delete;
	TetrisDelegator(const TetrisDelegator&) noexcept = delete;
	TetrisDelegator& operator=(const TetrisDelegator& p) noexcept = delete;

	TetrisDelegator(TetrisDelegator&& p) noexcept {
		cfg = p.cfg;
		eval_args.swap(p.eval_args);
		context.swap(p.context);
		thread_pool.swap(p.thread_pool);
		eval_args_freshed = p.eval_args_freshed;
	}

	TetrisDelegator& operator=(TetrisDelegator&& p) noexcept {
		cfg = p.cfg;
		eval_args.swap(p.eval_args);
		context.swap(p.context);
		thread_pool.swap(p.thread_pool);
		eval_args_freshed = p.eval_args_freshed;
		return *this;
	}

	static TetrisDelegator launch(const Config& cfg = { .use_static = true, .delete_byself = true, .thread_num = 1 }) {
		return TetrisDelegator{ cfg };
	}

	void set_thread_num(unsigned int _thread_num) {
		cfg.thread_num = _thread_num;
		thread_pool = std::make_unique<dp::thread_pool<>>(cfg.thread_num);
	}

	void set_use_static(bool _use_static) {
		cfg.use_static = _use_static;
	}

	void set_delete_byself(bool _delete_byself) {
		cfg.delete_byself = _delete_byself;
		context->root.get_deleter() = cfg.delete_byself ? TreeContext_t::treeDelete_donothing : context->get_delefn();
	}

	void set_board_weights(const EvalArgs& _eval_args) {
		eval_args = _eval_args;
		eval_args_freshed = true;
	}

	void run(const GameState& state, int limitTime) {
		auto& [dySpawn, upAtt, bag, cur, field, canHold, hold, b2b, combo, path_sd] = state;
		auto& [use_static, delete_byself, thread_num] = cfg;

		auto reconstruct = false;

#ifdef  CLASSIC
		auto dp = std::vector<Piece>();
		context->isOpenHold = false;
#else
		auto dp = std::vector(bag->begin(), bag->end());
#endif

		if (!use_static) {
			context = std::make_unique<TreeContext<TreeNode>>(delete_byself);
			eval_args_freshed = true;
		}

		if (eval_args_freshed) {
			eval_args_freshed = false;
			if (eval_args) {
				context->evalutator.board_weights = eval_args->board_weights;
				context->evalutator.placement_weights = eval_args->placement_weights;
			}
		}

		auto out_exec = [&](auto fn_p, auto ...args) {
			Task_manage receiver(thread_num);
			for (auto i = 0u; i < thread_num; i++) {
				thread_pool->enqueue_detach([&]() {
					std::invoke(fn_p, args...);
					receiver.arrive();
					});
			}
			receiver.wait();
		};

		auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(limitTime);
		auto last = context->createRoot(cur, field, dp, hold, canHold, b2b, combo, dySpawn, out_exec);

		{
			Task_manage receiver(thread_num);
			auto taskrun = [&](TreeContext_t* context, std::size_t index) {
#ifndef  CLASSIC
				do {
#endif
					context->run(upAtt, index);
#ifndef  CLASSIC
				} while (std::chrono::steady_clock::now() < end);
#endif
				receiver.arrive();
			};

			for (auto i = 0u; i < thread_num; i++) {
				thread_pool->enqueue_detach(taskrun, context.get(), i);
			}

			//taskrun(context.get(), 0);

			if (delete_byself)
				std::invoke(TreeContext_t::treeDelete, context.get(), last);

			receiver.wait();
		}
		return;
	}

	std::optional<std::vector<Oper>> suggest(const GameState& state, OutInfo& out_info) {
		auto& [dySpawn, upAtt, bag, cur, field, canHold, hold, b2b, combo, path_sd] = state;
		auto& [migrate, landpoint, info, nodeCounts, pathCost] = out_info;

		auto [best, ishold] = context->getBest(upAtt);
		std::optional<std::vector<Oper>> path = std::nullopt;

#ifdef TIMER_MEASURE
		Timer timer;
#endif
		if (!best) {
			landpoint = std::nullopt;
			goto end;
		}
		if (!ishold) {
			migrate = cur, landpoint = *best;
			//#ifndef  CLASSIC
#ifdef TIMER_MEASURE
			timer.reset();
#endif
			path = Search::make_path(cur, *best, field, path_sd);
#ifdef TIMER_MEASURE
			pathCost = timer.elapsed<Timer::us>();
#endif
			//#endif
		}
		else {
			auto holdNode = TetrisNode::spawn(hold != Piece::None ? hold : bag()[0], &field(), dySpawn);
			migrate = holdNode, landpoint = *best;
			//#ifndef  CLASSIC
#ifdef TIMER_MEASURE
			timer.reset();
#endif
			path = Search::make_path(holdNode, *best, field, path_sd);
#ifdef TIMER_MEASURE
			pathCost = timer.elapsed<Timer::us>();
#endif
			path->insert(path->begin(), Oper::Hold);
			//#endif
		}
	end:
		nodeCounts = context->count;
		info = context->info;
		return path;
	}
};