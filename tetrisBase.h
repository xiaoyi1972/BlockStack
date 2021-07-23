#ifndef TETRISBASE_H
#define TETRISBASE_H
#include<chrono>
#include<map>
#include<functional>
#include<cmath>
#include<vector>
#include<random>
#include<sstream>
#include<string>
//#include<iostream>
//#include<cassert>
enum class Piece { Trash = -2, None = -1, O, I, T, L, J, S, Z };
enum class Oper { None = -1, Left, Right, SoftDrop, HardDrop, Hold, Cw, Ccw, DropToBottom };
enum class TSpinType { None = -1, TSpinMini, TSpin };

class  Pos {
public:
    Pos() = default;
    Pos(int _x, int _y) : x(_x), y(_y) {}
    bool operator==(const Pos& a) const {
        return x == a.x && y == a.y;
    }
    int x, y;
};

class TetrisMap {
public:
    TetrisMap(int _width = 0, int _height = 0) : width(_width), height(_height), roof(_height), count(0) {
        data = new int[height];
        std::memset(data, 0, height * sizeof(int));
        std::memset(top, _height, 32 * sizeof(char));
    }

    TetrisMap(const TetrisMap& p) {
        width = p.width;
        height = p.height;
        roof = p.roof;
        count = p.count;
        data = new int[height];
        std::copy(p.data, p.data + height, data);
        std::copy(std::begin(p.top), std::end(p.top), std::begin(top));
    }

    TetrisMap(TetrisMap&& p) noexcept {
        width = p.width;
        height = p.height;
        roof = p.roof;
        count = p.count;
        data = std::exchange(p.data, nullptr);
        std::copy(std::begin(p.top), std::end(p.top), std::begin(top));
    }

    TetrisMap& operator=(const TetrisMap& p) {
        if (this != &p) {
            width = p.width;
            height = p.height;
            roof = p.roof;
            count = p.count;
            delete[] data;
            data = new int[height];
            std::copy(p.data, p.data + height, data);
            std::copy(std::begin(p.top), std::end(p.top), std::begin(top));
        }
        return *this;
    }

    TetrisMap& operator=(TetrisMap&& p) noexcept {
        if (this != &p) {
            width = p.width;
            height = p.height;
            roof = p.roof;
            count = p.count;
            delete[] data;
            data = std::exchange(p.data, nullptr);
            std::copy(std::begin(p.top), std::end(p.top), std::begin(top));
        }
        return *this;
    }

    ~TetrisMap() {
        delete[] data;
        data = nullptr;
    }

    auto clear() {
        std::vector<int> change;
        auto full = (1 << width) - 1;
        for (auto i = height - 1; i >= roof; i--) {
            if (data[i] == full) {
                change.push_back(i);
                data[i] = 0;
            }
            else if (!change.empty()) {
                data[i + change.size()] = data[i];
                data[i] = 0;
            }
        }
        if (!change.empty()) {
            count -= change.size() * width;
            roof += change.size();
            for (auto i = 0; i < width; i++) {
                top[i] = std::min<int>(top[i] + change.size(), height);
				while (!(*this)(top[i], i) && top[i] < height) ++top[i];
            }
        }
        return change;
    }

    int row(int x) const {
        if (x < 0 || x > height - 1) return 0;
        else return data[x];
    }

    inline void set(int row, int col, bool value) {
        if (value) data[row] |= (1 << col);
        else data[row] &= !(1 << col);
    }

    inline int operator()(int x, int y) const {
        if (x < 0 || x > height - 1 || y < 0 || y > width - 1) return 1;
        else return (data[x] >> y) & 1;
    }

    inline bool operator==(const TetrisMap& p) { 
        return width == p.width && height == p.height && (!std::memcmp(data, p.data, height * sizeof(int))); 
    }

    int width, height;//宽 高
    int roof, count; //最高位置 块数
	int* data{ nullptr }; //数据
    char top[32]{}; //每列高度
};

class TetrisMapEx : public TetrisMap {
public:
    using colorDatasType = std::vector<std::vector<Piece>>;
    using pieceArrType = std::vector<Piece>;
    TetrisMapEx(int _width = 0, int _height = 0) : TetrisMap(_width, _height) {
        colorDatas.clear();
        colorDatas.insert(colorDatas.begin(), height, pieceArrType(_width, Piece::None));
    }

