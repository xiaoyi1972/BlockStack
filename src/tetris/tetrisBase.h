#pragma once
#include<vector>
#include<iostream>
#include<iomanip>
#include<optional>
#include<algorithm>
#include<math.h>
#include<cstdint>
#include "tool.h"

enum class Piece { Trash = -2, None = -1, O, I, T, L, J, S, Z };
enum class Oper { None = -1, Left, Right, SoftDrop, HardDrop, Hold, Cw, Ccw, DropToBottom };
enum class TSpinType { None = -1, TSpinMini, TSpin };

namespace std {
	std::string to_string(const Oper&);
	std::string to_string(const Piece&);
	std::string to_string(const TSpinType&);
	std::string to_string(const std::vector<Oper>&);

	template<class T>
	auto operator<<(std::ostream& os, T const& t) -> decltype(t.print(os), os)
	{
		t.print(os);
		return os;
	}

	std::ostream& operator<<(std::ostream&, const Oper&);
	std::ostream& operator<<(std::ostream& out, const std::vector<Oper>&);
	std::ostream& operator<<(std::ostream&, const Piece&);
	std::ostream& operator<<(std::ostream&, const TSpinType&);
}

struct  Pos {
	Pos() noexcept = default;
	constexpr Pos(int _x, int _y) noexcept : x(_x), y(_y) {}
	bool operator==(const Pos& a) const {
		return x == a.x && y == a.y;
	}
	int x, y;
};

template<std::size_t...Is>
struct minoData {
	constexpr static std::size_t len = sizeof...(Is);
	static constexpr auto matrix = tuple_to_array(std::tuple_cat(bit_seq_tuple<len>(Is)...));

	template <std::size_t n = 1, class T, std::size_t N>
	static constexpr auto matrix_cw(std::size_t  l, const std::array<T, N>& seq) -> std::array<T, N>{
		if constexpr (n == 0)
			return seq;
		else {
			return matrix_cw<n - 1>(l, with_sequence<N>([&](auto...c) {
				return std::array{ seq[(static_cast<int>(c.value % l)) * l + l - 1 - static_cast<int>(c.value / l)]... }; }
			));
		}
	}

	template<std::size_t n>
	static constexpr auto point() {
		return with_sequence_impl([&](auto...Ints) { return std::array{ Pos{Ints.value % len, Ints.value / len}... }; },
			filter_integer_sequence(std::make_index_sequence<len* len>(),
				[&](auto c) {return matrix_cw<n>(len, matrix)[c]; }
		));
	};

	static constexpr auto points = with_sequence<4>([](auto...c) {
		return std::array{ point<c>()... };
		});
};

template<class TetrisMap>
struct printRoofMap {
	const TetrisMap& map;

	printRoofMap(const TetrisMap& _map) :map(_map) {}
	printRoofMap() = delete;
	void print(std::ostream& out) const {
		for (int y = map.roof - 1; y >= 0; --y) { // draw field
			out << std::setw(2) << y << " ";
			for (int x = 0; x < map.width; x++)
			{
				out << (map(x, y) ? "[]" : "  ");
			}
			out << "\n";
			if (y == 0) {
				out << std::setw(2) << "  " << " ";
				for (int x = 0; x < map.width; x++)
				{
					out << std::setw(2) << x;
				}
			}
		}
	}
};

template<class TetrisMap>
struct TetrisMapDelegate {
	TetrisMap& map;
	std::initializer_list<std::size_t> rows;

	TetrisMapDelegate(TetrisMap& _map, const std::initializer_list<std::size_t>& _rows) :
		map(_map), rows(_rows) {}

	TetrisMapDelegate& operator=(std::initializer_list<std::size_t> bits) {
		auto row_it = rows.begin();
		auto i = 0;
		for (auto& value : bits) {
			if (row_it != rows.end()) {
				i = *row_it;
				++row_it;
			}
			else ++i;
			if (!value) continue;
			auto row = *row_it;
			map.data[i] = value;
		}
		return *this;
	}
};


class TetrisMap {
public:
	int width, height, roof, count;
	int* data, * top;

	TetrisMap(int _width = 0, int _height = 0) noexcept : width(_width), height(_height), roof(0), count(0), data(nullptr), top(nullptr) {
		data = new int[height];
		top = new int[width];
		memset(data, 0, height * sizeof(int));
		memset(top, 0, width * sizeof(int));
	}

