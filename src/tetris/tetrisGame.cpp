#include "tetrisGame.h"

void Random::getBag() {
	if constexpr (My::test) {
        //bag = std::vector<Piece>{ 7 , Piece::J };

        bag = { Piece::S,Piece::J,Piece::L,Piece::O,Piece::T,Piece::Z,Piece::I };

        //bag = { Piece::O,Piece::I,Piece::J,Piece::S,Piece::L,Piece::Z,Piece::T };
        
        //bag = { Piece::O, Piece::I, Piece::T, Piece::L, Piece::T, Piece::S, Piece::Z };
		//bag = { Piece::O, Piece::I, Piece::T, Piece::L, Piece::J, Piece::S, Piece::Z };
        //bag = { Piece::O, Piece::T, Piece::Z, Piece::S, Piece::I, Piece::L, Piece::J };
        //bag = { Piece::S, Piece::T, Piece::J, Piece::Z, Piece::I, Piece::O, Piece::L };
        //bag = { Piece::T, Piece::J, Piece::S, Piece::Z, Piece::I, Piece::L, Piece::O};
        //bag = { Piece::I, Piece::Z, Piece::J, Piece::T, Piece::S, Piece::O, Piece::L };
        return;
    }
    else {
        std::vector<Piece> bagSets{ Piece::O, Piece::I, Piece::T, Piece::L, Piece::J, Piece::S, Piece::Z };
        while (bag.size() < 7) {
//#define CLASSIC
            auto it = std::next(bagSets.begin(), randGen() % bagSets.size());
            auto piece = *it;
#ifndef  CLASSIC
            bagSets.erase(it);
#endif
            bag.push_back(piece);
#undef CLASSIC
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
		if (type != Piece::None) {
            std::swap(type, curType);
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
    return;
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
	static std::array clearsInfo{ "", "", "double", "triple", "tetris" };
    auto &[spin, clear, b2b, combo] = status;
    int attackTo = 0;
    int clears[] {0, 0, 1, 2, 4};
    gd.clearTextInfo.clear();
    if (spin != TSpinType::None) {
        gd.clearTextInfo += "TSpin";
        switch (clear) {
        case 1: attackTo += ((spin == TSpinType::TSpinMini ? 1 : 2) + b2b); break;
        case 2: attackTo += (4 + b2b); break;
        case 3: attackTo += (b2b ? 8 : 6); break;
        default: break;
        }
        gd.clearTextInfo += std::to_string(clear - (spin == TSpinType::TSpinMini));
        gd.clearTextInfo += " ";
    }
    else {
        attackTo += (clears[clear] + (clear == 4 ? b2b : 0));
        gd.clearTextInfo += clearsInfo[clear];
		if (!gd.clearTextInfo.empty()) 
            gd.clearTextInfo += " ";
    }
	if (clear > 0 && b2b)
        gd.clearTextInfo += "b2b ";
    if (map.count == 0) {
        attackTo += 6;
        gd.clearTextInfo += "Perfect Clear ";
    }
    if (combo > 0) {
        attackTo += Modes::versus::comboTable[std::min<int>(combo, Modes::versus::comboTable.size() - 1)];
		gd.clearTextInfo += "combo" + std::to_string(combo);
    }
    if (attackTo > 0) {
		gd.clearTextInfo = "+" + std::to_string(attackTo) + " " + gd.clearTextInfo;
    }
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

TetrisNode TetrisGame::gen_piece(Piece piece) {
    return TetrisNode::spawn(piece, &map, dySpawn);
}

void TetrisGame::hardDrop() {
    if (!gd.isReplay)
        record.add(Oper::HardDrop, record.timeRecord());
    hS.reset();
    auto dropDis = tn.getDrop(map);
    if (dropDis > 0) tn.lastRotate = false;
    tn.y -= dropDis;
    if (tn.type == Piece::T && tn.lastRotate) {
        auto [spin, mini] = tn.corner3(map);
        if (spin)
            tn.typeTSpin = mini ? TSpinType::TSpinMini : TSpinType::TSpin;
    }
    auto [pieceChanges, clear] = tn.attachs(map);
    if (clear.size() > 1 && tn.typeTSpin == TSpinType::TSpinMini)
        tn.typeTSpin = TSpinType::TSpin;

    std::sort(pieceChanges.begin(), pieceChanges.end(), [](auto& lhs, auto& rhs) {
        return lhs.y == rhs.y ? lhs.x < rhs.x : lhs.y > rhs.y; });
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
    if (clear.size() == 4 || (clear.size() > 0 && tn.typeTSpin != TSpinType::None))  
        gd.b2b++;
    else if (clear.size() > 0) 
        gd.b2b = 0;
    gd.combo = clear.size() > 0 ? gd.combo + 1 : 0;
	auto attack = sendTrash({ tn.typeTSpin,clear.size(), gd.b2b > 1, gd.combo - 1 });
    gd.pieces++;
    gd.clear += clear.size();
    auto piece = rS.getOne();
	tn = gen_piece(piece);

	//std::osyncstream(std::cout) << " fact attack:" << attack << "\t" << gd.clearTextInfo << "\n";

	std::visit(overloaded{
	 [&](Modes::sprint& arg) {},
	 [&, &clear = clear, &pieceChanges = pieceChanges](Modes::dig& arg) {arg.handle(this, clear, pieceChanges); },
	 [&, &clear = clear, &pieceChanges = pieceChanges](Modes::versus& arg) {
	 gd.attack += attack;
	 arg.handle(this, clear.size() == 0, attack, pieceChanges); },
	}, gm);

    gd.pieceChanges = pieceChanges;

    if (!tn.check(map)) { //check is dead
        //std::cout << "dead:\n" << tn.mapping(map) << "\n";
        gd.deaded = true;
    }
}

void TetrisGame::restart() {
	rS = Random{}, hS = Hold{}, gd = gameData{};
    map = TetrisMapEx{ 10, 40 };

    //map = TetrisMapEx(10, 10);
    //dySpawn = 0;

    tn = TetrisNode{ TetrisNode::spawn(rS.getOne(),&map,dySpawn) };
    switchMode(mode, true);

    if constexpr (My::test) {

        //for (int i = 0; i < 18; i++)
          //  map[i] = 0111111110_r;

        //TetrisMap map(10, 40);
        /*map[21] = 0000000010_r;
        map[20] = 1100000011_r;
        map[19] = 1110000011_r;
        map[18] = 1110100111_r;
        map[17] = 1111110111_r;
        map[16] = 1111110111_r;
        map[15] = 1111110111_r;
        map[14] = 1111110111_r;
        map[13] = 1101111111_r;
        map[12] = 1110111111_r;
        map[11] = 1110111111_r;
        map[10] = 1110111111_r;
        map[9] = 1110111111_r;
        map[8] = 1110111111_r;
        map[7] = 1111111110_r;
        map[6] = 1111101111_r;
        map[5] = 1111101111_r;
        map[4] = 1011111111_r;
        map[3] = 1011111111_r;
        map[2] = 1011111111_r;
        map[1] = 1111011111_r;
        map[0] = 1111011111_r;*/


        //map[i] = 1111111111_r ^ (1 << (i % 10));
        //map[i] = 1111110000_r;
        //map[0] = 1111111110_r;

        /*
        //bt cannon
        map[6] = 1111100111_r;
        map[5] = 1111000111_r;
        map[4] = 1111011111_r;
        map[3] = 1111001111_r;
        map[2] = 1111000111_r;
        map[1] = 1111101111_r;
        map[0] = 1111011111_r;
        */

        
		//dt cannon
		/*map[6] = 0011111111_r;
		map[5] = 0001111111_r;
		map[4] = 1101111111_r;
		map[3] = 1001111111_r;
		map[2] = 1000111111_r;
		map[1] = 1101111111_r;
		map[0] = 1101111111_r;*/
        

        /*
        //tst
        map[5] = 1111100111_r;
        map[4] = 1111000111_r;
        map[3] = 1111011111_r;
        map[2] = 1111001111_r;
        map[1] = 1111001111_r;
        //map[0] = 1111101111_r;
        */

        /*map[17] = 1000000100_r;
        map[16] = 1000001100_r;
        map[15] = 1000001111_r;
        map[14] = 1100001111_r;
        map[13] = 1000001111_r;
        map[12] = 1101111111_r;
        map[11] = 1101111111_r;
        map[10] = 1101111111_r;
        map[9] = 1101111111_r;
        map[8] = 1101111111_r;
        map[7] = 1111111110_r;
        map[6] = 1111110111_r;
        map[5] = 1111110111_r;
        map[4] = 1111110111_r;
        map[3]=  1111110111_r;
        map[2] = 1011111111_r;
        map[1] = 1011111111_r;
        map[0] = 1011111111_r;*/


        {
            map[19] = 0000111100_r;
            map[18] = 0000111100_r;
            map[17] = 1111111110_r;
            map[16] = 1111110111_r;
            map[15] = 1111110111_r;
            map[14] = 1111110111_r;
            map[13] = 1101111111_r;
            map[12] = 1110111111_r;
            map[11] = 1110111111_r;
            map[10] = 1110111111_r;
            map[9] = 1110111111_r;
            map[8] = 1110111111_r;
            map[7] = 1111111110_r;
            map[6] = 1111101111_r;
            map[5] = 1111101111_r;
            map[4] = 1011111111_r;
            map[3] = 1011111111_r;
            map[2] = 1011111111_r;
            map[1] = 1111011111_r;
            map[0] = 1111011111_r;
        }

        /*map[0] = 1111111110_r;
        for (int i = 1; i < 20; i++) {
            map[i] = 1111110000_r;
        }*/
        map.update(true);

        if (mode == 0) {
           hS.type = Piece::I;
           std::get<Modes::versus>(gm).addTrash(2);
        }
    }
    else {
        if (mode == 2) {
            std::get<Modes::dig>(gm).digModAdd(this, std::get<Modes::dig>(gm).digRowUpper, true);
        }
    }
}

void TetrisGame::switchMode(std::size_t type, bool apply) {
	if (!apply)
		mode = type;
	else
		switch (type) {
		case 0:  gm = Modes::versus{}; break;
		case 1:  gm = Modes::sprint{}; break;
		case 2:  gm = Modes::dig{}; break;
		}
}