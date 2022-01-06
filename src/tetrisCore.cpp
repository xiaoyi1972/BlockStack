#include "TetrisCore.h"

std::vector<TetrisNode> Search::search(TetrisNode& first, TetrisMap& map) {
	auto is_filter= (first.type == Piece::I || first.type == Piece::Z || first.type == Piece::S);
	auto is_T = first.type == Piece::T;
	auto softDrop_disable = true, fixed = false, fastMode = true;
	std::vector<searchNode> queue;
	std::vector<TetrisNode> result;
	filterLookUp<std::size_t> checked{ map };
	filterLookUp<std::tuple<std::size_t, int, bool>> locks{ map };
	std::size_t amount = 0, cals = 0;
	if (std::all_of(map.top, map.top + map.width, [&map](int n) {return n < map.height - 4; })) {
		fastMode = true;
		for (auto& node : first.getHoriZontalNodes(map)) {
			auto drop = node.drop(map);
			auto delta_rs = std::abs(node.rs - first.rs);
			auto delta_x = std::abs(node.x - first.x);
			if (delta_rs == 3) delta_rs = 1;
			queue.emplace_back(drop, 1 + delta_x + delta_rs);
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
		if (is_T && lastRotate == true) {
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
			if (is_filter) {
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
				if (is_T) {
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
		if (!softDrop_disable) {
			if (auto next = TetrisNode::shift(node, map, 0, -1)) { // Softdrop
				next->step += node.step + 1;
				updatePosition(*next, count + 1, false);
			}
		}
	}
	return result;
}

std::vector<Oper>  Search::make_path(TetrisNode& startNode, TetrisNode& landPoint, TetrisMap& map, bool NoSoftToBottom) {
	//std::cout << "from:" << startNode << " To:" << landPoint<<"\n";
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
			if (oper == Oper::SoftDrop && softDrop_disable) continue;
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


Evaluator::Result Evaluator::evalute(TetrisNode& lp, TetrisMap& map, int clear, int tCount = 0) {
	const int width_m1 = map.width - 1;
	int ColTrans = 2 * (map.height - map.roof);
	int RowTrans = map.roof == map.height ? 0 : map.width;

	for (int y = 0; y < map.roof; ++y)
	{
		if (!map(0, y)) ++ColTrans;
		if (!map(width_m1, y))
		{
			++ColTrans;
		}
		ColTrans += BitCount((map[y] ^ (map[y] << 1)) & col_mask_);
		if (y != 0)
		{
			RowTrans += BitCount(map[y - 1] ^ map[y]);
		}
	}
	RowTrans += BitCount(row_mask_ & ~map[0]);
	if (map.roof > 0)
		RowTrans += BitCount(map.roof == map.height ? row_mask_ & ~map[map.roof - 1] : map[map.roof - 1]);
	struct
	{
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
	struct
	{
		int ClearWidth;
	} a[40];

	std::memset(a, 0, sizeof(a));

	for (int y = map.roof - 1; y >= 0; --y)
	{
		v.LineCoverBits |= map[y];
		int LineHole = v.LineCoverBits ^ map[y];
		if (LineHole != 0)
		{
			//v.HoleCount += BitCount(LineHole);
			++v.HoleLine;
			a[v.HolePosyIndex].ClearWidth = 0;
			for (int hy = y + 1; hy < map.roof; ++hy)
			{
				uint32_t CheckLine = LineHole & map[hy];
				if (CheckLine == 0)
				{
					break;
				}
				a[v.HolePosyIndex].ClearWidth += (hy + 1) * BitCount(CheckLine);
			}
			++v.HolePosyIndex;
		}
		for (int x = 1; x < width_m1; ++x)
		{
			if ((LineHole >> x) & 1)
			{
				v.HoleDepth += ++v.HoleNum[x];
			}
			else
			{
				v.HoleNum[x] = 0;
			}
			if (((v.LineCoverBits >> (x - 1)) & 7) == 5)
			{
				v.WellDepth += ++v.WellNum[x];
			}
		}
		if (LineHole & 1)
		{
			v.HoleDepth += ++v.HoleNum[0];
		}
		else
		{
			v.HoleNum[0] = 0;
		}
		if ((v.LineCoverBits & 3) == 2)
		{
			v.WellDepth += ++v.WellNum[0];
		}
		if ((LineHole >> width_m1) & 1)
		{
			v.HoleDepth += ++v.HoleNum[width_m1];
		}
		else
		{
			v.HoleNum[width_m1] = 0;
		}
		if (((v.LineCoverBits >> (width_m1 - 1)) & 3) == 1)
		{
			v.WellDepth += ++v.WellNum[width_m1];
		}
	}

	Result result;
	result.value = (0.
		- map.roof * 128
		- ColTrans * 160
		- RowTrans * 160
		- v.HoleCount * 80
		- v.HoleLine * 380
		- v.WellDepth * 100
		- v.HoleDepth * 40
		);

	double rate = 32, mul = 1.0 / 4;
	for (int i = 0; i < v.HolePosyIndex; ++i, rate *= mul)
	{
		result.value -= a[i].ClearWidth * rate;
	}

	result.count = map.count + v.HoleCount;
	result.clear = clear;
	result.t2_value = 0;
	result.t3_value = 0;
	result.map = &map;

#define USE_CC_DETECT_SLOT 0
#if USE_CC_DETECT_SLOT 1
	auto& T = context->getNode(Piece::T);
	for (auto x = 0; x < 8; x++) {
		const auto& [h0, h1, h2] = std::tuple<int, int, int>{ map.top[x] - 1, map.top[x + 1] - 1, map.top[x + 2] - 1 };
		if (h0 >= 2 && h2 == h0 - 2 && h1 < h2) {
			if (map(x, h0) && !map(x, h0 - 1) && map(x, h0 - 2)) {		                                      	/*T.attach(map, x, h2, 2) **/
				double t2_value = 0;
				double fit = T.fillRows(map, x, h2, 2);
				double bits = double(BitCount(map[h0 - 2], map[h0 - 1]) + 4) / map.width;
				t2_value = 2 + bits;
				if (fit == bits) t2_value += fit * fit;
				result.t2_value = std::max(result.t2_value, t2_value);
				if (fit == 2) break;
			}
		}
		else if (h2 >= 2 && h0 == h2 - 2 && h1 < h0) {
			if (map(x + 2, h2) && !map(x + 2, h2 - 1) && map(x + 2, h2 - 2)) { 				/*T.attach(map, x, h0, 2) **/
				double t2_value = 0;
				double fit = T.fillRows(map, x, h0, 2);
				double bits = double(BitCount(map[h2 - 2], map[h2 - 1]) + 4) / map.width;
				t2_value = 2 + bits;
				if (fit == bits) t2_value += fit * fit;
				result.t2_value = std::max(result.t2_value, t2_value);
				if (fit == 2) break;
			}
		}
	}
#else
	bool finding2 = true;
	bool finding3 = true;
	for (int y = 0; (finding2 || finding3) && y < map.roof - 2; ++y)
	{
		int row0 = map[y];
		int row1 = y + 1 < map.height ? map[y + 1] : 0;
		int row2 = y + 2 < map.height ? map[y + 2] : 0;
		int row3 = y + 3 < map.height ? map[y + 3] : 0;
		int row4 = y + 4 < map.height ? map[y + 4] : 0;
		for (int x = 0; finding2 && x < map.width - 2; ++x)
		{
			if (((row0 >> x) & 7) == 5 && ((row1 >> x) & 7) == 0)
			{
				result.t2_value += 1;
				if (BitCount(row0) == map.width - 1)
				{
					result.t2_value += 1;
					if (BitCount(row1) == map.width - 3)
					{
						result.t2_value += 2;
						int row2_check = (row2 >> x) & 7;
						if (row2_check == 1 || row2_check == 4)
						{
							result.t2_value += 2;
						}
						finding2 = false;
					}
				}
			}
		}
		for (int x = 0; finding3 && x < map.width - 3; ++x)
		{
			if (((row0 >> x) & 15) == 11 && ((row1 >> x) & 15) == 9)
			{
				int t3_value = 0;
				if (BitCount(row0) == map.width - 1)
				{
					t3_value += 1;
					if (BitCount(row1) == map.width - 2)
					{
						t3_value += 1;
						if (((row2 >> x) & 15) == 11)
						{
							t3_value += 2;
							if (BitCount(row2) == map.width - 1)
							{
								t3_value += 2;
							}
						}
						int row3_check = ((row3 >> x) & 15);
						if (row3_check == 8 || row3_check == 0)
						{
							t3_value += !!row3_check;
							int row4_check = ((row4 >> x) & 15);
							if (row4_check == 4 || row4_check == 12)
							{
								t3_value += 1;
							}
							else
							{
								t3_value -= 2;
							}
						}
						else
						{
							t3_value = 0;
						}
					}
				}
				result.t3_value += t3_value;
				if (t3_value > 3)
				{
					finding3 = false;
				}
			}
			if (((row0 >> x) & 15) == 13 && ((row1 >> x) & 15) == 9)
			{
				int t3_value = 0;
				if (BitCount(row0) == map.width - 1)
				{
					t3_value += 1;
					if (BitCount(row1) == map.width - 2)
					{
						t3_value += 1;
						if (((row2 >> x) & 15) == 13)
						{
							t3_value += 2;
							if (BitCount(row2) == map.width - 1)
							{
								t3_value += 2;
							}
						}
						int row3_check = ((row3 >> x) & 15);
						if (row3_check == 1 || row3_check == 0)
						{
							t3_value += !!row3_check;
							int row4_check = ((row4 >> x) & 15);
							if (row4_check == 3 || row4_check == 1)
							{
								t3_value += 1;
							}
							else
							{
								t3_value -= 2;
							}
						}
						else
						{
							t3_value = 0;
						}
					}
				}
				result.t3_value += t3_value;
				if (t3_value > 3)
				{
					finding3 = false;
				}
			}
		}
	}
#endif

	if (lp.type == Piece::T && clear > 0 && lp.lastRotate) {
		lp.typeTSpin = lp.spin ?
			(clear == 1 && lp.mini ? TSpinType::TSpinMini : TSpinType::TSpin) : 
			TSpinType::None;
		/*if (clear == 1 && lp.mini && lp.spin)
			lp.typeTSpin = TSpinType::TSpinMini;
		else if (lp.spin) lp.typeTSpin = TSpinType::TSpin;
		else lp.typeTSpin = TSpinType::None;*/
	}

	result.typeTSpin = lp.typeTSpin;
	return result;
}

int Evaluator::getSafe(const TetrisMap& map) {
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

Evaluator::Status Evaluator::get(const Evaluator::Result& eval_result, Status& status, std::optional<Piece> hold,
	std::vector<Piece>* nexts, int depth) {
	Status result = status;
	result.value = eval_result.value;
	int safe = getSafe(*eval_result.map);
	int table_max = table.size();
	if (safe <= 0)
	{
		result.deaded = true;
		return result;
	}

	switch (eval_result.clear)
	{
	case 0:
		if (status.combo > 0 && status.combo < 3)
		{
			result.like -= 2;
		}
		result.combo = 0;
		if (status.under_attack > 0)
		{
			result.map_rise += std::max(0, int(status.under_attack) - status.attack);
			if (result.map_rise >= safe)
			{
				result.death += result.map_rise - safe;
			}
			result.under_attack = 0;
		}
		break;
	case 1:
		if (eval_result.typeTSpin == TSpinType::TSpinMini)
		{
			result.attack += status.b2b ? 2 : 1;
		}
		else if (eval_result.typeTSpin == TSpinType::TSpin)
		{
			result.attack += status.b2b ? 3 : 2;
		}
		result.attack += table[std::min(table_max - 1, ++result.combo)];
		result.b2b = eval_result.typeTSpin != TSpinType::None;
		break;
	case 2:
		if (eval_result.typeTSpin != TSpinType::None)
		{
			result.like += 8;
			result.attack += status.b2b ? 5 : 4;
			//result.attack -= 1;
		}
		result.attack += table[std::min(table_max - 1, ++result.combo)] + 1;
		result.b2b = eval_result.typeTSpin != TSpinType::None;
		break;
	case 3:
		if (eval_result.typeTSpin != TSpinType::None)
		{
			result.like += 12;
			result.attack += status.b2b ? 8 : 6;
			//result.attack -= 2;
		}
		result.attack += table[std::min(table_max - 1, ++result.combo)] + 2;
		result.b2b = eval_result.typeTSpin != TSpinType::None;
		break;
	case 4:
		result.attack += table[std::min(table_max - 1, ++result.combo)] + (status.b2b ? 5 : 4);
		result.b2b = true;
		break;
	}
	if (result.combo < 5)
	{
		result.like -= 1.5 * result.combo;
	}
	if (eval_result.count == 0 && result.map_rise == 0)
	{
		result.like += 20;
		result.attack += 6;
	}
	if (status.b2b && !result.b2b)
	{
		result.like -= 2;
	}
	size_t t_expect = [=]()->int
	{
		if (hold && *hold == Piece::T)
		{
			return 0;
		}
		for (size_t i = depth; i < nexts->size(); ++i)
		{
			if ((*nexts)[i] == Piece::T)
			{
				return i;
			}
		}
		return 14;
	}();
	if (hold && depth == 1)
		switch (*hold)
		{
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
	result.value += ((0.
		+ result.max_attack * 40
		+ result.attack * 256 * rate
		+ eval_result.t2_value * (t_expect < 8 ? 512 : 320) * 1.5
		+ (safe >= 12 ? eval_result.t3_value * (t_expect < 4 ? 10 : 8) * (result.b2b ? 512 : 256) / (6 + result.under_attack) : 0)
		+ (result.b2b ? 512 : 0)
		+ result.like * 64
		) * std::max<double>(0.05, (full_count_ - eval_result.count - result.map_rise * width) / double(full_count_))
		+ result.max_combo * (result.max_combo - 1) * 40
		);
	return result;
}

bool TreeNodeCompare::operator()(TreeNode* const& lhs, TreeNode* const& rhs) const {
	return lhs->evalParm.status < rhs->evalParm.status;
}

void TreeContext::createRoot(const TetrisNode& node, const TetrisMap& map, std::vector<Piece>& dp, std::optional<Piece> hold, int b2b,
	int combo, int underAttack, int dySpawn) {
	evalutator.init(&map);
	nexts = dp;
	noneHoldFirstNexts = std::vector<Piece>{ dp.begin() + 1, dp.end() };
	auto depth = dp.size() + 1 + ((!hold.has_value() && isOpenHold) ? -1 : 0); // + 1;

	TreeNode* tree = new TreeNode;
	{
		tree->context = this;
		tree->nexts = &nexts;
		tree->cur = node.type;
		tree->map = map;
		tree->hold = hold;
		tree->evalParm.status.under_attack = underAttack;
		tree->evalParm.status.b2b = b2b;
		tree->evalParm.status.combo = combo;
		tree->evalParm.eval_result.map = &tree->map;
	}

	level.clear();
	extendedLevel.clear();
	level.resize(depth);
	extendedLevel.resize(depth);

	nodes.clear();
	for (auto i = 0; i < 7; i++)
		nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i), &map, dySpawn));

	tCount = int(hold == Piece::T && isOpenHold) + int(node.type == Piece::T);
	/*for (const auto& i : dp) {
		if (i == Piece::T)
			tCount++;
	}*/

	if (depth > 0) {
		double ratio = 1.5;
		while (width_cache.size() < depth - 1)
			width_cache.emplace_back(std::pow(width_cache.size() + 2, ratio));
		div_ratio = 2 / *std::max_element(width_cache.begin(), width_cache.end());
	}
	width = 0;

	tCount = std::max(0, --tCount);
	//tree->evalParm.status.max_combo = combo;
	update(tree);
	//root.reset(tree);
}

void TreeContext::update(TreeNode* node) {
	count = 0;
	if (root == nullptr) {
		root.reset(node);
		return;
	}
	auto it = std::find_if(root->children.begin(), root->children.end(), [&](auto val) {return (*node) == (*val); });
	if (it != root->children.end()) {
		auto selected = *it;
		selected->parent = nullptr;
		selected->evalParm.status = node->evalParm.status;
		auto needSd = selected->evalParm.needSd;
		auto previewLength = level.size();
		std::function<void(TreeNode*)> save = [&](TreeNode* t) {
			if (t == nullptr) return;
			t->nextIndex--;
			t->isHold = false;
			if (t->nexts == (&noneHoldFirstNexts) && t->hold && isOpenHold)
				t->nexts = &nexts;
			if (t->parent != nullptr) {
				t->evalParm.needSd -= needSd;
				t->evalParm.status = evalutator.get(t->evalParm.eval_result, t->parent->evalParm.status,
					t->hold, t->nexts, t->nextIndex);
				t->evalParm.status.value -= t->evalParm.needSd * 70;
				if (!t->extended) {
					if (!t->cur)
						t->cur = (t->nextIndex - 1 == t->nexts->size() ? std::nullopt : decltype(t->cur){t->nexts->operator[](t->nextIndex - 1)});
					level[previewLength - t->nextIndex].push(t);
				}
				else extendedLevel[previewLength - t->nextIndex].push(t);
			}
			else {
				if (isOpenHold)
					t->extended = false;
			}
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
		selected = nullptr;
	}
	else root.reset(node);
}

void TreeContext::treeDelete(TreeNode* t) {
	if (t == nullptr)
		return;
	for (auto i = 0; i < t->children.size(); i++) {
		if (t->children[i] != nullptr)
			treeDelete(t->children[i]);
	}
	delete t;
}

void TreeContext::run() {
	if (root != nullptr)
		root->run();
}

TreeContext::Result TreeContext::empty() { return { TetrisNode::spawn(*root->cur), false }; }
TreeContext::Result TreeContext::getBest() { return root->getBest(); }

TreeNode* TreeNode::generateChildNode(TetrisNode& node, bool _isHoldLock, std::optional<Piece> _hold, bool _isHold) {
	auto next_node = (nextIndex == nexts->size() ? std::nullopt : decltype(cur) { (*nexts)[nextIndex] });
	auto map_ = map;
	auto clear = node.attach(map_);
	auto eval_result = context->evalutator.evalute(node, map_, clear, context->tCount);
	auto status_ = context->evalutator.get(
		eval_result,
		evalParm.status, hold, nexts, nextIndex);
	auto needSd = this->evalParm.needSd + int(node.type != Piece::T ?
		node.step : (!clear ? node.step : 0));
	status_.value -= needSd * 70;
	//EvalParm evalParm_ = { node, static_cast<int>(clear),eval_result, status_, needSd };
	TreeNode* new_tree = nullptr;
	if (!status_.deaded) {
		context->count++;
		new_tree = new TreeNode{ context, this, next_node, map_, nextIndex + 1, _hold, _isHold,
			{node, static_cast<int>(clear),eval_result,status_, needSd } };
		new_tree->evalParm.eval_result.map = &(new_tree->map);
		new_tree->isHoldLock = _isHoldLock;
		children.push_back(new_tree);
	}
	return new_tree;
}

void TreeNode::search() {
	if (context->isOpenHold && parent == nullptr/* && !isHoldLock*/) {
		search_hold();
		return;
	}
	for (auto& node : Search::search(context->getNode(*cur), map)) {
		generateChildNode(node, isHoldLock, hold, bool(isHold));
	}
}

void TreeNode::search_hold(bool op, bool noneFirstHold) {
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
		if (!isHoldLock)
			for (auto& node : Search::search(context->getNode(*cur), map))
				generateChildNode(node, true, hold, false);
	}
	else if (cur != hold) {
		if (!isHoldLock)
			for (auto& node : Search::search(context->getNode(*cur), map)) {
				auto childTree = generateChildNode(node, true, hold, bool(op ^ false));
			}
		for (auto& node : Search::search(context->getNode(*hold), map)) {
			if (noneFirstHold) nexts = &context->noneHoldFirstNexts;
			auto childTree = generateChildNode(node, true, cur, bool(op ^ true));
			if (noneFirstHold) nexts = &context->nexts;
		}
	}
}

bool TreeNode::eval() {
	if (extended == true)
		return false;
	if (children.size() == 0 || (parent == nullptr)) {
		search();
		extended = true;
	}
	return true;
}

void  TreeNode::run() {
	auto previewLength = context->level.size() - 1;
	auto i = previewLength + 1;
	//std::mutex m;
	if (context->width == 0) { //for root
		auto& firstLevel = context->level[previewLength];
		auto len = children.empty() ? 0 : children.size();
		if (this->eval()) {
			for (auto index = len; index < children.size(); index++) {
				firstLevel.push(children[index]);
			}
		}
		context->width = 2;
	}
	else context->width += 1;
	while (--i > 0) {
		auto levelPruneHold = std::max<size_t>(1, size_t(context->width_cache[i - 1] * context->width * context->div_ratio));
		auto& deepIndex = i;
		/*auto& levelSets = context->level[deepIndex];
		auto& nextLevelSets = context->level[deepIndex - 1];
		auto& extendedLevelSets = context->extendedLevel[deepIndex];*/

		auto& level_prune_hold = levelPruneHold;
		auto wait = &context->level[deepIndex];
		if (wait->empty())
		{
			continue;
		}
		else
		{
		}
		auto sort = &context->extendedLevel[deepIndex];
		auto next = &context->level[deepIndex - 1];
		auto push_one = [&]
		{
			auto* child = wait->top();
			wait->pop();
			sort->push(child);
			child->eval();
			for (auto& it : child->children)
			{
				next->push(it);
			}
		};
		if (wait->empty())
		{
			// do nothing
		}
		else if (sort->size() >= level_prune_hold)
		{
			if (sort->top()->evalParm.status < wait->top()->evalParm.status)
			{
				push_one();
			}
		}
		else
		{
			do
			{
				push_one();
			} while (sort->size() < level_prune_hold && !wait->empty());
		}

		/*if (levelSets.empty()) continue;
		std::vector<TreeNode*> work;
		for (auto pi = 0; !levelSets.empty() && extendedLevelSets.size() < levelPruneHold; ++pi) {
			auto x = levelSets.top();
			work.push_back(x);
			levelSets.pop();
			extendedLevelSets.push(x);
		}
		std::for_each(std::execution::seq, work.begin(), work.end(), [&](TreeNode* tree) {
			tree->eval();
			std::lock_guard<std::mutex> guard(m);
			for (auto& child : tree->children)
				nextLevelSets.push(child);
			});*/
	}
}

TreeContext::Result TreeNode::getBest() {
	auto& lastLevel = context->level;
	TreeNode* best = lastLevel.front().empty() ? nullptr : lastLevel.front().top();
	if (best == nullptr)
		for (const auto& level : context->extendedLevel) {
			if (!level.empty()) {
				best = level.top();
				break;
			}
		}
	if (best == nullptr) return context->empty();
	std::deque<TreeNode*> record{ best };
	while (best->parent != nullptr) {
		record.push_front(best->parent);
		best = best->parent;
	}
	return { record[1]->evalParm.land_node, record[1]->isHold };
}