	TetrisMap(int _width, int _height, std::initializer_list<std::pair<int, int>> list) noexcept : TetrisMap(_width, _height) {
		for (const auto& [idx, v] : list) {
			this->data[idx] = v;
		}
		update();
	}

	TetrisMap(const TetrisMap& p) noexcept {
		width = p.width, height = p.height, roof = p.roof, count = p.count;
		data = new int[height], top = new int[width];
		std::copy(p.data, p.data + height, data);
		std::copy(p.top, p.top + width, top);
	}

	TetrisMap(TetrisMap&& p) noexcept {
		width = p.width, height = p.height, roof = p.roof, count = p.count;
		data = std::exchange(p.data, nullptr);
		top = std::exchange(p.top, nullptr);
	}

	TetrisMap& operator=(const TetrisMap& p) {
		if (this != &p) {
			if (width != p.width && height != p.height) {
				delete[] data, delete[] top;
				data = new int[p.height], top = new int[p.width];
			}
			width = p.width, height = p.height, roof = p.roof, count = p.count;
			std::copy(p.data, p.data + height, data);
			std::copy(p.top, p.top + width, top);
		}
		return *this;
	}

	TetrisMap& operator=(TetrisMap&& p) noexcept {
		if (this != &p) {
			width = p.width, height = p.height, roof = p.roof, count = p.count;
			delete[] data, delete[] top;
			data = std::exchange(p.data, nullptr);
			top = std::exchange(p.top, nullptr);
		}
		return *this;
	}

	~TetrisMap() {
		delete[] data, delete[] top;
		data = top = nullptr;
	}

	bool operator==(const TetrisMap& p) {
		return width == p.width && height == p.height && (!memcmp(data, p.data, height * sizeof(int)));
	}

	auto clear() {
		std::vector<int> change;
		auto full = (1 << width) - 1;
		for (auto i = 0; i < roof; ++i) {
			if (data[i] == full) {
				change.emplace_back(i);
				data[i] = 0;
			}
			else if (!change.empty()) {
				data[i - change.size()] = data[i];
				data[i] = 0;
			}
		}
		if (!change.empty()) {
			count -= change.size() * width;
			roof = 0;
			for (auto i = 0; i < width; i++) {
				top[i] = std::min<int>(top[i] - change.size(), height);
				while (!(*this)(i, top[i] - 1) && --top[i] > 0) {}
				roof = std::max(top[i], roof);
			}
		}
		return change;
	}

	int row(int x) const {
		if (x < 0 || x > height - 1) return 0;
		else return data[x];
	}

	template<bool value = true>  void set(int x, int y) {
		if constexpr (value) data[y] |= (1 << x);
		else data[y] &= ~(1 << x);
	}

	int operator()(int x, int y) const {
		if (y < 0 || y > height - 1 || x < 0 || x > width - 1) return 1;
		else return (data[y] >> x) & 1;
	}

	TetrisMapDelegate<TetrisMap> operator[](std::initializer_list<std::size_t> list) {
		return  { *this,list };
	}

	auto& operator[](std::size_t index) const{
		return  data[index];
	}

	void  update() {
		roof = count = 0;
		for (auto y = 0; y < height; y++)
			for (auto x = 0; x < width; x++) {
				if (row(y) && (*this)(x, y)) {
					roof = std::max(roof, y + 1), top[x] = std::max(top[x], y + 1);
					count++;
				}
			}
	}

	void print(std::ostream& out) const {
		for (int y = height - 1; y >= 0; --y) { // draw field
			out << std::setw(2) << y << " ";
			for (int x = 0; x < width; x++)
			{
				out << ((*this)(x, y) ? "[]" : "  ");
			}
			out << "\n";
		}
	}

	void tops() {
		std::cout << "top:";
		for (auto i = 0; i < width; i++) {
			std::cout << top[i] << "  ";
		}
		std::cout << "\n";
	}
};

class TetrisMapEx : public TetrisMap {
public:
	using colorDatasType = std::vector<std::vector<Piece>>;
	using pieceArrType = std::vector<Piece>;
	TetrisMapEx(int _width = 0, int _height = 0) noexcept : TetrisMap(_width, _height) {
		colorDatas.clear();
		colorDatas.insert(colorDatas.begin(), height, pieceArrType(_width, Piece::None));
	}