    TetrisMapEx(const TetrisMapEx& p) : TetrisMap(p) { colorDatas = p.colorDatas; }
    TetrisMapEx(TetrisMapEx&& p) noexcept :TetrisMap(std::move(p)) { colorDatas = std::move(p.colorDatas); }

    TetrisMapEx& operator=(const TetrisMapEx& p) noexcept {
        if (this != &p) {
            TetrisMap::operator=(p);
            colorDatas = p.colorDatas;
        }
        return *this;
    }

    TetrisMapEx& operator=(TetrisMapEx&& p) noexcept {
        if (this != &p) {
            TetrisMap::operator=(p);
            colorDatas = std::move(p.colorDatas);
        }
        return *this;
    }

    auto clear() {
        std::vector<int> change = TetrisMap::clear();
        for (const auto& i : change) {
            colorDatas.erase(colorDatas.begin() + i);
            colorDatas.insert(colorDatas.begin(), pieceArrType(width, Piece::None));
        }
        return change;
    }

    void  update(bool isColor = true) {
        roof = height;
        count = 0;
        for (auto i = 0; i < height; i++)
            for (auto j = 0; j < width; j++) {
                if ((*this)(i, j)) {
                    roof = std::min(roof, i);
                    top[j] = std::min<char>(top[j], i);
                    count++;
                    if (isColor)
                        if (colorDatas[i][j] == Piece::None)
                            colorDatas[i][j] = Piece::Trash;
                }
                else {
                    if (isColor)
                        colorDatas[i][j] = Piece::None;
                }
            }
    }
    colorDatasType colorDatas;
};

namespace customHash {
    inline void hashCombine(std::size_t&) { }
    template <typename T, typename... Rest>
    inline void hashCombine(std::size_t& seed, const T& v, Rest... rest) {
        seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        hashCombine(seed, rest...);
    }
}


class TetrisNode {
    using data = std::vector<int>;
    using datas = std::vector<data>;
    using dataPos = std::vector<Pos>;
    using datasPos = std::vector<dataPos>;
    struct NodeData {
        datas minos;
        datasPos points;
    };
public:
    TetrisNode(Piece _type = Piece::None, const Pos& _pos = Pos{ 3, 0 }, int _rotateState = 0)
		:pos(_pos), type(_type), rotateState(_rotateState) {}

	inline auto area() {
        const auto& data = rotateDatas[type].minos[rotateState];
        return std::make_pair(data.size(), data.size());
    }

    inline int operator()(int x, int y) const {
        const auto& data = rotateDatas[type].minos[rotateState];
        if (x < 0 || x > data.size() - 1 || y < 0 || y > data.size() - 1) return 0;
        else return (data[x] >> y) & 1;
    }

    bool operator!=(const TetrisNode& p) const { return !operator==(p); }
    bool operator==(const TetrisNode& p) const { return pos == p.pos && rotateState == p.rotateState && type == p.type; }
    bool operator <(const TetrisNode& p) const { 
        return rotateState == p.rotateState ? pos.x < p.pos.x : rotateState < p.rotateState; 
    }
    bool operator >(const TetrisNode& p) const { 
        return rotateState == p.rotateState ? pos.x > p.pos.x : rotateState > p.rotateState; 
    }

	void shift(int _x, int _y) { pos.x += _x; pos.y += _y; }

    void rotate(int _reverse = 0, int _index = 1) {
        rotateState = !_reverse ? rotateState + _index : rotateState - _index;
        rotateState = (rotateState < 0 ? rotateState + 4 : rotateState) % 4;
    }

    bool check(TetrisMap& map, int _x, int  _y) const {
        auto real = true;
        const auto& points = rotateDatas[type].points[rotateState];
        for (auto& point : points) {
            if (map(point.y + _y, point.x + _x)) {
                real = false;
                break;
            }
        }
        return real;
    }

    bool check(TetrisMap& map) const { return check(map, pos.x, pos.y); }

    std::tuple<std::vector<Pos>, std::vector<int>> attachs(TetrisMapEx& map, const Pos& _pos, char _rotateState) {
        std::vector<Pos> change;
        const auto& points = rotateDatas[type].points[_rotateState];
        for (auto& point : points) {
            auto py = point.y + _pos.y, px = point.x + _pos.x;
            map.set(py, px, true);
			change.push_back(Pos{ py,px });
            map.colorDatas[py][px] = type;
            map.roof = std::min(py, map.roof);
            map.top[px] = std::min<char>(py, map.top[px]);
            ++map.count;
        }
        return { change, map.clear() };
    }

