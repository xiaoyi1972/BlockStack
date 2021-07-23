#include "tetrisBot.h"
#pragma warning(disable:4146)

#define USE_TEST 0

int BitCount(int n) {
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    n = (n + (n >> 4)) & 0x0f0f0f0f;
    n = n + (n >> 8);
    n = n + (n >> 16);
    return n & 0x3f;
}

std::string Tool:: printType(Piece x) {
    std::string a = "a";
    switch (x) {
        case Piece::None: a = "None"; break;
        case Piece::O: a = "O"; break;
        case Piece::J: a = "J"; break;
        case Piece::I: a = "I"; break;
        case Piece::T: a = "T"; break;
        case Piece::S: a = "S"; break;
        case Piece::Z: a = "Z"; break;
        case Piece::L: a = "L"; break;
        case Piece::Trash: a = "trash"; break;
    }
    return a;
}

std::string Tool:: printPath(std::vector<Oper> &path) {
    std::string a;
    for (auto x : path) {
        switch (x) {
            case Oper::Left: a += "l"; break;
            case Oper::Right: a += "r"; break;
            case Oper::SoftDrop: a += "d"; break;
            case Oper::HardDrop: a += "V"; break;
            case Oper::Hold: a += "v"; break;
            case Oper::Cw: a += "z"; break;
            case Oper::Ccw: a += "c"; break;
            case Oper::DropToBottom: a += "D"; break;
            default: break;
        }
    }
    return a;
}

std::string Tool:: printOper(Oper &x) {
    std::string a = "a";
    switch (x) {
        case Oper::None: a = "None"; break;
        case Oper::Left: a = "Left"; break;
        case Oper::Right: a = "Right"; break;
        case Oper::SoftDrop: a = "SoftDrop"; break;
        case Oper::HardDrop: a = "HardDrop"; break;
        case Oper::Hold: a = "Hold"; break;
        case Oper::Cw: a = "Cw"; break;
        case Oper::Ccw: a = "Ccw"; break;
        case Oper::DropToBottom: a = "DropToBottom"; break;
    }
    return a;
}

void Tool::printMap(TetrisMap &map) {
    for (auto i = 0; i < map.height; i++) {
        std::string str;
        for (auto j = 0; j < map.width; j++) {
            if (map(i, j))
                str += "[]";
            else
                str += "  ";
        }
      //  if (map.data[i] > 0)
        //    std::cout << str;
    }
}

std::string Tool::printNode(TetrisNode &node) {
    std::string str;
    str += "pos [";
    str += std::to_string(node.pos.x);
    str += " ";
    str += std::to_string(node.pos.y);
    str += "] rs ";
    str += std::to_string(node.rotateState);
    str += " type ";
    str += Tool::printType(node.type);
    return str;
}

