#pragma once
#include "TetrisBase.h"
#include <random>
#include <algorithm>
#include <deque>
namespace CC_LIKE {

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
		static std::vector<TetrisNode> search(TetrisNode& first, TetrisMap& map) {
			auto isFilter = (first.type == Piece::I || first.type == Piece::Z || first.type == Piece::S);
			auto isNodeT = first.type == Piece::T;
			auto softDropDisable = true, fixed = false;
			std::vector<searchNode> queue;
			std::vector<TetrisNode> result;
			filterLookUp<std::size_t> checked{ map };
			filterLookUp<std::tuple<std::size_t, int, bool>> locks{ map };
			std::size_t amount = 0;

			bool fastMode{};
			std::size_t cals{};
			if (std::all_of(map.top, map.top + map.width, [&map](char n) {return n < map.height - 4; })) {
				fastMode = true;
				for (auto& node : first.getHoriZontalNodes(map)) {
					auto drop = node.drop(map);
					auto deltaRS = std::abs(node.rs - first.rs);
					auto deltaX = std::abs(node.x - first.x);
					if (deltaRS == 3) deltaRS = 1;
					queue.emplace_back(drop, 1 + deltaX + deltaRS);
					queue.back().node.step = node.y - drop.y;
					checked.set(drop, cals);
					++cals;
				}
				amount = cals;
			}
			else {
				fastMode = false;
				queue.emplace_back(first, 0);
				checked.set(first, 1);
				amount = 1;
			}

			auto updatePosition = [&](TetrisNode& target, int count, bool lastRotate) {
				if (isNodeT && lastRotate == true) {
					if (auto it = checked.get(target); it) {
						queue[*it].node.lastRotate = true;
					}
				}
				if (fastMode && target.open(map)) return;
				if (!checked.contain(target)) {
					target.lastRotate = lastRotate;
					checked.set(target, amount);
					queue.emplace_back(target, count);
					++amount;
				}
			};

			for (std::size_t i = 0; i != amount; ++i) {
				auto element = queue[i];
				auto& [node, count] = element;
				fixed = i < cals ? true : false;
				if (!node.check(map, node.x, node.y - 1)) {
					auto _step = node.step;
					if (fixed) node.step = 0;
					fixed = true;
					if (isFilter) {
						if (!locks.contain<false>(node)) {
							result.emplace_back(node.type, node.x, node.y, node.rs);
							result.back().step = node.step;
							locks.set<false>(node, { result.size() - 1, count ,node.lastRotate });
						}
						else {
							auto& [index, step, lastRoate] = locks.get<false>(node).value();
							if (node.rs == first.rs) result[index] = node;
							else if (step > count) result[index] = node;
						}
					}
					else {
						result.emplace_back(node.type, node.x, node.y, node.rs);
						if (isNodeT) {
							auto [spin, mini] = result.back().corner3(map);
							result.back().lastRotate = node.lastRotate;
							result.back().mini = mini, result.back().spin = spin;
						}
						result.back().step = node.step;
					}
					node.step = _step;
				}
				if (auto next = TetrisNode::shift(node, map, -1, 0)) { // Left
					next->step = node.step;
					updatePosition(*next, count + 1, false);
				}
				if (auto next = TetrisNode::shift(node, map, 1, 0)) { // Right
					next->step = node.step;
					updatePosition(*next, count + 1, false);
				}
				if (auto next = TetrisNode::rotate<true>(node, map)) { // Rotate Reverse
					next->step = node.step;
					updatePosition(*next, count + 1, true);
				}
				if (auto next = TetrisNode::rotate<false>(node, map)) { // Rotate Normal
					next->step = node.step;
					updatePosition(*next, count + 1, true);
				}
				if (!fixed)
					if (auto next = TetrisNode::shift(node, map, 0, -node.getDrop(map))) { // Softdrop to bottom
						next->step += node.step + node.y - next->y;
						updatePosition(*next, count + 1, false);
					}
				if (!softDropDisable) {
					if (auto next = TetrisNode::shift(node, map, 0, -1)) { // Softdrop
						next->step += node.step + 1;
						updatePosition(*next, count + 1, false);
					}
				}
			}
			return result;
		}

