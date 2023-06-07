#pragma once
#include"TetrisBase.h"
#include"pdqsort.h"
#include<random>
#include<algorithm>
#include<functional>
#include<mutex>
#include<thread>
#include<atomic>
#include<condition_variable>
#include<queue>

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

	struct Result {
		double value;
		int clear;
		int count;
		double t2_value;
		double t3_value;
		TetrisMap* map;
		TSpinType typeTSpin;
	};

	struct Status {
		int max_combo;
		int max_attack;
		int death;
		int combo;
		int attack;
		int under_attack;
		int map_rise;
		bool b2b;
		double like;
		double value;
		bool deaded;
		bool operator<(const Status& other) const {
			return this->value < other.value;
		}

		bool operator>(const Status& other) const {
			return this->value > other.value;
		}
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

	Result evaluate(TetrisNode& lp, TetrisMap& map, int clear) {
		const int width_m1 = map.width - 1;
		int ColTrans = 2 * (map.height - map.roof);
		int RowTrans = map.roof == map.height ? 0 : map.width;

		for (int y = 0; y < map.roof; ++y) {
			if (!map(0, y))
				++ColTrans;
			if (!map(width_m1, y)) {
				++ColTrans;
			}
			ColTrans += BitCount((map[y] ^ (map[y] << 1)) & col_mask_);
			if (y != 0) {
				RowTrans += BitCount(map[y - 1] ^ map[y]);
			}
		}
		RowTrans += BitCount(row_mask_ & ~map[0]);
		if (map.roof > 0)
			RowTrans += BitCount(map.roof == map.height ? row_mask_ & ~map[map.roof - 1]
				: map[map.roof - 1]);

		struct {
			int HoleCount;
			int HoleLine;
			int HoleDepth;
			int WellDepth;
			int HoleNum[32];
			int WellNum[32];
			int LineCoverBits;
			int HolePosyIndex;
		} v;
		std::memset(&v, 0, sizeof v);
		struct {
			int ClearWidth;
		} a[40];

		std::memset(a, 0, sizeof(a));

		for (int y = map.roof - 1; y >= 0; --y) {
			v.LineCoverBits |= map[y];
			auto LineHole = v.LineCoverBits ^ map[y];
			if (LineHole != 0) {
				++v.HoleLine;
				a[v.HolePosyIndex].ClearWidth = 0;
				for (auto hy = y + 1; hy < map.roof; ++hy) {
					auto CheckLine = LineHole & map[hy];
					if (CheckLine == 0) break;
					a[v.HolePosyIndex].ClearWidth += (hy + 1) * BitCount(CheckLine);
				}
				++v.HolePosyIndex;
			}
			for (int x = 1; x < width_m1; ++x) {
				if ((LineHole >> x) & 1) {
					v.HoleDepth += ++v.HoleNum[x];
				}
				else {
					v.HoleNum[x] = 0;
				}
				if (((v.LineCoverBits >> (x - 1)) & 7) == 5) {
					v.WellDepth += ++v.WellNum[x];
				}
			}
			if (LineHole & 1) {
				v.HoleDepth += ++v.HoleNum[0];
			}
			else {
				v.HoleNum[0] = 0;
			}
			if ((v.LineCoverBits & 3) == 2) {
				v.WellDepth += ++v.WellNum[0];
			}
			if ((LineHole >> width_m1) & 1) {
				v.HoleDepth += ++v.HoleNum[width_m1];
			}
			else {
				v.HoleNum[width_m1] = 0;
			}
			if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1) {
				v.WellDepth += ++v.WellNum[width_m1];
			}
		}

		Result result;
		result.value = (0. - map.roof * 128 - ColTrans * 160 - RowTrans * 160 -
			v.HoleCount * 80 - v.HoleLine * 380 - v.WellDepth * 100 -
			v.HoleDepth * 40);

		double rate = 32, mul = 1.0 / 4;
		for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul) {
			result.value -= a[i].ClearWidth * rate;
		}

		result.count = map.count + v.HoleCount;
		result.clear = clear;
		result.t2_value = 0;
		result.t3_value = 0;
		result.map = &map;