template<>
std::vector<TetrisNode> TetrisBot::search<true>(TetrisNode& _first, TetrisMap& _map) {
    auto softDropDisable = true;
    auto isFilter = (_first.type == Piece::I || _first.type == Piece::Z || _first.type == Piece::S);
    auto isNodeT = _first.type == Piece::T;
    std::deque<searchNode> queue;
    std::vector<TetrisNode> result;
    std::unordered_set<TetrisNode>checked;
    std::unordered_set<searchNode>results;
 
    result.reserve(64);
    checked.reserve(64);
    if(isFilter) results.reserve(64);

    for (auto& nodes : _first.getHoriZontalNodes(_map)) {
        auto drop = nodes.drop(_map);
        auto deltaRS = std::abs(nodes.rotateState - _first.rotateState);
		auto deltaX = std::abs(nodes.pos.x - _first.pos.x);
		//drop.lastRotate |= isNodeT;
		if (deltaRS == 3) deltaRS = 1;
		queue.emplace_back(drop, 1 + deltaX + deltaRS);
        checked.emplace(drop.type,drop.pos,drop.rotateState);
    }

    while (!queue.empty()) {
        auto element = queue.front();
        auto &[node, count] = element;
        auto fixed = false;
        queue.pop_front();
		if (!node.check(_map, node.pos.x, node.pos.y + 1)) {
			fixed = true;
            if (!isFilter) {
                result.emplace_back(node.type, node.pos, node.rotateState);
                if (isNodeT) {
                    auto [spin, mini] = node.corner3(_map);
                    result.back().mini = mini;
                    result.back().spin = spin;
                    result.back().lastRotate = node.lastRotate;
                }
            }
            else {
                auto& infoKey = element;
                if (auto it = results.find(infoKey); it != results.end()) {
                    if (auto& keyCount = it->count; keyCount > count) {
                        results.erase(it);
                        results.emplace(infoKey.node, infoKey.count);
                    }
                    else if (node.rotateState == _first.rotateState) {
                        results.erase(it);
                        results.emplace(infoKey.node, infoKey.count);
                    }
                }
                else results.emplace(infoKey.node, infoKey.count);
            }
        }

		if (auto next = node; TetrisNode::shift(next, _map, -1, 0) && !next.open(_map)) {     //左
            if (checked.find(next) == checked.end()) {
                next.lastRotate = false;
				checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }

		if (auto next = node; TetrisNode::shift(next, _map, 1, 0) && !next.open(_map)) {     //右
            if (checked.find(next) == checked.end()) {
                next.lastRotate = false;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }

		if (auto next = node; TetrisNode::rotate(next, _map, true) && !next.open(_map)) {     //逆时针旋转
			if (checked.find(next) == checked.end()) {
                next.lastRotate = true;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }

		if (auto next = node; TetrisNode::rotate(next, _map, false) && !next.open(_map)) {     //顺时针旋转
            if (checked.find(next) == checked.end()) {
                next.lastRotate = true;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }
        
		if (!fixed)
		if (auto next = node.drop(_map); !next.open(_map) && checked.find(next) == checked.end()) { //软降到底
			next.lastRotate = false;
            checked.emplace(next.type, next.pos, next.rotateState);
			queue.emplace_back(next, count + 1);
        }

        if (!softDropDisable) {
			if (auto next = node; TetrisNode::shift(next, _map, 0, 1) && !next.open(_map)) { //单次软降
                if (checked.find(next) == checked.end()) {
                    next.lastRotate = false;
					checked.emplace(next.type, next.pos, next.rotateState);
                    queue.emplace_back(next, count + 1);
                }
            }
        }
    }

    if (isFilter) {
		for (auto& each : results) result.emplace_back(each.node.type, each.node.pos, each.node.rotateState);
	}

    return result;
}

template<>
std::vector<TetrisNode> TetrisBot::search<false>(TetrisNode& _first, TetrisMap& _map) {
    auto softDropDisable = true;
    auto isFilter = (_first.type == Piece::I || _first.type == Piece::Z || _first.type == Piece::S);
    auto isNodeT = _first.type == Piece::T;
    std::deque<searchNode> queue;
    std::vector<TetrisNode> result;
    std::unordered_set<TetrisNode>checked;
    std::unordered_set<searchNode>results;

    result.reserve(64);
    checked.reserve(64);
    if (isFilter) results.reserve(64);

    queue.emplace_back(_first, 0);

    while (!queue.empty()) {
        auto element = queue.front();
        auto& [node, count] = element;
        queue.pop_front();
        if (!node.check(_map, node.pos.x, node.pos.y + 1)) {
			//std::cout << element << "\n";
                auto& infoKey = element;
                if (auto it = results.find(infoKey); it != results.end()) {
					if (it->node.rotateState != _first.rotateState)
                        if (it->count > count || node.rotateState == _first.rotateState) {
							//std::cout << (*it) << "->" << element<<"\n";
                            results.erase(it);
                            results.emplace(infoKey.node, infoKey.count);
                        }
                }
				else results.emplace(infoKey.node, infoKey.count);
        }

        if (auto next = node; TetrisNode::shift(next, _map, -1, 0)) {     //左
            if (checked.find(next) == checked.end()) {
                next.lastRotate = false;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }

        if (auto next = node; TetrisNode::shift(next, _map, 1, 0)) {     //右
            if (checked.find(next) == checked.end()) {
                next.lastRotate = false;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
        }

        if (auto next = node; TetrisNode::rotate(next, _map, true)) {     //逆时针旋转
            if (auto iter = checked.find(next); iter == checked.end()) {
                next.lastRotate = true;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
           else {
                auto& infoKey = element;
                if (auto it = results.find(infoKey); it != results.end()) {
					it->node.lastRotate = true;
                }
            }
        }

        if (auto next = node; TetrisNode::rotate(next, _map, false)) {     //顺时针旋转
            if (auto iter = checked.find(next); iter == checked.end()) {
                next.lastRotate = true;
                checked.emplace(next.type, next.pos, next.rotateState);
                queue.emplace_back(next, count + 1);
            }
            else {
                auto& infoKey = element;
                if (auto it = results.find(infoKey); it != results.end()) {
                    it->node.lastRotate = true;
                }
            }
        }

		if (auto next = node.drop(_map); checked.find(next) == checked.end()) { //软降到底
            next.lastRotate = false;
            checked.emplace(next.type, next.pos, next.rotateState);
            queue.emplace_back(next, count + 1);
        }

        if (!softDropDisable) {
            if (auto next = node; TetrisNode::shift(next, _map, 0, 1)) { //单次软降
                if (checked.find(next) == checked.end()) {
                    next.lastRotate = false;
                    checked.emplace(next.type, next.pos, next.rotateState);
                    queue.emplace_back(next, count + 1);
                }
            }
        }
    }

	result.reserve(results.size());
    for (auto& each : results) {
        if (isNodeT) {
			auto& node = each.node;
            auto [spin, mini] = node.corner3(_map);
			node.mini = mini;
			node.spin = spin;
        }
        result.emplace_back(each.node);
	}

    return result;
}

auto TetrisBot::make_path(TetrisNode &startNode, TetrisNode &landPoint, TetrisMap &map, bool NoSoftToBottom)
->std::vector<Oper> {
    using OperMark = std::pair<TetrisNode, Oper>;
    auto softDropDisable = true;
    std::deque<TetrisNode> nodeSearch;
    std::unordered_map<TetrisNode, OperMark> nodeMark;
    auto &landNode = landPoint;

    auto mark = [&nodeMark](TetrisNode & key, OperMark value) {
        if (nodeMark.find(key) == nodeMark.end()) {
            nodeMark.insert({key, value});
            return true;
        } else return false;
    };

    auto buildPath = [&nodeMark, &NoSoftToBottom](TetrisNode & lp) {
        std::deque<Oper> path;
        TetrisNode *node = &lp;
        while (nodeMark.find(*node) != nodeMark.end()) {
            auto &result = nodeMark.at(*node);
            auto &next = std::get<0>(result);
            auto dropToSd = (path.size() > 0 && std::get<1>(result) == Oper::DropToBottom);
            auto softDropDis = node->pos.y - next.pos.y;
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
    nodeMark.insert({startNode, OperMark {TetrisNode::spawn(Piece::None), Oper::None }});
    auto disable_d = landNode.open(map);
    static constexpr Oper opers[] = {Oper::Cw, Oper::Ccw, Oper::Left, Oper::Right, Oper::SoftDrop, Oper::DropToBottom};
    if (landNode.type == Piece::T && landNode.typeTSpin == TSpinType::TSpinMini) disable_d = false;
    while (!nodeSearch.empty()) {
        auto next = nodeSearch.front();
        nodeSearch.pop_front();
        for (auto &oper : opers) {
            auto node = (oper == Oper::DropToBottom ? next.drop(map) : next);
            if (oper == Oper::SoftDrop && softDropDisable) continue;
            bool res = false;
            switch (oper) {
                case Oper::Cw: res = TetrisNode::rotate(node, map, true) && mark(node, OperMark {next, oper}); break;
                case Oper::Ccw: res = TetrisNode::rotate(node, map, false) && mark(node, OperMark {next, oper}); break;
                case Oper::Left: res = TetrisNode::shift(node, map, -1, 0) && mark(node, OperMark {next, oper}); break;
                case Oper::Right: res = TetrisNode::shift(node, map, 1, 0) && mark(node, OperMark {next, oper}); break;
                case Oper::SoftDrop: res = TetrisNode::shift(node, map, 0, 1) && mark(node, OperMark{next, oper}); break;
                case Oper::DropToBottom: res = mark(node, OperMark { next, oper}); break;
                default: break;
            }
            if (res) {
                if (node == landNode)
                    return buildPath(landNode);
                else if (oper == Oper::DropToBottom ? !disable_d : true)
                    nodeSearch.push_back(node);
            }
        }
    }
    return std::vector<Oper> {};
}

TetrisBot::EvalResult TetrisBot::evalute(TetrisNode &lp, TetrisMap &map, int clear, int tCount = 0) {
    TetrisBot::EvalResult evalResult;
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

    auto eval_landpoint = [&]() {
        auto LandHeight = map.height - map.roof;
        auto Middle = std::abs((lp.pos.x + 1) * 2 - map.width);
        int
        value = 0
                - LandHeight / map.height * 40
                + Middle * 0.2
                + (clear * 6);
        return value;
    };

    auto  eval_map = [](TetrisMap & map) {

        auto isCompact = [&](int num) {
            int filter = (1 << map.width) - 1 ;
            unsigned x = filter & (~num);
            return (x & x + (x & -x)) == 0;
        };

        struct {
            int colTrans;
            int rowTrans;
            int holes;
            int holeLines;
            int roof;
            int lowestRoof;
            double clearWidth;
            int wide[31];
            int messy;
            int wellDepth;
            int holeDepth;
            int wellNum[32];
            int holeNum[32];
            int HolePosyIndex;
        } m;

        memset(&m, 0, sizeof(m));
        m.roof = map.height - map.roof;
        m.lowestRoof = map.height;

        //行列变换
        for (auto j = (map.roof == 0 ? 0 : map.roof - 1); j < map.height; j++) {
            auto rt = (map.data[j] ^
                   (map.data[j] & (1 << (map.width - 1)) ? (map.data[j] >> 1) | (1 << (map.width - 1)) : map.data[j] >> 1));
            m.rowTrans += BitCount(rt);
            if (j < map.height - 1) {
            auto ct = (map.data[j] ^ map.data[j + 1]);
            m.colTrans += BitCount(ct);
            }
        }

        /*for (auto j = (map.roof == 0 ? 0 : map.roof - 1); j < map.height; j++) {
            auto cty = ((~map.data[j]) & ((1 << map.width) - 1));
            auto ctyF = ((~map.data[j + 1]) & ((1 << map.width) - 1));
            auto ct = (cty ^
                       (cty & (1 << (map.width - 1)) ? (cty >> 1) | (1 << (map.width - 1)) : cty >> 1));
            m.colTrans += BitCount(ct);
            if (j < map.height - 1) {
                auto rt = (cty ^ ctyF);
                m.rowTrans += BitCount(rt);
            }
        }*/
        struct
        {
            int ClearWidth;
        } a[40];

        //洞井改
        unsigned LineCoverBits = 0, widthL1 = map.width - 1;
        for (auto y = map.roof; y < map.height; y++) {
            LineCoverBits |= map.data[y];
            auto LineHole = LineCoverBits ^ map.data[y];
            if (BitCount(LineCoverBits) == map.width) {
                m.lowestRoof = std::min(m.lowestRoof, y);   //最低的屋檐
            }
            if (LineHole != 0) {
                m.holes += BitCount(LineHole);   //洞个数
                m.holeLines++;//洞行数
                a[m.HolePosyIndex].ClearWidth = 0;
                for (auto hy = y - 1; hy >= map.roof; hy--) {   //覆盖的洞层
                    auto CheckLine = LineHole & map.data[hy];
                    if (CheckLine == 0) break;
                    //m.clearWidth += (map.width - BitCount(map.data[hy])) * (map.height - hy);
                    a[m.HolePosyIndex].ClearWidth += (hy - 1) * BitCount(CheckLine);
                }
				++m.HolePosyIndex;
            }
            auto spacing = std::min<int>(widthL1, map.width - BitCount(LineCoverBits));
            if (isCompact(LineCoverBits)) ++m.wide[spacing]; //几宽
            else m.messy++; //离散的分布
            for (auto x = 0; x < map.width; x++) {
                if ((LineHole >> x) & 1) m.holeDepth += ++m.holeNum[x];     //洞深
                else m.holeNum[x] = 0;
                auto isWell = x == 0 ? (LineCoverBits & 3) == 2 :
                              (x == widthL1 ? ((LineCoverBits >> (widthL1 - 1)) & 3) == 1 :
                               ((LineCoverBits >> (x - 1)) & 7) == 5);
                if (isWell) m.wellDepth += ++m.wellNum[x];   //井深
            }
        }

        auto n = std::min({map.top[3], map.top[4], map.top[5], map.top[6]});
        m.roof = map.height - map.roof;//n;
        m.lowestRoof = map.height - m.lowestRoof;

#if USE_TEST
        auto  value = (0.
            - m.roof * 100
            );
        return value;
#else
        auto  value = (0.
            - m.colTrans * 160
            - m.rowTrans * 160
            - m.roof * 120
            - m.holeLines * 380
           // - m.clearWidth * 3
            - m.wellDepth * 100
            - m.holeDepth * 40
            //    - m.messy * 100
            );

        double rate = 32, mul = 1.0 / 4;
        for (int i = 0; i < m.HolePosyIndex; ++i, rate *= mul)
        {
            value -= a[i].ClearWidth * rate;
        }

        return value;
#endif 
    };

    auto tspinDetect = [&]() {
        bool finding2, finding3;
        auto *loopMap = &map;
        auto tc = 0;
        auto rowBelow = [&](int x, int y, int yCheck) {
            auto ifCover = (loopMap->top[x] <= y) |
                           ((loopMap->top[x + 1] <= y) << 1) |
                           ((loopMap->top[x + 2] <= y) << 2);
            return !(ifCover ^ yCheck);
        };

loop:
        finding2 =  finding3 = true;
        for (int y = loopMap->roof; (finding2 || finding3) && y < loopMap->height; ++y) {
            auto y0 = loopMap->row(y);
            auto y1 = loopMap->row(y - 1) ;
            auto y2 = loopMap->row(y - 2);
            auto y3 = loopMap->row(y - 3);
            auto y4 = loopMap->row(y - 4);
            for (int x = 0; finding2 && x < loopMap->width - 2 ; ++x) {
				if (auto value = 0, y2Check = (y2 >> x & 7); (((y0 >> x & 7) == 5) && ((y1 >> x & 7) == 0))) {
					if(((BitCount(y0) == loopMap->width - 1) && (value += 1)) &&
                       ((BitCount(y1) == loopMap->width - 3) && (value += 2)) &&
					   ((y2Check == 4 || y2Check == 1) /* && rowBelow(x, y - 2, y2Check)*/ && (value += 2))
                      ){
                        evalResult.t2Value += value;
                        finding2 = false;
                        if (tc++ < tCount) {
                            auto* virtualNode = &TreeContext::nodes[static_cast<int>(Piece::T)];
                            if (!tc) {
                                loopMap = nullptr;
                                loopMap = new TetrisMap{ map };
                            }
                            virtualNode->attach(*loopMap, Pos{ x, y - 2 }, 2);
                            goto loop;
                        }
                    }
					if (finding2) evalResult.t2Value += value;
                }
            }
            for (int x = 1; finding3 && x < loopMap->width - 2; ++x) {
                if (auto value = 0, mirror = 0;
                    (((y0 >> x & 7) == 3 || ((y0 >> x & 7) == 6 && (++mirror))) &&
                     (y1 >> x & 7) == (!mirror ? 1 : 4) && x < loopMap->width - 3)) {
                    if (auto y4Check = (y4 >> x & 7);
                        ((BitCount(y0) == loopMap->width - 1) && (value += 1)) &&
                        ((BitCount(y1) == loopMap->width - 2) && (value += 1)) &&
                        (((y2 >> x & 7) == (!mirror ? 3 : 6) && (y3 >> x & 7) == 0) && (value += 2)) &&
                        ((BitCount(y2) == loopMap->width - 1) && (value += 2)) &&
                        (((y3 >> x & 7) == 0) ? (value += 1) : (value = 0)) &&
                        ((y4Check == (!mirror ? 4 : 1) && rowBelow(x, y - 4, y4Check)) ? (value += 1) : ((value -= 2) && false))
                       ) {
                        evalResult.t3Value += value;
                        finding3 = false;
                        if (tc++ < tCount) {
                            auto *virtualNode = &TreeContext::nodes[static_cast<int>(Piece::T)];
                            if (!tc) {
                                loopMap = nullptr;
                                loopMap = new TetrisMap{map};
                            }
                            virtualNode->attach(*loopMap, Pos{x + (!mirror ? 1 : -1), y - 2}, !mirror ? 3 : 1);
                            goto loop;
                        }
                    }
                    if (finding3) evalResult.t3Value += value;
                    if (value > 3) finding3 = false;
                }
            }
        }
        //auto valueX = eval_map(*loopMap);
        auto valueX = eval_map(map);
        if (loopMap != &map) delete loopMap;
        loopMap = nullptr;
        return valueX;
    };

    auto getSafe = [&]() {
        int safe = INT_MAX;
        for (auto type = 0; type < 7; type++) {
            auto *virtualNode = &TreeContext::nodes[type];
            if (!virtualNode->check(map)) {
                safe = -1;
                break;
            } else {
                auto dropDis = virtualNode->getDrop(map);
                safe = std::min(dropDis, safe);
            }
        }
        return safe;
    };

    tCount = 0;

#if USE_TEST
	evalResult.value = (map.height - map.roof) * -100;
#else
    evalResult.value = /*eval_map(map) +*/  tspinDetect();
#endif 
    evalResult.clear = clear;
    evalResult.typeTSpin = lp.typeTSpin;
    evalResult.safe = getSafe();
    evalResult.count = map.count;
    evalResult.type = lp.type;
    evalResult.mapWidth = map.width;
    evalResult.mapHeight = map.height;
    return evalResult;
}


TetrisBot::Status TetrisBot::get(const TetrisBot::EvalResult& evalResult, TetrisBot::Status& status, 
	Piece hold, std::vector<Piece>* nexts) {
    auto &comboTable = TreeContext::comboTable;
    auto &nZCI = TreeContext::ComboAttackIndex;
    auto tableMax = int(comboTable.size());
    auto result = status;
	auto fullCount = evalResult.mapWidth * evalResult.mapHeight;
    
    result.eval = evalResult;
    result.value = evalResult.value;

    int attack = 0;
    result.t2Value = evalResult.t2Value;
    result.t3Value = evalResult.t3Value;

    if (evalResult.safe <= 0) {
        result.deaded = true;
        return result;
    }

    switch (evalResult.clear) {
        case 0:
            if (status.combo > 0 && status.combo < 3) result.like -= 2;
            result.combo = 0;
            if (status.underAttack > 0) {
                result.mapRise = std::max(0, std::abs(status.underAttack - attack));
                if (result.mapRise >= evalResult.safe) {
                    result.deaded = true;
                    return result;
                }
                result.underAttack = 0;
            }
            break;
        case 1:
            if (evalResult.typeTSpin == TSpinType::TSpinMini)
                attack += status.b2b ? 2 : 1;
            else if (evalResult.typeTSpin == TSpinType::TSpin)
                attack += status.b2b ? 3 : 2;
            result.b2b = evalResult.typeTSpin != TSpinType::None;
            break;
        case 2:
            if (evalResult.typeTSpin != TSpinType::None) {
                result.like += 8;
                attack += status.b2b ? 5 : 4;
			}
			else attack += 1;
            result.b2b = evalResult.typeTSpin != TSpinType::None;
            break;
        case 3:
            if (evalResult.typeTSpin != TSpinType::None) {
                result.like += 12;
                attack += status.b2b ? 8 : 6;
			}
			else attack += 2;
            result.b2b = evalResult.typeTSpin != TSpinType::None;
            break;
        case 4:
            result.like += 8;
            attack += (status.b2b ? 5 : 4);
            result.b2b = true;
            break;
    }


    auto comboAtt = 0;
    if (evalResult.clear > 0) {
        comboAtt= comboTable[std::max(std::min(tableMax - 1, (++result.combo) - 1), 0)];
		attack += comboAtt;
    }
   
    if (result.combo < 5)
        result.like -= 1.5 * result.combo;

    if (evalResult.count == 0 && result.mapRise == 0) {
        result.like += 20;
        attack += 6;
    }
    if (status.b2b && !result.b2b) {
        result.like -= 2;
        result.cutB2b = true;
    }

    if (attack > 0) result.underAttack = std::max(0, result.underAttack - attack);

	size_t t_expect = [hold = hold, nexts = nexts]() -> int {
        if (hold == Piece::T) return 0;
        for (size_t i = 0; i < nexts->size(); ++i) if ((*nexts)[i] == Piece::T) return i;
        return 14;
	}();

    switch (hold)
    {
    case Piece::T:
		if (evalResult.typeTSpin == TSpinType::None) result.like += 4;
        break;
    case Piece::I:
        if (evalResult.clear != 4) result.like += 2;
		break;
    }


//   bool wasteT = evalResult.type == Piece::T && evalResult.typeTSpin == TSpinType::None && evalResult.clear == 0;
    //result.maxAttack = std::max(attack, result.maxAttack);
    result.attack += attack;
    result.maxAttack = std::max(result.attack, result.maxAttack);
    result.maxCombo = std::max(result.combo, result.maxCombo);
    double base = 0;// (evalResult.count) / (23. * 10 - evalResult.count);

    //return result;
    // + (result.maxCombo - nZCI) * (result.maxCombo - nZCI) * 50
    // + (result.b2b ? ((!evalResult.clear ? 210 : 0) * (result.cutB2b ? 0.5 : 1)) : 0)
    // + (result.b2b && (!!evalResult.clear) ? 210 : 0)
    //  + (wasteT ? -400 : 0)


#if USE_TEST
    auto accValue = 0.+ (result.maxCombo);
	result.value = evalResult.value + accValue;
#else
   /* auto accValue = 0. + (
        // + result.maxAttack * 30
        +result.attack * 50
        + result.maxCombo * attack * 100
        //+ std::pow(std::max<int>(result.maxCombo - nZCI, 0), 2) * 50
        + result.like * 30
        + (result.b2b ? 100 : 0)
        + evalResult.t2Value * 30
        //+ (evalResult.safe >= 10 ? 40 * (evalResult.t3Value - result.underAttack) : 0)
        );*/
	auto accValue = 0. + (0
        + result.maxAttack * 40
        + result.attack * 256 * 3//rate
        + evalResult.t2Value * (t_expect < 8 ? 512 : 320) * 1.5
        + (evalResult.safe >= 12 ? evalResult.t3Value * (t_expect < 4 ? 10 : 8) * (result.b2b ? 512 : 256) / (6 + result.underAttack) : 0)
		+ (result.b2b ? 512 : 0)
        + result.like * 64
		) * std::max<double>(0.05, (fullCount - evalResult.count - result.mapRise * evalResult.mapWidth) / double(fullCount))
		+ result.maxCombo * (result.maxCombo - 1) * 40;
	result.value += accValue;
#endif 
    return result;
}

std::vector<TetrisNode>TreeContext::nodes = std::invoke([]() {
    std::vector<TetrisNode>_nodes;
    for (auto i = 0; i < 7; i++)
        _nodes.push_back(TetrisNode::spawn(static_cast<Piece>(i)));
    return _nodes;
});



TreeContext::~TreeContext() { root.reset(nullptr); return; }
bool TreeNodeCompare::operator()(TreeNode *const &a, TreeNode *const &b) const {return  a->evalParm.status < b->evalParm.status;}

void TreeContext::createRoot(TetrisNode &_node, TetrisMap &_map, std::vector<Piece> &dp, Piece _hold, int _b2b, 
	int _combo, int _underAttack) {
    nexts = dp;
	noneHoldFirstNexts = std::vector<Piece>{ dp.begin() + 1, dp.end() };
    auto depth = dp.size() + 1 + ((_hold == Piece::None && isOpenHold) ?  -1 : 0); // + 1;

    level.clear();
    extendedLevel.clear();
    level.resize(depth);
    extendedLevel.resize(depth);

    tCount = int (_hold == Piece::T && isOpenHold) + int(_node.type == Piece::T);
    for (const auto &i : dp) {
        if (i == Piece::T)
            tCount++;
    }

    if (depth > 0) {
        double ratio = 1.5;
        while (width_cache.size() < depth - 1)
            width_cache.emplace_back(std::pow(width_cache.size() + 2, ratio));
        div_ratio = 2 / *std::max_element(width_cache.begin(), width_cache.end());
    }
    width = 0;

    tCount = std::max(0, --tCount);
    TreeNode *tree = new TreeNode;
    tree->context = this;
    tree->nexts = &nexts;
    tree->cur = _node.type;
    tree->map = _map;
	tree->hold  = _hold;
    tree->evalParm.status.underAttack = _underAttack;
    tree->evalParm.status.b2b = _b2b;
    tree->evalParm.status.combo = _combo;
    tree->evalParm.status.maxCombo = _combo;
    update(tree);
    //root.reset(tree);
}

void TreeContext::update(TreeNode* node) {
    count = 0;
    if (root == nullptr) {
        root.reset(node);
        return;
    }
    auto it = std::find_if(root->children.begin(), root->children.end(), [&](auto val) {return (*node) == (*val);});
    if (it != root->children.end()) {
        auto selected = *it;
        selected->parent = nullptr;
		selected->evalParm.status = node->evalParm.status;
        auto previewLength = level.size();
        std::function<void(TreeNode*)> save = [&](TreeNode* t) {
            if (t == nullptr) return;
			t->nextIndex--;
            t->isHold = false;
            if (t->nexts == (&noneHoldFirstNexts) && t->hold != Piece::None && isOpenHold) t->nexts = & nexts;
            if (t->parent != nullptr) {
				t->evalParm.status = TetrisBot::get(t->evalParm.status.eval, t->parent->evalParm.status, t->hold,t->nexts);
                if (!t->extended) {
                    if (t->cur == Piece::None) 
                        t->cur= (t->nextIndex - 1 == t->nexts->size() ? Piece::None : t->nexts->at(t->nextIndex - 1));
                    level[previewLength - t->nextIndex].push(t);
                }
                else extendedLevel[previewLength - t->nextIndex].push(t);
            }
            else {
                if(isOpenHold)
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

void TreeContext::treeDelete(TreeNode *t) {
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

TreeContext::Result TreeContext::empty() {return {TetrisNode::spawn(root->cur), false};}
TreeContext::Result TreeContext::getBest() {return root->getBest();}

TreeNode::TreeNode(TreeContext *_ctx, TreeNode *_parent, Piece _cur, TetrisMap _map,
                   int _nextIndex, Piece _hold, bool _isHold,  EvalParm &_evalParm): 
    map(std::move(_map)),evalParm(_evalParm) {
    context = _ctx;
    parent = _parent;
    cur = _cur;
    // map = _map;
    nexts = _parent->nexts;
    nextIndex = _nextIndex;
    hold = _hold;
    isHold = _isHold;
    //evalParm = _evalParm;
}

void TreeNode::printInfoReverse(TetrisMap &map_,TreeNode *_test = nullptr) {
    if (false) Tool::printMap(map_);
    TreeNode *test = _test == nullptr ? this : _test;
    Piece first = test->  evalParm.land_node.type;
    auto isChange = false;
    while (test != nullptr) {
        if (test->isHold) {
            isChange = true;
            first = test->  evalParm.land_node.type;
        }
       /* if (test->parent == nullptr && isChange)
            qDebug() <<  Tool::printType(first);
        else
            qDebug() << (test->node == nullptr ? "None" : Tool::printType(test->node->type));*/
        Tool::printMap(test->map);
        test = test->parent;
    }
}

TreeNode *TreeNode::generateChildNode(TetrisNode &i_node, bool _isHoldLock, Piece _hold, bool _isHold) {
    auto next_node = (nextIndex == nexts->size() ? Piece::None : nexts->at(nextIndex));
    auto map_ = map;
    auto clear = i_node.attach(map_);
    auto status_ = TetrisBot::get(TetrisBot::evalute(i_node, map_, clear, context->tCount),
		evalParm.status, hold, nexts);
    auto ifOpen = i_node.open(map);
    //auto needSd = this->evalParm.needSd + int(i_node.type != Piece::T ?
      //  !ifOpen : !i_node.lastRotate || (i_node.lastRotate && !clear));
    //status_.value -= needSd * 150;
    EvalParm evalParm_ = { i_node, static_cast<int>(clear), status_, 0/*needSd*/};
    TreeNode *new_tree = nullptr;
	if (/*(i_node.type == Piece::T ? true : ifOpen) &&*/ !status_.deaded) {
        context->count++;
        new_tree = new TreeNode{context, this, next_node, map_, nextIndex + 1, _hold, _isHold, evalParm_};
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
    for (auto &i_node : TetrisBot::search(*(context->getNode(cur)), map))
        generateChildNode(i_node,  isHoldLock, hold, bool(isHold));
}

void TreeNode::search_hold(bool op, bool noneFirstHold) {
    if (hold == Piece::None || nexts->size() == 0) {
        auto hold_save = hold;
        if (hold == Piece::None) {
            hold = nexts->front();
            search_hold(false, true);
        }
        hold = hold_save;
        return;
    }
    if (cur == hold) {
        if(!isHoldLock)
        for (auto& i_node : TetrisBot::search(*(context->getNode(cur)), map))
        generateChildNode(i_node, true, hold, false);
    } else if (cur != hold) {
        if(!isHoldLock)
		for (auto& i_node : TetrisBot::search(*(context->getNode(cur)), map)) {
			auto childTree = generateChildNode(i_node, true, noneFirstHold ? Piece::None : hold, bool(op ^ false));
        }
		for (auto& i_node : TetrisBot::search(*(context->getNode(hold)), map)) {
			if (noneFirstHold) nexts = &context->noneHoldFirstNexts;
            auto childTree = generateChildNode(i_node, true, cur, bool(op ^ true));
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
    std::mutex m;
    if (context->width == 0) { //for root
		auto& firstLevel = context->level[previewLength];
        auto len = children.empty() ? 0 : children.size();
        if (this->eval()) {
            for (auto index = len; index < this->children.size(); index++)
                firstLevel.push(this->children[index]);
        }
        context->width = 2;
    } else context->width += 1;
    while (--i > 0) {
        auto levelPruneHold = std::max<size_t>(1, size_t(context->width_cache[i - 1] * context->width * context->div_ratio));
        auto &deepIndex = i;
        auto &levelSets = context->level[deepIndex];
        auto &nextLevelSets = context->level[deepIndex - 1];
        auto &extendedLevelSets = context->extendedLevel[deepIndex];
        if (levelSets.empty()) continue;
        std::vector<TreeNode *> work;
		for (auto pi = 0; !levelSets.empty() && extendedLevelSets.size() < levelPruneHold; ++pi) {
			auto x = levelSets.top();
            work.push_back(x);
            levelSets.pop();
            extendedLevelSets.push(x);
        }
        std::for_each(std::execution::seq, work.begin(), work.end(), [&](TreeNode  * tree) {
            tree->eval();
            std::lock_guard<std::mutex> guard(m);
            for (auto &x : tree->children)
                nextLevelSets.push(x);
        });
    }
}

TreeContext::Result TreeNode::getBest() {
    auto &lastLevel = context->level;
    TreeNode *best = lastLevel.front().empty() ? nullptr : lastLevel.front().top();
    if (best == nullptr)
        for (const auto &level : context->extendedLevel) {
            if (!level.empty()) {
                best = level.top();
                break;
            }
        }
    if (best == nullptr) return context->empty();
    std::deque<TreeNode *> record{best};
    while (best->parent != nullptr) {
        record.push_front(best->parent);
        best = best->parent;
    }
	return { record[1]->evalParm.land_node, record[1]->isHold};
}

#undef USE_TEST