	TetrisMapEx(const TetrisMapEx& p) noexcept : TetrisMap(p) { colorDatas = p.colorDatas; }
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
		auto offset = 0;
		for (const auto& i : change) {
			colorDatas.erase(colorDatas.begin() + i - offset);
			colorDatas.emplace_back(pieceArrType(width, Piece::None));
			offset++;
		}
		return change;
	}

	void  update(bool isColor = true) {
		roof = count = 0;
		for (auto y = 0; y < height; y++)
			for (auto x = 0; x < width; x++) {
				if ((*this)(x, y)) {
					roof = std::max(roof, y + 1), top[x] = std::max(top[x], y + 1);
					count++;
					if (isColor && colorDatas[y][x] == Piece::None)
						colorDatas[y][x] = Piece::Trash;
				}
				else if (isColor) colorDatas[y][x] = Piece::None;
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

template<class TetrisNode>
class printNode {
	const TetrisMap& map;
	const TetrisNode& node;
	bool simple;
public:
	printNode(const TetrisMap& _map, const TetrisNode& _node, bool _simple)
		:map(_map), node(_node), simple(_simple) {}

	void print(std::ostream& out) const {
		auto points = *(node.data + node.rs);
		std::sort(points.begin(), points.end(), [](auto& lhs, auto& rhs) {return lhs.y == rhs.y ? lhs.x < rhs.x : lhs.y > rhs.y; });
		auto it = points.begin();
		for (int y = (!simple ? map.height - 1 : map.roof - 1); y >= 0; --y) {//draw field
			out << std::setw(2) << y << " ";
			for (int x = 0; x < map.width; x++)
			{
				if (it != points.end() && it->y + node.y == y && it->x + node.x == x) {
					out << "<>";
					++it;
				}
				else out << (map(x, y) == 1 ? "[]" : "  ");
			}
			out << "\n";
		}
	}
};


class TetrisNode {
public:
	Piece type;
	int x, y, rs;
	const std::array<Pos, 4U>* data;
	int step, moves;
	bool mini, spin, lastRotate;
	TSpinType typeTSpin;

	TetrisNode(Piece _type, int _x, int _y, int _rs) noexcept :
		type(_type), x(_x), y(_y), rs(_rs),
		data(_type == Piece::None ? nullptr : &rotateDatas[static_cast<int>(_type)][0]),
		step(0), moves(0),
		mini(false), spin(false), lastRotate(false), typeTSpin(TSpinType::None) {}

	TetrisNode() = default;

	bool operator!=(const TetrisNode& rhs) const {
		return !operator==(rhs);
	}
	bool operator==(const TetrisNode& rhs) const {
		return type == rhs.type && x == rhs.x && y == rhs.y && rs == rhs.rs;
	}
	bool operator <(const TetrisNode& rhs) const {
		return rs == rhs.rs ? x < rhs.x : rs < rhs.rs;
	}
	bool operator >(const TetrisNode& rhs) const {
		return rs == rhs.rs ? x > rhs.x : rs > rhs.rs;
	}

	void shift(int dx, int dy) {
		x += dx, y += dy;
	}

	template<bool reverse = true>
	void rotate(int index = 1) {
		if constexpr (reverse) rs -= index;
		else rs += index;
		rs &= 3;
	}

	bool check(const TetrisMap& map, int x, int  y, int rs) const {
		const auto& points = data[rs];
		for (auto& point : points) {
			if (map(point.x + x, point.y + y)) return false;
		}
		return true;
	}

	bool check(const TetrisMap& map, int x, int  y) const { return check(map, x, y, rs); }
	bool check(const TetrisMap& map) const { return check(map, x, y); }

	std::tuple<std::vector<Pos>, std::vector<int>> attachs(TetrisMapEx& map, int x, int y, int rs) {
		std::vector<Pos> change;
		const auto& points = data[rs];
		for (auto& point : points) {
			auto py = point.y + y, px = point.x + x;
			map.set<true>(px, py);
			change.push_back(Pos{ px,py });
			map.colorDatas[py][px] = type;
			map.roof = std::max(py + 1, map.roof);
			map.top[px] = std::max(py + 1, map.top[px]);
			++map.count;
		}
		return { change, map.clear() };
	}

	auto& getPoints() const {
		return  data[rs];
	}

	std::size_t attach(TetrisMap& map, int x, int y, int rs) {
		const auto& points = data[rs];
		for (auto& point : points) {
			auto px = point.x + x, py = point.y + y;
			map.set<true>(px, py);
			map.roof = std::max(py + 1, map.roof);
			map.top[px] = std::max(py + 1, map.top[px]);
			++map.count;
		}
		return map.clear().size();
	}

	std::size_t fillRows(TetrisMap& map, int x, int y, int rs) {
		const auto& points = data[rs];
		std::size_t count = 0;
		int full = (1 << map.width) - 1, row{};
		std::optional<int> pre;
		for (auto& point : points) {
			auto px = point.x + x, py = point.y + y;
			if (!pre.has_value() || *pre != py) row = map[py];
			row |= (1 << px);
			if (row == full) count++;
			pre = py;
		}
		return count;
	}

	template<class T>
	auto& getHoriZontalNodes(TetrisMap& map, T& out) {
#define GEN(_type,x,y,rs) \
		out.emplace_back(_type, x, y, rs)
		out.reserve((type == Piece::O ? 1 : 4) * map.width);
		auto node = *this;
		for (auto i = 0; node.check(map) && i++ < 4;) {
			GEN(type, node.x, node.y, node.rs);
			for (int n = 1; node.check(map, node.x - n, node.y); ++n) {
				GEN(type, node.x - n, node.y, node.rs);
			}
			for (int n = 1; node.check(map, node.x + n, node.y); ++n) {
				GEN(type, node.x + n, node.y, node.rs);
			}
			if (type == Piece::O) break;
			node.rotate();
		}
		return out;
	}

	std::tuple<std::vector<Pos>, std::vector<int>> attachs(TetrisMapEx& map) { return attachs(map, x, y, rs); }
	std::size_t attach(TetrisMap& map) { return attach(map, x, y, rs); }

	int getDrop(const TetrisMap& map) {
		const auto& points = data[rs];
		auto res = y + points.front().y - map.top[x + points.front().x];
		for (auto it = points.begin() + 1; it != points.end() && res >= 0; ++it) {
			res = std::min(res, y + it->y - map.top[x + it->x]);
		}
		if (res < 0) {
			auto i = 0;
			if (!check(map, x, y)) res = i;
			else {
				while (check(map, x, y - i)) ++i;
				res = i - 1;
			}
		}
		return res;
	}

	 auto& getkickDatas(bool dirReverse = false) {
		auto& r1 = rs;
		int i = (r1 << 1) + !dirReverse;
		if (type == Piece::I) return kickDatas[1][i];
		else return kickDatas[0][i];
	}

	 bool open(TetrisMap& map) {
		const auto& points = data[rs];
		for (auto& point : points) {
			if (point.y + y < map.top[point.x + x]) return false;
		}
		return true;
	}

	TetrisNode drop(const TetrisMap& map) const{
		auto next = *this;
		next.shift(0, -next.getDrop(map));
		return next;
	}

	static TetrisNode spawn(Piece _type, const TetrisMap* map = nullptr, int dy = 0) {
		return TetrisNode{ _type, 3, dy + (_type == Piece::I ? -1 : 0) + (map != nullptr ? map->height - 3 : 0), 0 };
	}

	static std::optional<TetrisNode> shift(TetrisNode& tn, TetrisMap& map, int x, int y) {
		if (tn.check(map, tn.x + x, tn.y + y)) {
			return TetrisNode{ tn.type,tn.x + x,tn.y + y,tn.rs };
		}
		return std::nullopt;
	}

	template<bool reverse = true>
	static std::optional<TetrisNode> rotate(TetrisNode& tn, TetrisMap& map) {
		if (tn.type == Piece::O) return std::nullopt;
		auto& kd = tn.getkickDatas(reverse);
		auto trans = tn.rs;
		if constexpr (reverse) trans--;
		else trans++;
		trans &= 3;
		if (!tn.check(map, tn.x, tn.y, trans)) {
			for (std::size_t i = 0; i < kd.size(); i += 2) {
				if (tn.check(map, tn.x + kd[i], tn.y + kd[i + 1], trans)) {
					return TetrisNode{ tn.type,tn.x + kd[i], tn.y + kd[i + 1], trans };
				}
			}
			return std::nullopt;
		}
		return TetrisNode{ tn.type, tn.x, tn.y, trans };
	}

	template<bool save = false>
	std::tuple<bool, bool> corner3(TetrisMap& map) {
		auto mini = 0, sum = 0;
		switch (rs) {
		case 0:mini = map(x + 1, y); break;
		case 1:mini = map(x, y + 1); break;
		case 2:mini = map(x + 1, y + 2); break;
		case 3:mini = map(x + 2, y + 1); break;
		}
		sum = map(x, y) + map(x, y + 2) + map(x + 2, y) + map(x + 2, y + 2);
		if constexpr (save) 
			return { this->spin = sum > 2, this->mini = mini };
		else
			return{ sum > 2, mini };
	}

	void getTSpinType() {
		if (lastRotate) {
			if (mini && spin) typeTSpin = TSpinType::TSpinMini;
			else if (spin) typeTSpin = TSpinType::TSpin;
			else typeTSpin = TSpinType::None;
		}
	}

	void print(std::ostream& out) const {
		out << "Type:" << type << " Pos:[" << x << "," << y << "] Rs:" << rs;
	}

	auto mapping(const TetrisMap& map, bool simple = false) {
		return printNode{ map,*this ,simple };
	}

	static constexpr std::array rotateDatas{
		minoData<0, 6, 6, 0>::points, // O
		minoData<0, 0,15, 0>::points, // I
		minoData<0, 7, 2>::points, // T
		minoData<0, 7, 4>::points, // L
		minoData<0, 7, 1>::points, // J
		minoData<0, 3, 6>::points, // S
		minoData<0, 6, 3>::points // Z
	};

	static constexpr std::array kickDatas{
		std::array{//kickDatas for Piece Others
			std::array{ 1, 0, 1, 1, 0, -2, 1, -2},//0->L
			std::array{-1, 0, -1, 1, 0, -2, -1, -2},//0->R
			std::array{ 1, 0, 1, -1, 0, 2, 1, 2},//R->0
			std::array{ 1, 0, 1, -1, 0, 2, 1, 2},//R->2
			std::array{-1, 0, -1, 1, 0, -2, -1, -2},//2->R
			std::array{ 1, 0, 1, 1, 0, -2, 1, -2},//2->L
			std::array{-1, 0, -1, -1, 0, 2, -1, 2},//L->2
			std::array{-1, 0, -1, -1, 0, 2, -1, 2},//L->0
		},
		std::array{//kickDatas for Piece I
			std::array{-1, 0, 2, 0, -1, 2, 2, -1},//0->L
			std::array{-2, 0, 1, 0, -2, -1, 1, 2},//0->R
			std::array{ 2, 0, -1, 0, 2, 1, -1, -2},//R->0
			std::array{-1, 0, 2, 0, -1, 2, 2, -1},//R->2
			std::array{ 1, 0, -2, 0, 1, -2, -2, 1},//2->R
			std::array{ 2, 0, -1, 0, 2, 1, -1, -2},//2->L
			std::array{-2, 0, 1, 0, -2, -1, 1, 2},//L->2
			std::array{ 1, 0, -2, 0, 1, -2, -2, 1},//L->0
		}
	};
};

struct TetrisNodeMoveEx {
	std::size_t index;
	int step;
	bool locked;
};

template<class T, bool init = true>
class filterLookUp {
private:
	template<class T1, class T2, class T3>
	constexpr auto index(T1 x, T2 y, T3 r) noexcept {
		return ((x * height << 2) + (y << 2) + r);
	}

	int width, height, count;
	trivail_vector<T> elem;
	dynamic_bitset exist;

public:
	filterLookUp(const TetrisMap& map) :
		width(map.width), height(map.height), count(map.width* map.height * 4), exist(count)
	{
		elem.reserve(count);
		elem.resize(count);
	}

	template<bool is = true>
	void set(const TetrisNode& target, const T& value) {
		const auto& point_front = target.getPoints()[0];
		if constexpr (is) {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs);
			elem[n] = value;
			exist.set_value<true>(n);
		}
		else {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs & 1);
			elem[n] = value;
			exist.set_value<true>(n);
		}
	}

	template<bool is = true>
	void clear(const TetrisNode& target) {
		const auto& point_front = target.getPoints()[0];
		if constexpr (is) {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs);
			exist.set_value<false>(n);
		}
		else {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs & 1);
			exist.set_value<false>(n);
		}
	}

	template<bool is = true>
	bool contain(const TetrisNode& target) {
		const auto& point_front = target.getPoints()[0];
		if constexpr (is) {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs);
			return exist.has_value(n);
		}
		else {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs & 1);
			return exist.has_value(n);
		}
	}

	template<bool is = true>
	T* get(const TetrisNode& target) {
		const auto& point_front = target.getPoints()[0];
		if constexpr (is) {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs);
			return exist.has_value(n) ? std::addressof(elem[n]) : nullptr;
		}
		else {
			auto n = index(point_front.x + target.x, point_front.y + target.y, target.rs & 1);
			return exist.has_value(n) ? std::addressof(elem[n]) : nullptr;
		}
	}
};

template<class T>
class filterLookUp<T, false> {
public:
	filterLookUp(const TetrisMap& map) {}
};
