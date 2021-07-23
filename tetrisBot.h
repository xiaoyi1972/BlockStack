#ifndef TETRISBOT_H
#define TETRISBOT_H
#include"tetrisBase.h"
#include<unordered_set>
#include<array>
#include<queue>
#include<climits>
#include<algorithm>
#include<functional>
#include<execution>
#include<mutex>
#include<memory>
#include<iostream>
#include<cassert>
#include<list>

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

namespace Tool {
std::string printType(Piece);
void printMap(TetrisMap &);
std::string printNode(TetrisNode &);
std::string printPath(std::vector<Oper> &);
std::string printOper(Oper &);
void sleepTo(int msec);
}

namespace TetrisBot {
	struct EvalResult {
		int safe = 0;
		double t2Value = 0.;
		double t3Value = 0.;
		double value = 0.;
		int clear = 0;
		int count = 0;
		Piece type = Piece::None;
		TSpinType typeTSpin = TSpinType::None;
		int mapWidth, mapHeight;
	};

	struct Status {
		EvalResult eval;
		int combo = 0;
		bool b2b = false;
		bool deaded = false;
		int like = 0;
		int maxAttack = 0;
		int maxCombo = 0;
		double value = 0.;
		int mapRise = 0;
		int underAttack = 0;
		int attack = 0;
		bool cutB2b = false;
		double t2Value = 0.;
		double t3Value = 0.;
		bool operator<(const Status& other) const {
			return this->value < other.value;
		}
};

template<bool res = true>
std::vector<TetrisNode> search(TetrisNode&, TetrisMap&);
auto make_path(TetrisNode&, TetrisNode&, TetrisMap&, bool NoSoftToBottom = false)->std::vector<Oper>;
TetrisBot::EvalResult evalute(TetrisNode &, TetrisMap &, int, int);
TetrisBot::Status get(const TetrisBot::EvalResult &, TetrisBot::Status &, Piece hold, std::vector<Piece>* nexts);
}

class TreeNode;
class TreeNodeCompare;
class TreeContext {
public:
    TreeContext():root(nullptr,TreeContext::treeDelete) { };
    ~TreeContext();
    using Result = std::tuple<TetrisNode, bool>;
    void createRoot(TetrisNode &, TetrisMap &, std::vector<Piece> &, Piece, int, int, int);
    static void treeDelete(TreeNode *);
    void update(TreeNode* );
    void run();
    Result empty();
    Result getBest();
    auto getNode(Piece type) {
        return &nodes[static_cast<int>(type)];
    }
    static std::vector<TetrisNode> nodes;
    constexpr static std::array<int,13> comboTable{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, -1 };
    constexpr static int ComboAttackIndex= []() constexpr{
        for (int i = 0; i < comboTable.size(); i++) if (comboTable[i] > 0) return i;
        return -1;
    }();
    int count = 0;
private:
    friend class TreeNode;
    using treeQueue = std::priority_queue<TreeNode *, std::vector<TreeNode *>, TreeNodeCompare>;
    std::vector<treeQueue>level; //保存每一个等级层
    std::vector<treeQueue>extendedLevel; //保存每一个等级层
    std::vector<Piece>nexts, noneHoldFirstNexts; //预览队列
    int width = 0;
    std::vector<double> width_cache;
    bool isOpenHold = 
   // false; 
    true;
    std::unique_ptr<TreeNode, decltype(TreeContext::treeDelete)*> root;
    double  div_ratio;
    int tCount = 0;
};

class TreeNodeCompare {
public:
    bool operator()(TreeNode *const &a, TreeNode *const &b) const;
};

class TreeNode {
public:
    struct EvalParm {
        TetrisNode land_node;
        int clear = 0;
        TetrisBot::Status status;
        int needSd = 0;
    };
    TreeNode() {}
    TreeNode(TreeContext *, TreeNode *, Piece , TetrisMap, int, Piece, bool,  EvalParm &);
    bool operator==(TreeNode& p) {
        return
            /*!(std::memcmp(nexts->data(), p.nexts->data(), std::min(nexts->size(), p.nexts->size()))) &&*/
            cur == p.cur &&
            map == p.map &&
            hold == p.hold; /* &&
            evalParm.status.underAttack == p.evalParm.status.underAttack &&
            evalParm.status.b2b == p.evalParm.status.b2b &&
            evalParm.status.combo == p.evalParm.status.combo &&
            evalParm.status.maxCombo == p.evalParm.status.maxCombo;*/
    }
    void printInfoReverse(TetrisMap &, TreeNode *);
    TreeNode *generateChildNode(TetrisNode &, bool, Piece, bool);
    void search();
    void search_hold(bool op = false, bool noneFirstHold = false);
    bool eval();
    void run();
    TreeContext::Result getBest();
    friend class TreeContext;
    friend class TreeNodeCompare ;
private:
    EvalParm evalParm; //落点信息
    TreeNode *parent = nullptr;  //父节点
    TreeContext *context = nullptr; //公用的层次对象
    std::vector<Piece> *nexts = nullptr; //预览队列
    int nextIndex = 0; //第几个Next
    std::vector<TreeNode *>children; //子节点
    TetrisMap map; //当前场地
    bool extended = false, isHold = false, isHoldLock = false;
    Piece cur = Piece::None,hold = Piece::None;//当前块 暂存块
};

#endif // TETRISBOT_H