		static std::vector<Oper> make_path(TetrisNode& startNode, TetrisNode& landPoint, TetrisMap& map, bool NoSoftToBottom = false) {
			using OperMark = std::pair<TetrisNode, Oper>;
			auto softDropDisable = true;
			std::deque<TetrisNode> nodeSearch;
			filterLookUp<OperMark> nodeMark{ map };
			auto& landNode = landPoint;

			auto mark = [&nodeMark](TetrisNode& key, const OperMark& value) {
				if (!nodeMark.contain(key)) {
					nodeMark.set(key, value);
					return true;
				}
				else return false;
			};

			auto buildPath = [&nodeMark, &NoSoftToBottom](TetrisNode& lp) {
				std::deque<Oper> path;
				TetrisNode* node = &lp;
				while (nodeMark.contain(*node)) {
					auto& result = *nodeMark.get(*node);
					auto& next = std::get<0>(result);
					auto dropToSd = (path.size() > 0 && std::get<1>(result) == Oper::DropToBottom);
					auto softDropDis = next.y - node->y;
					node = &next;
					if (node->type == Piece::None) break;
					path.push_front(std::get<1>(result));
					if (dropToSd && NoSoftToBottom) {
						path.pop_front();
						path.insert(path.begin(), softDropDis, Oper::SoftDrop);
					}
				}
				while (path.size() != 0 && (path[path.size() - 1] == Oper::SoftDrop || path[path.size() - 1] == Oper::DropToBottom))
					path.pop_back();
				path.push_back(Oper::HardDrop);
				return std::vector<decltype(path)::value_type>(std::make_move_iterator(path.begin()),
					std::make_move_iterator(path.end()));
			};

			nodeSearch.push_back(startNode);
			nodeMark.set(startNode, OperMark{ TetrisNode::spawn(Piece::None), Oper::None });

			auto disable_d = landNode.open(map);
			auto need_rotate = landNode.type == Piece::T && landNode.typeTSpin != TSpinType::None;
			static constexpr Oper opers[]{ Oper::Cw, Oper::Ccw, Oper::Left, Oper::Right, Oper::SoftDrop, Oper::DropToBottom };
			if (landNode.type == Piece::T && landNode.typeTSpin != TSpinType::None) disable_d = false;
			while (!nodeSearch.empty()) {
				auto next = nodeSearch.front();
				nodeSearch.pop_front();
				for (auto& oper : opers) {
					auto rotate = false;
					std::optional<TetrisNode> node{ std::nullopt };
					if (oper == Oper::SoftDrop && softDropDisable) continue;
					bool res = false;
					switch (oper) {
					case Oper::Cw: res = (node = TetrisNode::rotate<true>(next, map)) && mark(*node, OperMark{ next, oper }); rotate = true; break;
					case Oper::Ccw: res = (node = TetrisNode::rotate<false>(next, map)) && mark(*node, OperMark{ next, oper }); rotate = true; break;
					case Oper::Left: res = (node = TetrisNode::shift(next, map, -1, 0)) && mark(*node, OperMark{ next, oper }); break;
					case Oper::Right: res = (node = TetrisNode::shift(next, map, 1, 0)) && mark(*node, OperMark{ next, oper }); break;
					case Oper::SoftDrop: res = (node = TetrisNode::shift(next, map, 0, 1)) && mark(*node, OperMark{ next, oper }); break;
					case Oper::DropToBottom: res = (node = next.drop(map)) && mark(*node, OperMark{ next, oper }); break;
					default: break;
					}
					if (res && *node == landNode) {
						if (need_rotate ? rotate : true)
							return buildPath(landNode);
						else nodeMark.clear(*node);
					}
					else if ((oper == Oper::DropToBottom ? !disable_d : true) && node && res)
						nodeSearch.push_back(*node);
				}
			}
			return std::vector<Oper> {};
		}
	};

	template<class TreeContext>
	struct Evaluator {
		int col_mask_{}, row_mask_{}, full_count_{}, width{}, height{};
		std::array<int, 12> table{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
		TreeContext* context = nullptr;

		struct BoardWeights {
			int back_to_back = 50;
			int bumpiness = -10;
			int bumpiness_sq = -5;
			int height = -40;
			int top_half = -150;
			int top_quarter = -1000;
			int cavity_cells = -150;
			int cavity_cells_sq = -10;
			int overhang_cells = -50;
			int overhang_cells_sq = -10;
			int covered_cells = -10;
			int covered_cells_sq = -10;
			int tslot_present = 150;
			int well_depth = 50;
			int max_well_depth = 8;
		}board_weights;// = { 50,-10,-5,-40,-150,-1000,-150,-10,-50,-10,-10,-10,150,50, 8 };

		struct PlacementWeights {
			int soft_drop = 0;
			int b2b_clear = 100;
			int clear1 = -150;
			int clear2 = -100;
			int clear3 = -50;
			int clear4 = 400;
			int tspin1 = 130;
			int tspin2 = 400;
			int tspin3 = 600;
			int mini_tspin1 = 0;
			int mini_tspin2 = 100;
			int perfect_clear = 1000;
			std::array<int, 12> combo_table = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
		} placement_weights;// = { 0,100,-150,-100,-50,400,130,400,600,0,100,1000,{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 } };

		struct Result {
			double transient_value;
			int clear;
			int count;
			TetrisMap* map;
			TSpinType typeTSpin;
		};

		struct Status {
			Result eval;
			int max_combo;
			int max_attack;
			int death;
			int combo;
			int attack;
			int under_attack;
			int map_rise;
			bool b2b;
			double acc_value;
			double value;
			bool deaded;
			bool operator<(const Status& other) const {
				return this->value < other.value;
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

		Result evalute(TetrisNode& lp, TetrisMap& map, int clear, std::vector<Piece>* nexts) {
			Result result;
			if (lp.type == Piece::T) {
				if (clear > 0 && lp.lastRotate) {
					if (clear == 1 && lp.mini && lp.spin)
						lp.typeTSpin = TSpinType::TSpinMini;
					else if (lp.spin)
						lp.typeTSpin = TSpinType::TSpin;
					else
						lp.typeTSpin = TSpinType::None;
				}
			}
			std::memset(&result, 0,sizeof result);
			result.typeTSpin = lp.typeTSpin;
			auto& transient_eval = result.transient_value;

			auto highest_point = *std::max_element(map.top, map.top + map.width);
			transient_eval += board_weights.height * highest_point;
			transient_eval += board_weights.top_half * std::max(highest_point - 10, 0);
			transient_eval += board_weights.top_quarter * std::max(highest_point - 15, 0);

			auto well = 0;
			for (auto x = 1; x < map.width; x++)  if (map.top[x] < map.top[well]) well = x;

			auto depth = 0;
			for (auto y = map.top[well], out = 1; y < 20 && out; y++) {
				for (auto x = 0; x < 10; x++) {
					if (x != well && !map(x, y)) {
						out = 0;
						break;
					}
					depth += 1;
				}
			}

			depth = std::min(depth, board_weights.max_well_depth);
			transient_eval += board_weights.well_depth * depth;

			if (board_weights.bumpiness | board_weights.bumpiness_sq != 0) {
				auto [bump, bump_sq] = [&map,&well]() //bumpiness 
				{
					auto bumpiness = -1, bumpiness_sq = -1;
					auto  prev = (well == 0 ? 1 : 0);
					for (auto i = 1; i < 10; i++) {
						if (i == well) continue;
						auto dh = std::abs(map.top[prev] - map.top[i]);
						bumpiness += dh;
						bumpiness_sq += dh * dh;
						prev = i;
					}
					return  std::tuple<int, int>{ std::abs(bumpiness), std::abs(bumpiness_sq) };
				}();

				transient_eval += bump * board_weights.bumpiness;
				transient_eval += bump_sq * board_weights.bumpiness_sq;
			}

			if (board_weights.cavity_cells | board_weights.cavity_cells_sq |
				board_weights.overhang_cells | board_weights.overhang_cells_sq != 0) {
				auto [cavity_cells, overhang_cells] = [&map]() //cavities_and_overhangs
				{
					auto cavities = 0;
					auto overhangs = 0;

					for (auto y = 0;y<map.roof;y++) {
						for (auto x = 0; x < 10; x++) {
							if (map(x, y) || y >= map.top[x]) {
								continue;
							}

								if( x > 1) {
									if (map.top[x - 1] <= y - 1 && map.top[x - 2] <= y) {
										overhangs += 1;
										continue;
									}
								}

							if (x < 8) {
								if (map.top[x + 1] <= y - 1 && map.top[x + 2] <= y){
									overhangs += 1;
									continue;
								}
							}

							cavities += 1;
						}
					}
					return std::tuple<int, int>{cavities, overhangs};
				}();
				transient_eval += board_weights.cavity_cells * cavity_cells;
				transient_eval += board_weights.cavity_cells_sq * cavity_cells * cavity_cells;
				transient_eval += board_weights.overhang_cells * overhang_cells;
				transient_eval += board_weights.overhang_cells_sq * overhang_cells * overhang_cells;
			}

			if (board_weights.covered_cells | board_weights.covered_cells_sq != 0) {
				auto [covered_cells, covered_cells_sq] = [&map]() //covered_cells
				{
					auto covered = 0, covered_sq = 0;
					for (auto x = 0; x < 10; x++) {
						auto cells = 0;
						for (auto y = map.top[x]; y >= 0; y--) {
							if (!map(x, y)) {
								covered += cells;
								covered_sq += cells * cells;
								break;
							}
							cells += 1;
						}
					}
					return std::tuple<int, int>{covered, covered_sq};
				}();
				transient_eval += board_weights.covered_cells * covered_cells;
				transient_eval += board_weights.covered_cells_sq * covered_cells_sq;
			}

			auto tslot = [&nexts, &map]() {
				if (!std::count(nexts->begin(), nexts->end(), Piece::T)) return false;
				for (auto x = 0; x < 8; x++) {
					const auto& [h0, h1, h2] = std::tuple<int, int, int>{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
					if (h0 >= 2 && h2 == h0 - 2 && h1 < h2 && map(x, h0) && !map(x, h0 - 1) && map(x, h0 - 2))
						return true;
					else if (h2 >= 2 && h0 == h2 - 2 && h1 < h0 && map(x + 2, h2) && !map(x + 2, h2 - 1) && map(x + 2, h2 - 2))
						return true;
				}
				return false;
			};

			if (tslot()) {
		    	transient_eval += board_weights.tslot_present;
			}
			result.map = &map;
			result.count = map.count;
			result.clear = clear;
			return result;
		}

		Status get(const Evaluator::Result& eval_result, Status& status, std::optional<Piece> hold,
			std::vector<Piece>* nexts, int depth) {
			Status result = status;
			int safe = getSafe(*eval_result.map);
			int table_max = table.size();
			result.eval = eval_result;
			result.value = 0;
			if (safe <= 0)
			{
				result.deaded = true;
				return result;
			}

			auto& acc_eval = result.acc_value;
			auto& clear = eval_result.clear;
			auto& tspin = eval_result.typeTSpin;
			auto& count = eval_result.count;
			result.b2b = status.b2b && (clear == 0 || clear == 4 || tspin != TSpinType::None);

			if (clear) {
				if (count == 0) acc_eval += placement_weights.perfect_clear;
				if (result.b2b) acc_eval += placement_weights.b2b_clear;
				acc_eval += placement_weights.combo_table[std::min<int>(placement_weights.combo_table.size() - 1, ++result.combo)];
				switch (clear) {
				case 1:
				{
					if (tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin1;
					else if (tspin == TSpinType::TSpinMini) acc_eval += placement_weights.mini_tspin1;
					else acc_eval += placement_weights.clear1;
				}
				break;
				case 2:
				{
					if (tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin2;
					else acc_eval += placement_weights.clear2;
				}
				break;
				case 3:
				{
					if (tspin == TSpinType::TSpin) acc_eval += placement_weights.tspin3;
					else acc_eval += placement_weights.clear3;
				}
				break;
				case 4:acc_eval += placement_weights.clear4; break;
				}
			}
			else result.combo = 0;
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
		std::vector<TetrisNode> nodes;
		Evaluator evalutator{ this };
		bool isOpenHold = true;
		int count = 0;
		std::vector<Piece>nexts, noneHoldFirstNexts;
		std::random_device rd;
		std::mt19937 gen{ rd() };

		TreeContext() :root(nullptr, TreeContext::treeDelete) {
		};

		~TreeContext() {
			root.reset(nullptr);
			return;
		}

		void createRoot(const TetrisNode& node, const TetrisMap& map, std::vector<Piece>& dp, std::optional<Piece> hold, int b2b,
			int combo, int underAttack, int dySpawn) {
			evalutator.init(&map);
			nexts = dp;
			noneHoldFirstNexts = std::vector<Piece>{ dp.begin() + 1, dp.end() };
			auto depth = dp.size() + 1 + ((!hold.has_value() && isOpenHold) ? -1 : 0); // + 1;

			nodes.clear();
			for (auto i = 0; i < 7; i++) nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i), &map, dySpawn));

			TreeNode* tree = new TreeNode;
			{//init for root
				tree->context = this;
				tree->nexts = &nexts;
				tree->cur = node.type;
				tree->map = map;
				tree->hold = hold;
				tree->evalParm.status.under_attack = underAttack;
				tree->evalParm.status.b2b = b2b;
				tree->evalParm.status.combo = combo;
			}
			update(tree);
			//root.reset(tree);
		}

		void run() {
			TreeNode* branch = root.get();
			while (true) {
				auto& branches = branch->children;
				if (branches.empty()) break;
				std::vector<double> tmp{};
#ifdef _DEBUG
					if (branches.size() == 1) {
						branch = branches[0];
						break;
					}
#endif
				std::sort(branches.begin(), branches.end(), TreeNode::TreeNodeCompare);
				auto min = branches.back()->evalParm.status.value;
				std::transform(branches.begin(), branches.end(), std::back_inserter(tmp),
					[&min, index = 0](auto&& it) mutable {
					auto& value = it->evalParm.status.value;
					auto e = value - min;
					auto dis = index++;
					return e * e / (dis + 1); }
				);
				std::discrete_distribution<> d{ std::begin(tmp),std::end(tmp) };
				branch = branch->children[d(gen)];
			}
			if (branch->eval()) 
				branch->repropagate();
		}

		void update(TreeNode* node) {
			count = 0;
			if (root == nullptr) {
				root.reset(node);
				return;
			}
			auto it = std::find_if(root->children.begin(), root->children.end(), [&](auto val) {return (*node) == (*val); });
			if (it != root->children.end()) {
				auto selected = *it;
				selected->parent = nullptr;
				std::function<void(TreeNode*)> save = [&](TreeNode* t) {
					if (t == nullptr) return;
					t->nextIndex--;
					if (t->nexts == (&noneHoldFirstNexts) && t->hold && isOpenHold) t->nexts = &nexts;
					if (!t->cur.has_value())
						t->cur = (t->nextIndex - 1 == t->nexts->size() ? std::nullopt : decltype(t->cur){t->nexts->operator[](t->nextIndex - 1)});
					for (auto& child : t->children) {
						if (child != nullptr)
							save(child);
					}
				};
				save(selected);
				root->children.erase(it);
				root.reset(selected);
				delete node;
				node = nullptr;
			}
			else 
				root.reset(node);
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
		struct EvalParm {
			TetrisNode land_node;
			int clear = 0;
			Evaluator::Status status;
			int needSd = 0;
		};

		constexpr static struct {
			bool operator()(TreeNode* const& lhs, TreeNode* const& rhs) const {
				return lhs->evalParm.status < rhs->evalParm.status;
			}
		}TreeNodeCompare{};

	private:
		TreeNode* parent = nullptr;
		TreeContext* context = nullptr;
		std::vector<Piece>* nexts = nullptr;
		int nextIndex = 0;
		TetrisMap map;
		bool extended = false, isHold = false, isHoldLock = false;
		std::optional<Piece> cur = std::nullopt, hold = std::nullopt;
		EvalParm evalParm;
		std::vector<TreeNode*>children;

	public:
		TreeNode() {}
		TreeNode(TreeContext* _ctx, TreeNode* _parent, std::optional<Piece> _cur, TetrisMap _map,
			int _nextIndex, std::optional<Piece> _hold, bool _isHold, EvalParm& _evalParm) :
			parent(_parent), context(_ctx), nexts(_parent->nexts), nextIndex(_nextIndex),
			map(std::move(_map)), isHold(_isHold), cur(_cur), hold(_hold), evalParm(_evalParm) {

		}

		bool operator==(TreeNode& p) {
			return cur == p.cur &&
				map == p.map &&
				hold == p.hold;
		}

		auto branches() {
			std::vector<TreeNode*> arr;
			for (auto& it : children) {
				//if (!it->extended && *it->cur != Piece::None) arr.push_back(it);
				if (it->extended)
					arr.push_back(it);
			}
			return arr;
		}

		TreeNode* generateChildNode(TetrisNode& i_node, bool _isHoldLock, std::optional<Piece> _hold, bool _isHold) {
			decltype (cur) next_node = std::nullopt;
			if (nextIndex < nexts->size()) next_node = nexts->at(nextIndex);
			auto map_ = map;
			auto clear = i_node.attach(map_);
			auto status_ = context->evalutator.get(
				context->evalutator.evalute(i_node, map_, clear, nexts),
				evalParm.status, hold, nexts, nextIndex);
			status_.value = status_.eval.transient_value + status_.acc_value;
			/*auto needSd = this->evalParm.needSd + int(i_node.type != Piece::T ?
				i_node.step : (!clear ? i_node.step : 0));
			status_.value -= needSd * 70;*/
			EvalParm evalParm_ = { i_node, static_cast<int>(clear), status_, 0/*needSd*/};
			TreeNode* new_tree = nullptr;
			if (!status_.deaded) {
				context->count++;
				new_tree = new TreeNode{ context, this, next_node, map_, nextIndex + 1, _hold, _isHold, evalParm_ };
				new_tree->isHoldLock = _isHoldLock;
				children.push_back(new_tree);
			}
			return new_tree;
		}

		void search() {
			if (context->isOpenHold && parent == nullptr/* && !isHoldLock*/) {
				search_hold();
				return;
			}
			for (auto& node : Search::search(context->getNode(*cur), map)) {
				generateChildNode(node, isHoldLock, hold, false/*bool(isHold)*/);
			}
		}

		void search_hold(bool op = false, bool noneFirstHold = false) {
			if (!hold.has_value() || nexts->size() == 0) {
				auto hold_save = hold;
				if (!hold.has_value()) {
					hold = nexts->front();
					search_hold(false, true);
				}
				hold = hold_save;
				return;
			}
			if (cur == hold) {
					for (auto& node : Search::search(context->getNode(*cur), map))
						generateChildNode(node, true, hold, false);
			}
			else if (cur != hold) {
				if (!isHoldLock)
					for (auto& node : Search::search(context->getNode(*cur), map)) {
						auto childTree = generateChildNode(node, true, noneFirstHold ? std::nullopt : hold, bool(op ^ false));
					}
				for (auto& node : Search::search(context->getNode(*hold), map)) {
					if (noneFirstHold) nexts = &context->noneHoldFirstNexts;
					auto childTree = generateChildNode(node, true, cur, bool(op ^ true));
					if (noneFirstHold) nexts = &context->nexts;
				}
			}
		}

		bool eval() {
			if (!extended) {
				if (cur) {
					if (children.size() == 0 || (parent == nullptr)) {
						search();
						extended = true;
					}
				}
				else return false;
			}
			else return true;
		}

		void repropagate() {
			auto it = this;
			while (it != nullptr) {
				if (!it->children.empty()) {
					std::sort(it->children.begin(), it->children.end(), TreeNode::TreeNodeCompare);
					it->evalParm.status.value = it->evalParm.status.acc_value + it->children.back()->evalParm.status.value;
				}
				it = it->parent;
			}
		}

		void run() {}
		TreeContext::Result getBest() {
			std::sort(children.begin(), children.end(), TreeNode::TreeNodeCompare);
				  return { children.back()->evalParm.land_node, children.back()->isHold };
        }
    };
}
