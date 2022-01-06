#include "tetrisGame.h"

void Random::getBag() {
    constexpr bool  test = false;
	if constexpr (test) {
        //bag = std::vector<Piece>{ 7 , Piece::I };
		bag = { Piece::L,Piece::I,Piece::L,Piece::I,Piece::L,Piece::I,Piece::L };
        return;
    }
    else {
        std::vector<Piece> bagSets{ Piece::O, Piece::I, Piece::T, Piece::L, Piece::J, Piece::S, Piece::Z };
        while (bag.size() < 7) {
            auto it = std::next(bagSets.begin(), randGen() % bagSets.size());
            auto piece = *it;
            bagSets.erase(it);
            bag.push_back(piece);
        }
	}
}

Piece Random::getOne() {
    if (bag.empty()) { //[0,1,2,3,4,5,6]
        getBag();
        if (displayBag.empty()) {
            displayBag = bag;
            bag.resize(0);
            getBag();
        }
    }
    if (displayBag.size() < 7) {
        auto first = bag.front();
        bag.erase(bag.begin());
        displayBag.push_back(first);
    }
    auto num = displayBag.front();
    displayBag.erase(displayBag.begin());
    return num;
}

void Hold::reset() { 
    able = true; 
}

int Hold::exchange(TetrisNode& cur, Random& rS, const TetrisMap& map, int dySpawn) {
    auto curType = cur.type;
    auto res = 0;
    if (able) {
        if (type) {
            std::swap(*type, curType);
			cur = TetrisNode::spawn(curType, &map, dySpawn), res = 1;
        }
        else {
            type = curType;
			cur = TetrisNode::spawn(rS.getOne(), &map, dySpawn), res = 2;
        }
        able = !able;
    }
    return res;
}


int Recorder::timeRecord() {
    using std::chrono::high_resolution_clock;
    using std::chrono::milliseconds;
    static auto startTime = high_resolution_clock::now();
    auto now = high_resolution_clock::now();
    milliseconds timeDelta = std::chrono::duration_cast<milliseconds>(now - startTime);
    startTime = now;
    return timeDelta.count();
}

void Recorder::clear() {
    time.clear();
    oper.clear();
    playIndex = 0;
}

std::ostream& operator<<(std::ostream& out, const Recorder& R) {
	out << R.seed << R.playIndex << R.timeAll << R.gameType << R.firstTime;
	out << R.time.size() << R.oper.size();
	out.write(reinterpret_cast<char const*>(R.time.data()), R.time.size() * sizeof(decltype(R.time)::value_type));
	out.write(reinterpret_cast<char const*>(R.oper.data()), R.oper.size() * sizeof(decltype(R.oper)::value_type));
	return out;
}

std::istream& operator>>(std::istream& in, Recorder& R) {
	auto timeLen = 0, operLen = 0;
	in >> R.seed >> R.playIndex >> R.timeAll >> R.gameType >> R.firstTime;
	in >> timeLen >> operLen;
	R.time.resize(timeLen);
	R.oper.resize(operLen);
	in.read(reinterpret_cast<char*>(R.time.data()), R.time.size() * sizeof(decltype(R.time)::value_type));
	in.read(reinterpret_cast<char*>(R.oper.data()), R.oper.size() * sizeof(decltype(R.oper)::value_type));
	return in;
}

Recorder Recorder::readStruct(const std::string& ba) {
    Recorder obj;
    std::stringstream in;
    in.str(ba);
    in >> obj;
    return obj;
}

std::string Recorder::writeStruct(Recorder& obj) {
    std::stringstream out;
    out << obj;
    std::string ba{ out.str() };
    return ba;
}

void Recorder::add(Oper _operation, int _time) {
    auto len = time.size();
    if (len == 0)
        firstTime = _time;
    oper.push_back(_operation);
    time.push_back(_time);
}

std::tuple<int, Oper> Recorder::play() {
    auto playIndex_ = playIndex++;
    return{ playIndex >= time.size() ? 0 : time[playIndex], playIndex_ >= oper.size() ? Oper::None : oper[playIndex_] };
}

void Recorder::reset() { 
    playIndex = 0; 
}

auto Recorder::isEnd() {
    return playIndex < time.size() ? false : true; 
}

int TetrisGame::sendTrash(const std::tuple<TSpinType, int, bool, int> &status) {
    int attackTo = 0;
    auto [spin, clear, b2b, combo] = status;
    int clears[] = {0, 0, 1, 2, 4};
    if (spin != TSpinType::None) {
        switch (clear) {
        case 1: attackTo += ((spin == TSpinType::TSpinMini ? 1 : 2) + b2b); break;
        case 2: attackTo += (4 + b2b); break;
        case 3: attackTo += (b2b ? 6 : 8); break;
        default: break;
        }
    } else attackTo += (clears[clear] + (clear == 4 ? b2b : 0));
	if (map.count == 0) attackTo += 6;
	if (combo > 0) attackTo += Modes::versus::comboTable[std::min<int>(combo - 1, Modes::versus::comboTable.size() - 1)];
    return attackTo;
}