    auto &getPoints() {
        return rotateDatas[type].points[rotateState];
    }

    std::size_t attach(TetrisMap& map, const Pos& _pos, char _rotateState) {
         const auto& points = rotateDatas[type].points[_rotateState];
         for (auto& point : points) {
			 auto py = point.y + _pos.y, px = point.x + _pos.x;
			 map.set(py, px, true);
			 map.roof = std::min(py, map.roof);
			 map.top[px] = std::min<char>(py, map.top[px]);
			 ++map.count;
         }
        auto clears = map.clear();
        auto clearsNum = clears.size();
        return clearsNum;
    }

	std::vector<TetrisNode> getHoriZontalNodes(TetrisMap& map) {
        std::vector<TetrisNode> nodes{};
		nodes.reserve((type == Piece::O ? 1 : 4) * map.width);
        auto node = *this;
        while (node.check(map)) {
            nodes.emplace_back(type, Pos{ node.pos.x,node.pos.y }, node.rotateState);
            for (int n = 1; node.check(map, node.pos.x - n, node.pos.y); ++n) 
		    nodes.emplace_back(type, Pos{ node.pos.x - n,node.pos.y }, node.rotateState);
            for (int n = 1; node.check(map, node.pos.x + n, node.pos.y); ++n) 
		    nodes.emplace_back(type, Pos{ node.pos.x + n,node.pos.y }, node.rotateState);
            if (type != Piece::O) node.rotate(0);
            if (node == *this) break;
        }
		return nodes;
    }

    std::tuple<std::vector<Pos>, std::vector<int>> attachs(TetrisMapEx& map) { return attachs(map, pos, rotateState); }
    std::size_t attach(TetrisMap& map) { return attach(map, pos, rotateState); }

    std::size_t fillRows(TetrisMap& map, const Pos& _pos, char _rotateState) {
        const auto& data = rotateDatas[type].minos[_rotateState];
        const auto full = (1 << map.width) - 1;
        auto size = data.size();
        size_t count = 0;
        for (auto i = 0; i < size; i++) {
            auto dataI = _pos.x > 0 ? data[i] << _pos.x : data[i] >> std::abs(_pos.x);
            if (map.data[i + _pos.y] & dataI) return 0;
            auto dataBits = (map.data[i + _pos.y] | dataI);
			if (dataI && dataBits == full) ++count;
        }
        return count;
    }

    int getDrop(TetrisMap& map) {
        auto origin = [&]() {
            auto i = 0;
            if (!check(map, pos.x, pos.y)) return i;
			else while (check(map, pos.x, pos.y + i)) ++i;
            return i - 1;
        };
#define topDis(key) (map.top[key.x + pos.x] - (key.y + pos.y))
        const auto& points = rotateDatas[type].points[rotateState];
        auto res = topDis(points.front());
        for (auto& point : points) res = std::min(res, topDis(point));
        --res;
        if (res < 0) res = origin();
        return res;
#undef topDis
    }

    auto& getkickDatas(bool dirReverse = false) {
        auto &r1 = rotateState;
        auto r2 = r1;
        r2 = !dirReverse ? ++r2 : --r2;
        r2 = (r2 < 0 ? r2 + 4 : r2) % 4;
        int i = -1;
        switch (r1) {
        case 0: i = r2 == 1 ? 0 : 7; break;
        case 1: i = r2 == 0 ? 1 : 2; break;
        case 2: i = r2 == 1 ? 3 : 4; break;
        case 3: i = r2 == 2 ? 5 : 6; break;
        default: i = -1; break;
        }
        if (type != Piece::I) return kickDatas[true][i];
        else return kickDatas[false][i];
    }

    bool open(TetrisMap& _map) {
        const auto& points = rotateDatas[type].points[rotateState];
        for (auto& point : points) {
            if (point.y + pos.y > _map.top[point.x + pos.x]) return false;
        }
        return true;
    }

    TetrisNode drop(TetrisMap& _map) {
        auto next = *this;
        next.shift(0, next.getDrop(_map));
        return next;
    }

    static TetrisNode spawn(Piece _type) { return TetrisNode{ _type, Pos{3, _type == Piece::O ? -1 : 0} }; }
    static bool shift(TetrisNode& _tn, TetrisMap& _map, int _x, int _y) {
        _tn.shift(_x, _y);
        if (!_tn.check(_map)) {
            _tn.shift(-_x, -_y);
            return false;
        }
        else return true;
    }