#define USE_CC_DETECT_SLOT 0
#if USE_CC_DETECT_SLOT == 1
		auto& T = context->getNode(Piece::T);
		for (auto x = 0; x < 8; x++) {
			const auto& [h0, h1, h2] = std::tuple<int, int, int>{
				map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
			if (h0 >= 2 && h2 == h0 - 2 && h1 < h2) {
				if (map(x, h0) && !map(x, h0 - 1) &&
					map(x, h0 - 2)) { /*T.attach(map, x, h2, 2) **/
					double t2_value = 0;
					int fit = T.fillRows(map, x, h2, 2);
					double bits =
						double(BitCount(map[h0 - 2], map[h0 - 1]) + 4) / map.width;
					t2_value = bits + 2 * bits * (fit >> 1);
					result.t2_value = std::max(result.t2_value, t2_value);
					if (fit == 2)
						break;
				}
			}
			else if (h2 >= 2 && h0 == h2 - 2 && h1 < h0) {
				if (map(x + 2, h2) && !map(x + 2, h2 - 1) &&
					map(x + 2, h2 - 2)) { /*T.attach(map, x, h0, 2) **/
					double t2_value = 0;
					int fit = T.fillRows(map, x, h0, 2);
					double bits =
						double(BitCount(map[h2 - 2], map[h2 - 1]) + 4) / map.width;
					t2_value = bits + 2 * bits * (fit >> 1);
					result.t2_value = std::max(result.t2_value, t2_value);
					if (fit == 2)
						break;
				}
			}
		}
#else
		bool finding2 = true;
		bool finding3 = true;
		for (int y = 0; (finding2 || finding3) && y < map.roof - 2; ++y) {
			int row0 = map[y];
			int row1 = y + 1 < map.height ? map[y + 1] : 0;
			int row2 = y + 2 < map.height ? map[y + 2] : 0;
			int row3 = y + 3 < map.height ? map[y + 3] : 0;
			int row4 = y + 4 < map.height ? map[y + 4] : 0;
			for (int x = 0; finding2 && x < map.width - 2; ++x) {
				if (((row0 >> x) & 7) == 5 && ((row1 >> x) & 7) == 0) {
					if (BitCount(row0) == map.width - 1) {
						result.t2_value += 2;
						if (BitCount(row1) == map.width - 3) {
							result.t2_value += 2;
							int row2_check = (row2 >> x) & 7;
							if (row2_check == 1 || row2_check == 4) {
								result.t2_value += 2;
							}
							finding2 = false;
						}
					}
				}
			}
			for (int x = 0; finding3 && x < map.width - 3; ++x) {
				if (((row0 >> x) & 15) == 11 && ((row1 >> x) & 15) == 9) {
					int t3_value = 0;
					if (BitCount(row0) == map.width - 1) {
						t3_value += 1;
						if (BitCount(row1) == map.width - 2) {
							t3_value += 1;
							if (((row2 >> x) & 15) == 11) {
								t3_value += 2;
								if (BitCount(row2) == map.width - 1) {
									t3_value += 2;
								}
							}
							int row3_check = ((row3 >> x) & 15);
							if (row3_check == 8 || row3_check == 0) {
								t3_value += !!row3_check;
								int row4_check = ((row4 >> x) & 15);
								if (row4_check == 4 || row4_check == 12) {
									t3_value += 1;
								}
								else {
									t3_value -= 2;
								}
							}
							else {
								t3_value = 0;
							}
						}
					}
					result.t3_value += t3_value;
					if (t3_value > 3) {
						finding3 = false;
					}
				}
				if (((row0 >> x) & 15) == 13 && ((row1 >> x) & 15) == 9) {
					int t3_value = 0;
					if (BitCount(row0) == map.width - 1) {
						t3_value += 1;
						if (BitCount(row1) == map.width - 2) {
							t3_value += 1;
							if (((row2 >> x) & 15) == 13) {
								t3_value += 2;
								if (BitCount(row2) == map.width - 1) {
									t3_value += 2;
								}
							}
							int row3_check = ((row3 >> x) & 15);
							if (row3_check == 1 || row3_check == 0) {
								t3_value += !!row3_check;
								int row4_check = ((row4 >> x) & 15);
								if (row4_check == 3 || row4_check == 1) {
									t3_value += 1;
								}
								else {
									t3_value -= 2;
								}
							}
							else {
								t3_value = 0;
							}
						}
					}
					result.t3_value += t3_value;
					if (t3_value > 3) {
						finding3 = false;
					}
				}
			}
		}
#endif

		if (lp.type == Piece::T && clear > 0 && lp.lastRotate) {
			lp.typeTSpin = lp.spin ? (clear == 1 && lp.mini ? TSpinType::TSpinMini
				: TSpinType::TSpin)
				: TSpinType::None;
		}

		result.typeTSpin = lp.typeTSpin;
		return result;
	}

	Status get(const Result& eval_result, Status& status, const std::optional<Piece>& hold,
		std::vector<Piece>* nexts, int depth, int needSd) {
		Status result = status;
		result.value = eval_result.value;
		int safe = getSafe(*eval_result.map);
		int table_max = combo_table.size();

		if (safe < 0) {
			result.deaded = true;
			return result;
		}

		switch (eval_result.clear) {
		case 0:
			if (status.combo > 0 && status.combo < 3) {
				result.like -= 2;
			}
			result.combo = 0;
			if (status.under_attack > 0) {
				result.map_rise += std::max(0, int(status.under_attack) - status.attack);
				if (result.map_rise > safe) {
					result.death += result.map_rise - safe;
					result.deaded = true;
					return result;
				}
				result.under_attack = 0;
			}
			break;
		case 1:
			if (eval_result.typeTSpin == TSpinType::TSpinMini) {
				result.attack += status.b2b ? 2 : 1;
			}
			else if (eval_result.typeTSpin == TSpinType::TSpin) {
				result.attack += status.b2b ? 3 : 2;
			}
			result.attack += combo_table[std::min(table_max - 1, ++result.combo)];
			result.b2b = eval_result.typeTSpin != TSpinType::None;
			break;
		case 2:
			if (eval_result.typeTSpin != TSpinType::None) {
				result.like += 8;
				result.attack += status.b2b ? 4 : 3;
			}
			result.attack += 1;
			result.attack += combo_table[std::min(table_max - 1, ++result.combo)];
			result.b2b = eval_result.typeTSpin != TSpinType::None;
			break;
		case 3:
			if (eval_result.typeTSpin != TSpinType::None) {
				result.like += 12;
				result.attack += status.b2b ? 6 : 4;
			}
			result.attack += 2;
			result.attack += combo_table[std::min(table_max - 1, ++result.combo)];
			result.b2b = eval_result.typeTSpin != TSpinType::None;
			break;
		case 4:
			result.attack += combo_table[std::min(table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
			result.b2b = true;
			break;
		}
		if (result.combo < 5) {
			result.like -= 1.5 * result.combo;
		}
		if (eval_result.count == 0 && result.map_rise == 0) {
			result.like += 20;
			result.attack += 6;
		}
		if (status.b2b && !result.b2b) {
			result.like -= 2;
		}
		size_t t_expect = [=]() -> int {
			if (hold && *hold == Piece::T) {
				return 0;
			}
			for (size_t i = 0; i < nexts->size(); ++i) {
				if ((*nexts)[i] == Piece::T) {
					return i;
				}
			}
			return 14;
		}();
		if (hold)
			switch (*hold) {
			case Piece::T:
				if (eval_result.typeTSpin == TSpinType::None) {
					result.like += 4;
				}
				break;
			case Piece::I:
				if (eval_result.clear != 4) {
					result.like += 2;
				}
				break;
			}
		double rate = (1. / (depth + 1)) + 3;
		result.max_combo = std::max(result.combo, result.max_combo);
		result.max_attack = std::max(result.attack, result.max_attack);
		result.value +=
			((0. +
				result.max_attack * 40 +
				result.attack * 256 * rate +
				eval_result.t2_value * (t_expect < 8 ? 512 : 320) * 1.5 +
				(safe >= 12 ? eval_result.t3_value * (t_expect < 4 ? 10 : 8) * (result.b2b ? 512 : 256) / (6 + result.under_attack) : 0) +
				(result.b2b ? 512 : 0) + result.like * 64) *
				std::max<double>(0.05, (full_count_ - eval_result.count - result.map_rise * width) / double(full_count_)) +
				result.max_combo * (result.max_combo - 1) * 40 - needSd * 100
				);
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
struct TreeNodeCompare {
	bool operator()(TreeNode* const& lhs, TreeNode* const& rhs) const {
		return lhs->props < rhs->props;
	}
};

template<class TreeNode>
class TreeContext {
public:
	using Evaluator = Evaluator<TreeContext>;
	friend TreeNode;
	using Result = std::tuple<TetrisNode, bool>;
	using treeQueue = std::priority_queue<TreeNode*, std::vector<TreeNode*>, TreeNodeCompare<TreeNode>>;

public:
	std::vector<TetrisNode> nodes;
	Evaluator evalutator{ this };
	bool isOpenHold = true;
	std::atomic<int> count = 0;
	std::string info;
	int flag = 0, isAdvanceNext{}; //detect none hold convert to be used
	std::vector<Piece>nexts;
	std::vector<treeQueue>level, extendedLevel;
	std::vector<double> width_cache;
	double  div_ratio{};
	int width{};

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
		auto depth = dp.size() + 1 + ((!hold.has_value() && isOpenHold) ? -1 : 0);
		{
			level.clear();
			extendedLevel.clear();
			level.resize(depth);
			extendedLevel.resize(depth);
		}

		if (depth > 0) {
			double ratio = 1.5;
			while (width_cache.size() < depth - 1)
				width_cache.emplace_back(std::pow(width_cache.size() + 2, ratio));
			div_ratio = 2 / *std::max_element(width_cache.begin(), width_cache.end());
		}
		width = 0;

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
			tree->props.status.max_combo = combo;
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
		auto previewLength = level.size() - 1;
		auto i = previewLength + 1;
		if (width == 0) { // for root
			auto& firstLevel = level[previewLength];
			auto len = root->children.empty() ? 0 : root->children.size();
			if (root->eval()) {
				for (auto index = len; index < root->children.size(); index++) {
					firstLevel.push(root->children[index]);
				}
			}
			width = 2;
		}
		else width += 1;
		while (--i > 0) {
			auto levelPruneHold = std::max<size_t>(1, size_t(width_cache[i - 1] * width * div_ratio));
			auto& deepIndex = i;
			auto& level_prune_hold = levelPruneHold;
			auto& wait = level[deepIndex];
			if (wait.empty()) {
				continue;
			}
			else {
			}
			auto& sort = extendedLevel[deepIndex];
			auto& next = level[deepIndex - 1];
			auto push_one = [&] {
				auto* child = wait.top();
				wait.pop();
				sort.push(child);
				child->eval();
				for (auto& it : child->children) {
					next.emplace(it);
				}
			};
			if (wait.empty()) {
				// do nothing
			}
			else if (sort.size() >= level_prune_hold)
			{
				if (sort.top()->props.status < wait.top()->props.status)
				{
					push_one();
				}
			}
			else {
				do
				{
					push_one();
				} while (sort.size() < level_prune_hold && !wait.empty());
			}
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
	using TreeNodeCompare = TreeNodeCompare<TreeNode>;

	friend TreeContext;
	friend TreeNodeCompare;
public:
	struct EvalArgs {
		TetrisNode land_node;
		int clear = 0;
		Evaluator::Result eval_result;
		Evaluator::Status status;
		int needSd = 0;

		bool operator<(const  EvalArgs& other) const {
			return this->status < other.status;
		}

		bool operator>(const  EvalArgs& other) const {
			return this->status > other.status;
		}
	};

private:
	TreeNode* parent = nullptr;
	TreeContext* context = nullptr;
	std::vector<Piece>* nexts = nullptr;
	int ndx = 0, nth = 0;
	TetrisMap map;
	std::mutex mtx;
	std::atomic<bool> extended = false, occupied = false;
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
		auto eval_result = context->evalutator.evaluate(land_node, map_, clear);
		auto status = context->evalutator.get(eval_result, props.status, hold,
			nexts, nth, needSd);
		EvalArgs props_ = {
			land_node, static_cast<int>(clear),
			eval_result,
			status,
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
				std::lock_guard guard{ this->mtx };
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
		bool result = false;
		if (occupied) return false;
		occupied = true;
		if (!extended) {
			if (cur) {
				if (children.size() == 0 || (parent == nullptr)) {
					search();
					extended = true;
					result = true;
					goto out;
				}
			}
			else {
				goto out;
			}
		}
	out:
		occupied = false;
		return result;
	}

	TreeContext::Result getBest() {
		auto& lastLevel = context->level;
		TreeNode* best = lastLevel.front().empty() ? nullptr : lastLevel.front().top();
		if (best == nullptr)
			for (const auto& level : context->extendedLevel) {
				if (!level.empty()) {
					best = level.top();
					break;
				}
			}
		if (best == nullptr)
			return context->empty();
		std::deque<TreeNode*> record{ best };
		while (best->parent != nullptr) {
			record.push_front(best->parent);
			best = best->parent;
		}
		return { record[1]->props.land_node, record[1]->isHold };
	}
};
