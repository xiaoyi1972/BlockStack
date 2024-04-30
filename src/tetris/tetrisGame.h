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
#include<chrono>
#include<variant>
#include<array>
#include<list>
#include<sstream>
#include<mutex>
#include<random>
#include"tetrisBase.h"

#include<syncstream>

//#define CLASSIC

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; }; // helper type for the visitor #4
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>; // explicit deduction guide (not needed as of C++20)

namespace My {
	static constexpr bool  test = false ^ 0;
}

class Random {
public:
	int seed;
	std::vector<Piece> bag, displayBag;
	std::default_random_engine randGen;

	Random(unsigned _seed = std::chrono::system_clock::now().time_since_epoch().count()) :seed(_seed) {
		//std::cout << "_seed:" << _seed << "\n";
		//seed = 3366990943;
		randGen.seed(seed);
	}

	void getBag();
	Piece getOne();
};


class Hold {
public:
	bool able;
	Piece type;

	Hold() :able(true), type(Piece::None) {
		if constexpr (My::test) {
			//type = Piece::L;
		}
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
		int digRows = digRowUpper;
		int digRowUpper = 18;
		int digRowsEnd = 10000;

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
			digRows = digRowUpper;
			map.update();
		}

		template<class T>
		void handle(T* tg, std::vector<int>& clear, std::vector<Pos>& pieceChanges) {
			auto& map = tg->map;
			if (digRowsEnd > 0) {
				for (auto& row_i : clear) {
					if (row_i < std::min(digRows, digRowUpper))
						digRows--;
				}
				if (clear.size() == 0) {
					auto roof = *std::max_element(map.top + 3, map.top + 7);
					auto needDigRows = std::max(0, std::min(20 - roof - 2, digRowUpper - digRows));
					digRowsEnd -= needDigRows;
					digModAdd(tg, needDigRows, false);
					digRows = digRowUpper;
					for (auto& piecePos : pieceChanges)
						piecePos.y += needDigRows;
				}
			}
		}
	};

	struct versus {
		int trashLinesCount = 0;
		std::list<int> trashLines;
		static constexpr std::array comboTable{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5,-1 };

		template<class T>
		void handle(T* tg, bool noClear, int& attack, std::vector<Pos>& pieceChanges) {
			auto& map = tg->map;
			auto& rS = tg->rS;
			auto& opponent = tg->opponent;
			auto addLines = [&](int rows) {
				auto row = 0b1111111111;
				auto num = 0;
				num = int(rS.randGen() % 10);
				row &= ~(1 << num);
				for (auto i = 0; i < rows; i++) {
					std::memmove(map.data + 1, map.data, (map.height - 1) * sizeof(int));
					map.data[0] = row;
					map.colorDatas.pop_back();
					map.colorDatas.insert(map.colorDatas.begin(), std::vector<Piece>(map.width, Piece::Trash));
					map.colorDatas[0][num] = Piece::None;
				}
			};
			auto addCount = 0;
			{
				std::lock_guard guard(tg->mtx);
				while (trashLinesCount != 0) {
					auto& first = trashLines.front();
					if (!noClear) {
						if (attack <= 0) break;
						auto offset = std::min(attack, first);
						attack -= (first -= offset, offset);
						trashLinesCount = std::max(trashLinesCount - offset, 0);
						if (first <= 0)
							trashLines.pop_front();
					}
					else {
						trashLinesCount = std::max(trashLinesCount - first, 0);
						addCount += first;
						addLines(first);
						trashLines.pop_front();
					}
				}
				if (noClear && addCount > 0) {
					trashLinesCount = 0;
					map.update(false);
					for (auto& piecePos : pieceChanges)
						piecePos.y += addCount;
				}
			}
			attack = std::max(0, attack);
			if (opponent != nullptr && attack != 0) {
				std::lock_guard guard(opponent->mtx);
				std::get<versus>(opponent->gm).addTrash(attack);
			}
		}

		void addTrash(int attack) {
			trashLines.push_back(attack);
			trashLinesCount += attack;
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
		std::size_t attack = 0;
		bool comboState = false;
		bool isReplay = false;
		bool deaded = false;
		std::string clearTextInfo{};
		std::vector<Pos> pieceChanges{};
	};

	std::mutex mtx;
	int dySpawn = -18;
	std::size_t mode = 0;
	TetrisGame* opponent = nullptr;
	Random rS{};
	Hold hS;
	gameData gd;
	std::variant<Modes::versus, Modes::dig, Modes::sprint> gm;
	TetrisMapEx map{ 10, 40 };
	TetrisNode tn{ TetrisNode::spawn(rS.getOne(), &map, dySpawn) };
	Recorder record{ rS.seed }, recordPath{ rS.seed };
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

public:
	TetrisGame() {
		restart();
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
	void switchMode(std::size_t , bool = false);
	TetrisNode gen_piece(Piece);

	friend struct Modes::versus;
	friend struct Modes::dig;
	friend struct Modes::sprint;
};

#endif