    static bool rotate(TetrisNode& _tn, TetrisMap& _map, bool _reverse = true, bool isKickOpen = false) {
        if (_tn.type == Piece::O) return false;
        auto& kd = _tn.getkickDatas(_reverse);
        _tn.rotate(_reverse ? 1 : 0);
        auto prePos = _tn.pos;
        if (!_tn.check(_map)) {
            auto offsetRes = false;
            for (auto i = 0; i < kd.size() / 2; i++) {
                _tn.pos.x = prePos.x + kd[i * 2];
                _tn.pos.y = prePos.y - kd[i * 2 + 1];
                if (_tn.check(_map)) {
                    offsetRes = !isKickOpen ? true : _tn.open(_map);
                    if (offsetRes) break;
                }
            }
            if (!offsetRes) {
                _tn.pos = prePos;
                _tn.rotate(_reverse ? 0 : 1);
                return false;
            }
        }
        return true;
    }

    std::tuple<bool, bool> corner3(TetrisMap& map) const{
        auto mini = 0, sum = 0;
        auto& _x = pos.x;
        auto& _y = pos.y;
#define occupied(x,y) int(!(*this)(y, x) && map(_y + y, _x + x))
        mini = occupied(0, 1) + occupied(1, 0) + occupied(2, 1) + occupied(1, 2);
#undef occupied
        sum = map(_y, _x) + map(_y + 2, _x) + map(_y, _x + 2) + map(_y + 2, _x + 2);
        return{ sum > 2, mini == 1 };
    }

    auto cellsEqual() {
        size_t seed = 0;
        const auto& points = rotateDatas[type].points[rotateState];
        for (auto& point : points) customHash::hashCombine(seed, point.y + pos.y, point.x + pos.x);
        return seed;
    }

    void getTSpinType() {
        if (lastRotate) {
            if (mini && spin)
                typeTSpin = TSpinType::TSpinMini;
            else if (spin)
                typeTSpin = TSpinType::TSpin;
            else
                typeTSpin = TSpinType::None;
		}
    }

    Pos pos;
    Piece type;
    char rotateState;
    mutable bool mini = false, spin = false, lastRotate = false;
    mutable TSpinType typeTSpin = TSpinType::None;
    static std::map<Piece, NodeData> rotateDatas;
    static std::map<bool, std::vector<data>> kickDatas;
};


struct searchNode {
    TetrisNode node;
    int count;
    searchNode(TetrisNode& _node, int _count) : node{ _node }, count(_count) {}
    searchNode(TetrisNode&& _node, int _count) : node{ _node }, count(_count) {}
    bool operator==(const searchNode& p) const {
        return const_cast<TetrisNode&>(node).cellsEqual() == const_cast<TetrisNode&>(p.node).cellsEqual();
    }
};

namespace std {
    template<>
    struct hash<TetrisNode> {
    public:
        size_t operator()(const TetrisNode& key) const {
            size_t seed = 0;
            customHash::hashCombine(seed, key.pos.x, key.pos.y, key.rotateState, static_cast<int>(key.type));
            return seed;
        }
    };

    template<>
    struct hash<searchNode> {
    public:
        size_t operator()(const searchNode& key) const {
            return const_cast<TetrisNode&>(key.node).cellsEqual();
        }
    };
}

