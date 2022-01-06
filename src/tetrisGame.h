#ifndef TETRISGAME_H
#define TETRISGAME_H

#ifdef min
#undef min
#endif //min

#ifdef max
#undef max
#endif //max

#include<vector>
#include<deque>
#include"tetrisBase.h"
#include<chrono>
#include<variant>
#include<type_traits>
#include<utility>
#include<array>
#include<list>
#include<sstream>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; // helper type for the visitor #4
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>; // explicit deduction guide (not needed as of C++20)


class Random {
public:
	int seed;
	std::vector<Piece> bag, displayBag;
	std::default_random_engine randGen;

	Random(unsigned _seed = std::chrono::system_clock::now().time_since_epoch().count()) :seed(_seed) {
		randGen.seed(seed);
	}

	void getBag();
	Piece getOne();
};


class Hold {
public:
	bool able;
	std::optional<Piece>  type;

	Hold() :able(true), type(std::nullopt) {
	}

	void reset();
	int exchange(TetrisNode&, Random&, const TetrisMap&, int);
};

class Recorder {
public:
	int seed, firstTime = 0;
	int playIndex = 0, timeAll = 0, gameType = 1;
	std::vector<int>time;
	std::vector<Oper>oper;

	Recorder(int _seed = -1) :seed(_seed) {
	}

	int timeRecord();
	void clear();
	void add(Oper _operation, int _time);
	std::tuple<int, Oper>play();
	void reset();
	auto isEnd();
	static Recorder readStruct(const std::string&);
	static std::string writeStruct(Recorder&);
	friend std::ostream& operator<<(std::ostream&, const Recorder&);
	friend std::istream& operator>>(std::istream&, Recorder&);
};

namespace Modes {
	struct dig {
		int digRows = 10;
		int digRowsEnd = 100;

		template<class T>
		void digModAdd(T* tg, int needDigRows, bool isInit) {
			auto& map = tg->map;
			auto& randSys = tg->rS;
			for (auto i = 0; i < needDigRows; i++) {
				auto row = 0, num = 0;
				do {
					row = 0b1111111111;
					num = int(randSys.randGen() % 10);
					row &= ~(1 << num);
				} while (row == map[0]);
				std::memmove(map.data + 1, map.data, (map.height - 1) * sizeof(int));
				map.data[0] = row;
				if (!isInit) {
					map.colorDatas.pop_back();
					map.colorDatas.insert(map.colorDatas.begin(), std::vector<Piece>(map.width, Piece::Trash));
					map.colorDatas[0][num] = Piece::None;
				}
			}
			digRows = 10;
			map.update();
		}

		template<class T>
		void handle(T* tg, std::vector<int>& clear, std::vector<Pos>& pieceChanges) {
			auto& map = tg->map;
			if (digRowsEnd > 0) {
				for (auto& row_i : clear) {
					if (row_i < std::min(digRows, 10))
						digRows--;
				}
				if (clear.size() == 0) {
					auto needDigRows = 10 - digRows;
					digRowsEnd -= needDigRows;
					digModAdd(tg, needDigRows, false);
					digRows = 10;
					for (auto& piecePos : pieceChanges)
						piecePos.x += needDigRows;
				}
			}
		}
	};

	struct versus {
		int trashLinesCount = 0;
		std::list<int> trashLines;
		static constexpr std::array<int, 13> comboTable{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5,-1 };
		template<class T>
		void handle(T* tg, bool noClear, int& attack, std::vector<Pos>& pieceChanges) {
			auto& map = tg->map;
			auto& rS = tg->rS;
			auto addLines = [&](int rows) {
				auto row = 0b1111111111;
				auto num = 0;
				num = int(rS.randGen() % 10);
				row &= ~(1 << num);
				for (auto i = 0; i < rows; i++) {
					std::copy(map.data + 1, map.data + map.height - 1, map.data);
					map.data[map.height - 1] = row;
					map.colorDatas.erase(map.colorDatas.begin());
					map.colorDatas.push_back(std::vector<Piece>(map.width, Piece::Trash));
					map.colorDatas[map.height - 1][num] = Piece::None;
				}
			};
			auto addCount = 0;
			while (trashLinesCount != 0) {
				auto& first = trashLines.front();
				if (!noClear) {
					if (attack <= 0)
						break;
					auto offset = std::min(attack, first);
					attack -= (first -= offset, offset);
					trashLinesCount -= offset;
					if (first <= 0)
						trashLines.pop_front();
				}
				else {
					trashLines.pop_front();
					addCount += first;
					addLines(first);
				}
			}
			if (noClear && addCount > 0) {
				trashLinesCount = 0;
				map.update(false);
				for (auto& piecePos : pieceChanges)
					piecePos.x -= addCount;
			}
			attack = std::max(0, attack);
		}
	};

	struct sprint {

	};
}

class TetrisGame {
protected:
	struct gameData {
		std::size_t clear = 0;
		std::size_t b2b = 0;
		std::size_t combo = 0;
		std::size_t pieces = 0;
		bool comboState = false;
		bool isReplay = false;
		bool deaded = false;
		std::vector<Pos> pieceChanges{};
	};

	int dySpawn = -18;
	Random rS;
	Hold hS;
	gameData gd;
	using playing_mode = std::in_place_index_t<0>;
	std::variant<Modes::versus, Modes::dig, Modes::sprint> gm{ playing_mode()};
	TetrisMapEx map{ 10, 40 };
	TetrisNode tn{ TetrisNode::spawn(rS.getOne(),&map,dySpawn) };
	Recorder record{ rS.seed }, recordPath{ rS.seed };
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

public:
	TetrisGame() {
	}

	void opers(Oper a);
	int sendTrash(const std::tuple<TSpinType, int, bool, int>&);
	void hold();
	void left();
	void right();
	void softDrop();
	void hardDrop();
	void dropToBottom();
	void ccw();
	void cw();
	void restart();

	friend struct Modes::versus;
	friend struct Modes::dig;
	friend struct Modes::sprint;
};

#endif // TETRISGAME_H
