#include "tetrisGame.h"

TetrisGame::TetrisGame() {

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
    default: break;
    }
}

void TetrisGame::hold() {
    auto res = hS.exchange(tn,rS);
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
    if (TetrisNode::shift(tn, map, -1, 0)) {
        if (!gd.isReplay)
            record.add(Oper::Left, record.timeRecord());
        tn.lastRotate = false;
    }
}

void TetrisGame::right() {
    if (TetrisNode::shift(tn, map, 1, 0)) {
        if (!gd.isReplay)
            record.add(Oper::Right, record.timeRecord());
        tn.lastRotate = false;
    }
}

void TetrisGame::softDrop() {
    if (TetrisNode::shift(tn, map, 0, 1)) {
        if (!gd.isReplay)
            record.add(Oper::SoftDrop, record.timeRecord());
        tn.lastRotate = false;
    }
}

void TetrisGame::hardDrop() {
    if (!gd.isReplay)record.add(Oper::HardDrop, record.timeRecord());
    hS.reset();
    auto dropDis = tn.getDrop(map);
    if (dropDis > 0) tn.lastRotate = false;
    tn.pos.y += dropDis;
	if (tn.type == Piece::T && tn.lastRotate) {
        auto [spin, mini] = tn.corner3(map);
        if (spin) tn.typeTSpin = mini ? TSpinType::TSpinMini : TSpinType::TSpin;
    }
    auto [pieceChanges, clear] = tn.attachs(map);
    if (clear.size() == 4 || (clear.size() > 0 && tn.typeTSpin != TSpinType::None))
        gd.b2b++;
    else if (clear.size() > 0)
        gd.b2b = 0;
    gd.combo = clear.size() > 0 ? gd.combo + 1 : 0;
    auto attack = sendTrash(std::tuple<TSpinType, int, bool, int> {tn.typeTSpin, clear.size(), gd.b2b > 1, gd.combo});
    gd.pieces++;
    gd.clear += clear.size();
    auto piece = rS.getOne();
    tn = TetrisNode::spawn(piece);

    std::visit(overloaded{
	 [&] (Modes::sprint &arg){},
     [&, &clear = clear, &pieceChanges = pieceChanges](Modes::dig &arg) {arg.handle(this, clear, pieceChanges); },
     [&, &clear = clear, &pieceChanges = pieceChanges](Modes::versus &arg) {
     arg.handle(this, clear.size() == 0, attack, pieceChanges); },
     }, gm);

    if (!tn.check(map)) //check is dead
        gd.deaded = true;
}

void TetrisGame::ccw() {
    if (TetrisNode::rotate(tn, map, false)) {
        if (!gd.isReplay)
            record.add(Oper::Ccw, record.timeRecord());
        tn.lastRotate = true;
    }
}

void TetrisGame::cw() {
    if (TetrisNode::rotate(tn, map)) {
        if (!gd.isReplay)
            record.add(Oper::Cw, record.timeRecord());
        tn.lastRotate = true;
    }
}