inline std::ostream& operator<<(std::ostream& out, const TetrisMap& p) {
    for (int i = 0; i < p.height; i++) {//draw field
        for (int j = 0; j < p.width; j++)
        {
            out << (p(i, j) ? "[]" : "  ");
        }
        out << "\n";
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const Piece& p) {
    switch (p) {
    case Piece::None:out << " "; break;
    case Piece::O:out << "O"; break;
    case Piece::T:out << "T"; break;
    case Piece::I:out << "I"; break;
    case Piece::S:out << "S"; break;
    case Piece::Z:out << "Z"; break;
    case Piece::L:out << "L"; break;
    case Piece::J:out << "J"; break;
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const TetrisNode& p) {
	out << "Type:" << p.type << " Pos:[" << p.pos.x << "," << p.pos.y << "] Rs:" << int(p.rotateState);
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const searchNode& p) {
	out << p.node << "(" << p.count << ")";
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const TSpinType& p) {
    switch (p) {
    case TSpinType::None:out << "None"; break;
    case TSpinType::TSpinMini:out << "TSpinMini"; break;
	case TSpinType::TSpin:out << "TSpin"; break;
    }
    return out;
}

class Random {
public:
    Random(unsigned _seed = std::chrono::system_clock::now().time_since_epoch().count()) {
        seed = _seed;
        randGen.seed(seed);
    }

    void getBag() {
		if (constexpr bool  test = false; test) {
         //   bag.fill(Piece::T, 7);
            bag = std::vector<Piece>{ 7 , Piece::L };
            return;
        }
        std::vector<Piece> bagSets{ Piece::O, Piece::I, Piece::T, Piece::L, Piece::J, Piece::S, Piece::Z };
        while (bag.size() < 7) {
            auto it = std::next(bagSets.begin(), randGen() % bagSets.size());
            auto piece = *it;
            bagSets.erase(it);
            bag.push_back(piece);
        }
    }

    Piece getOne() {
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

    std::vector<Piece> bag, displayBag;
    int seed;
    std::default_random_engine randGen;
};

class Hold {
public:
    Hold() {
        able = true;
        type = Piece::None;
    }

    void reset() { able = true; }

    int exchange(TetrisNode& _cur, Random& rS) {
        auto _curType = _cur.type;
        auto res = 0;
        if (able) {
            if (type == Piece::None) {
                type = _curType;
                _cur = TetrisNode::spawn(rS.getOne());
                res = 2;
            }
            else {
                std::swap(type, _curType);
                _cur = TetrisNode::spawn(_curType);
                res = 1;
            }
            able = !able;
        }
        return res;
    }
    bool able = true;
    Piece type = Piece::None;
};

class Recorder {
public:
    Recorder(int _seed = -1) { seed = _seed; }

    int timeRecord() {
        using std::chrono::high_resolution_clock;
        using std::chrono::milliseconds;
        static auto startTime = high_resolution_clock::now();
        auto now = high_resolution_clock::now();
        milliseconds timeDelta = std::chrono::duration_cast<milliseconds>(now - startTime);
        startTime = now;
        return timeDelta.count();
    }

    void clear() {
        time.clear();
        oper.clear();
        playIndex = 0;
    }

    friend std::stringstream& operator<<(std::stringstream& out, const Recorder& R) {
        out << R.seed << R.playIndex << R.timeAll << R.gameType << R.firstTime;
        out << R.time.size() << R.oper.size();
        out.write(reinterpret_cast<char const*>(R.time.data()), R.time.size() * sizeof(decltype(R.time)::value_type));
        out.write(reinterpret_cast<char const*>(R.oper.data()), R.oper.size() * sizeof(decltype(R.oper)::value_type));
        return out;
    }

    friend std::stringstream& operator>>(std::stringstream& in, Recorder& R) {
        auto timeLen = 0, operLen = 0;
        in >> R.seed >> R.playIndex >> R.timeAll >> R.gameType >> R.firstTime;
        in >> timeLen >> operLen;
        R.time.resize(timeLen);
        R.oper.resize(operLen);
        in.read(reinterpret_cast<char*>(R.time.data()), R.time.size() * sizeof(decltype(R.time)::value_type));
        in.read(reinterpret_cast<char*>(R.oper.data()), R.oper.size() * sizeof(decltype(R.oper)::value_type));
        return in;
    }

    static Recorder readStruct(const std::string& ba) {
        Recorder obj;
        std::stringstream in;
        in.str(ba);
        in >> obj;
        return obj;
    }

    static std::string writeStruct(Recorder& obj) {
        std::stringstream out;
        out << obj;
        std::string ba{ out.str() };
        return ba;
    }

    void add(Oper _operation, int _time) {
        auto len = time.size();
        if (len == 0)
            firstTime = _time;
        oper.push_back(_operation);
        time.push_back(_time);
    }

    std::tuple<int, Oper>play() {
        auto playIndex_ = playIndex++;
        return{ playIndex >= time.size() ? 0 : time[playIndex], playIndex_ >= oper.size() ? Oper::None : oper[playIndex_] };
    }

    void reset() { playIndex = 0; }
    auto isEnd() { return playIndex < time.size() ? false : true; }

    int seed, firstTime = 0;
    int playIndex = 0, timeAll = 0, gameType = 1;
    std::vector<int>time;
    std::vector<Oper>oper;
};

#endif // TETRISBASE_H
