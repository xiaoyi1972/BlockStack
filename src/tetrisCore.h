#pragma once
#include "tetrisBase.h"
#include<functional>
#include<execution>
#include<mutex>
#include<memory>
struct Search {
	static std::vector<TetrisNode> search(TetrisNode& , TetrisMap& );
	static std::vector<Oper> make_path(TetrisNode&, TetrisNode&, TetrisMap&, bool NoSoftToBottom = false);
};

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

class TreeContext;
struct Evaluator {
	int col_mask_{}, row_mask_{}, full_count_{}, width{}, height{};
	std::array<int, 12> table{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5 };
    TreeContext* context = nullptr;

    struct Result {
        double value;
        int clear;
        int count;
        double t2_value;
        double t3_value;
        TetrisMap* map;
        TSpinType typeTSpin;
    };

    struct Status {
        int max_combo;
        int max_attack;
        int death;
        int combo;
        int attack;
        int under_attack;
        int map_rise;
        bool b2b;
        double like;
        double value;
        bool deaded;
        bool operator<(const Status& other) const {
            return this->value < other.value;
        }
    };

	Evaluator(TreeContext* _ctx):context(_ctx) {

	}

    void init(const TetrisMap* map) {
        auto full = ((1 << map->width) - 1);
        col_mask_ = full & ~1;
        row_mask_ = full;
        full_count_ = map->width * 24;
        width = map->width;
        height = map->height;
    }

	Result evalute(TetrisNode& lp, TetrisMap& map, int clear, int tCount);
	int getSafe(const TetrisMap& map);
	Status get(const Result& eval_result, Status& status, std::optional<Piece> hold, std::vector<Piece>* nexts, int depth);
};


class TreeNode;
class TreeNodeCompare;
class TreeContext {
public:
    friend class TreeNode;
    using Result = std::tuple<TetrisNode, bool>;
    using treeQueue = std::priority_queue<TreeNode*, std::vector<TreeNode*>, TreeNodeCompare>;
    int count = 0;
	Evaluator evalutator{ this };
    static void treeDelete(TreeNode*);
private:
    std::vector<treeQueue>level; 
    std::vector<treeQueue>extendedLevel;
    std::vector<Piece>nexts, noneHoldFirstNexts;
    std::vector<double> width_cache;
    bool isOpenHold = true;
    double  div_ratio{};
    int width{}, tCount = 0;
    std::vector<TetrisNode> nodes;
    std::unique_ptr<TreeNode, decltype(TreeContext::treeDelete)*> root;
public:
    TreeContext() :root(nullptr, TreeContext::treeDelete) {
    };
    ~TreeContext() { 
        root.reset(nullptr); 
        return; 
    }
    void createRoot(const TetrisNode&, const TetrisMap&, std::vector<Piece>&, std::optional<Piece>, int, int, int,int );
    void update(TreeNode*);
    void run();
    Result empty();
    Result getBest();
    auto& getNode(Piece type) {
        return nodes[static_cast<int>(type)];
    }

};

struct TreeNodeCompare {
    bool operator()(TreeNode* const& , TreeNode* const& ) const;
};

class TreeNode {
    friend class TreeContext;
    friend struct TreeNodeCompare;
public:
    struct EvalParm {
        TetrisNode land_node;
        int clear = 0;
        Evaluator::Result eval_result;
        Evaluator::Status status;
        int needSd = 0;
    };
private:
    EvalParm evalParm;
    TreeNode* parent{ nullptr };
    TreeContext* context{ nullptr };
	std::vector<Piece>* nexts{ nullptr };
    int nextIndex = 0;
    std::vector<TreeNode*>children;
    TetrisMap map;
	bool extended{ false }, isHold{ false }, isHoldLock{ false };
    std::optional<Piece> cur{ std::nullopt };
	std::optional<Piece> hold{ std::nullopt };
public:
    TreeNode() {}
    TreeNode(TreeContext* _ctx, TreeNode* _parent, std::optional<Piece> _cur, TetrisMap _map,
        int _nextIndex, std::optional<Piece> _hold, bool _isHold, const EvalParm& _evalParm) :
        parent(_parent), context(_ctx), nexts(_parent->nexts), nextIndex(_nextIndex),
        map(std::move(_map)), isHold(_isHold), cur(_cur), hold(_hold), evalParm(_evalParm) {

    }
    bool operator==(TreeNode& p) {
        return cur == p.cur &&
            map == p.map &&
            hold == p.hold /* &&
            evalParm.status.under_attack == p.evalParm.status.under_attack &&
            evalParm.status.b2b == p.evalParm.status.b2b &&
			evalParm.status.combo == p.evalParm.status.combo*/;
    }
    TreeNode* generateChildNode(TetrisNode&, bool, std::optional<Piece>, bool);
    void search();
    void search_hold(bool op = false, bool noneFirstHold = false);
    bool eval();
    void run();
    TreeContext::Result getBest();
};