void TetrisGame::opers(Oper a) {
    switch (a) {
    case Oper::Left: left(); break;
    case Oper::Right: right(); break;
    case Oper::SoftDrop: softDrop(); break;
    case Oper::HardDrop: hardDrop(); break;
    case Oper::Cw: cw(); break;
    case Oper::Ccw: ccw(); break;
    case Oper::Hold: hold(); break;
	case Oper::DropToBottom:dropToBottom(); break;
    default: break;
    }
}

void TetrisGame::hold() {
	auto res = hS.exchange(tn, rS, map,dySpawn);
    if (res > 0) {
        if (!gd.isReplay)
            record.add(Oper::Hold, record.timeRecord());
        //    passHold();
        tn.lastRotate = false;
    }
    if (res == 2) {
        /*passNext();
        passPiece(true);
        updateCall();*/
    }
}

void TetrisGame::left() {
	if (auto vary = TetrisNode::shift(tn, map, -1, 0); vary) {
		if (!gd.isReplay) record.add(Oper::Left, record.timeRecord());
        tn = *vary;
        tn.lastRotate = false;
    }
}

void TetrisGame::right() {
    if (auto vary = TetrisNode::shift(tn, map, 1, 0);vary) {
		if (!gd.isReplay) record.add(Oper::Right, record.timeRecord());
        tn = *vary;
        tn.lastRotate = false;
    }
}

void TetrisGame::softDrop() {
	if (auto vary = TetrisNode::shift(tn, map, 0, -1); vary) {
        if (!gd.isReplay) record.add(Oper::SoftDrop, record.timeRecord());
        tn = *vary;
        tn.lastRotate = false;
    }
}

void TetrisGame::ccw() {
    if (auto vary = TetrisNode::rotate<false>(tn, map); vary) {
        if (!gd.isReplay) record.add(Oper::Ccw, record.timeRecord());
        tn = *vary;
        tn.lastRotate = true;
    }
}

void TetrisGame::cw() {
    if (auto vary = TetrisNode::rotate<true>(tn, map); vary) {
        if (!gd.isReplay) record.add(Oper::Cw, record.timeRecord());
        tn = *vary;
        tn.lastRotate = true;
    }
}

void TetrisGame::dropToBottom() {
	if (auto dropDis = tn.getDrop(map); dropDis != 0) {
        tn.y -= dropDis;
        if (!gd.isReplay)
            record.add(Oper::DropToBottom, record.timeRecord());
        tn.lastRotate = false;
    }
}

void TetrisGame::hardDrop() {
    if (!gd.isReplay)record.add(Oper::HardDrop, record.timeRecord());
    hS.reset();
    auto dropDis = tn.getDrop(map);
    if (dropDis > 0) tn.lastRotate = false;
    tn.y -= dropDis;
	if (tn.type == Piece::T && tn.lastRotate) {
        auto [spin, mini] = tn.corner3(map);
        if (spin) tn.typeTSpin = mini ? TSpinType::TSpinMini : TSpinType::TSpin;
    }
    auto [pieceChanges, clear] = tn.attachs(map);
    std::sort(pieceChanges.begin(), pieceChanges.end(), [](auto& lhs, auto& rhs) {return lhs.y == rhs.y ? lhs.x < rhs.x : lhs.y > rhs.y; });
	for (auto it = pieceChanges.begin(); !clear.empty() && it != pieceChanges.end(); ) {
        auto dec = 0;
        auto del = false;
		for (auto n : clear) {
			if (it->y == n) {
				del = true;
                break;
			}
            else if (it->y > n) dec++;
		}
		if (del) it = pieceChanges.erase(it);
        else { 
            if (it == pieceChanges.begin() && dec == 0) break;
            it->y -= dec; ++it; 
        }
    }
    gd.pieceChanges = pieceChanges;
    if (clear.size() == 4 || (clear.size() > 0 && tn.typeTSpin != TSpinType::None))  gd.b2b++;
    else if (clear.size() > 0) gd.b2b = 0;
    gd.combo = clear.size() > 0 ? gd.combo + 1 : 0;
    auto attack = sendTrash(std::tuple<TSpinType, int, bool, int> {tn.typeTSpin, clear.size(), gd.b2b > 1, gd.combo});
    gd.pieces++;
    gd.clear += clear.size();
    auto piece = rS.getOne();
    tn = TetrisNode::spawn(piece,&map, dySpawn);

    std::visit(overloaded{
	 [&] (Modes::sprint &arg){},
     [&, &clear = clear, &pieceChanges = pieceChanges](Modes::dig &arg) {arg.handle(this, clear, pieceChanges); },
     [&, &clear = clear, &pieceChanges = pieceChanges](Modes::versus &arg) {
     arg.handle(this, clear.size() == 0, attack, pieceChanges); },
     }, gm);

    if (!tn.check(map)) //check is dead
        gd.deaded = true;
}

void TetrisGame::restart() {
     rS = Random{};
     hS = Hold{};
	 gd = gameData{};
    map = TetrisMapEx{ 10, 40 };
    tn = TetrisNode { TetrisNode::spawn(rS.getOne(),&map,dySpawn) };
    new(&gm) decltype(gm){playing_mode{}};
}


