// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include <ctime>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include "threadPool.h"
#include <vector>
#include <iostream>
#include <algorithm>

static const unsigned int threads = 2u;

#define USE_CONSOLE 0

#ifdef _MSC_VER
#include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    FILE* stream;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
#if USE_CONSOLE
        AllocConsole();
        freopen_s(&stream,"CONOUT$", "w+t", stdout);
        freopen_s(&stream,"CONIN$", "r+t", stdin);
#endif
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN


#define DECLSPEC_EXPORT __declspec(dllexport)
#define WINAPI __stdcall

extern "C" DECLSPEC_EXPORT char const* WINAPI Name()
{
    static std::string name = "test";
    if (threads > 1)
        name += "_t" + std::to_string(threads);
    return name.c_str();
}

extern "C" DECLSPEC_EXPORT int __cdecl AIDllVersion()
{
    return 2;
}

extern "C" DECLSPEC_EXPORT char* __cdecl AIName(int level)
{
    static char name[200];
    static std::string name1 = "test";
    if (threads > 1) 
		name1 += "_t" + std::to_string(threads);
#ifdef _MSC_VER
    strcpy_s(name, name1.c_str());
#else
    strcpy(name, name1.c_str());
#endif
    return name;
}


auto typeConvert(char type) {
    auto result = Piece::None;
    switch (type) {
    case 'O':result = Piece::O; break;
    case 'T':result = Piece::T; break;
    case 'L':result = Piece::L; break;
    case 'J':result = Piece::J; break;
    case 'S':result = Piece::S; break;
    case 'Z':result = Piece::Z; break;
    case 'I':result = Piece::I; break;
    }
    return result;
};

auto operConvert(std::vector<Oper>& path) {
    std::string a;
    a.reserve(path.size());
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
};

static auto dealMap(int overfield[], int field[], TetrisMap& map) {
    for (size_t d = 0, s = 22; d < 23; ++d, --s)
    {
        map[d] = field[s];
    }
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
        map[d] = overfield[s];
    }
    map.update();
};


extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b,
    int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, 
    bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    static double const base_time = std::pow(100, 1.0 / 8);
    static char result_buffer[8][1024];
    char* result = result_buffer[player];

#ifndef UNIT
    if (field_w != 10 || field_h != 22)
    {
        *result = '\0';
        return result;
    }

    //std::cout << "dll call\n";

    //TetrisMap map{ 10, 40 };

#if USE_TEST
    for (size_t d = 0; d < 23; ++d)
    {
        for (auto i = 0; i < 10; i++) {
            if ((field[d] >> i) & 1)
                printf("[]");
            else
                printf("  ");
        }
        printf(" %2d", d);
        printf("\n");
    }
    printf("============\n");
#endif
	//std::cout << "cur:" << active << " x:" << x << " y: " << y << "\n";

    TetrisDelegator::GameState v;
    TetrisDelegator::OutInfo tag;

	v.field.emplace<TetrisMap>(10, 40);
    v.bag.emplace<std::vector<Piece>>();

//#define USE_OUTPUT_INFO1
#ifdef USE_OUTPUT_INFO1
    static std::ostringstream os;
#endif

    dealMap(overfield, field, v.field);
    auto cur_type = typeConvert(active);
    v.dySpawn = -18;
	v.cur = TetrisNode::spawn(cur_type, &v.field(), v.dySpawn);

	for (auto i = 0; i < maxDepth; i++) {
		v.bag->emplace_back(typeConvert(next[i]));
    }

#ifdef USE_OUTPUT_INFO
    std::cout << "preview:{";
    for (auto it : v.bag()) {
        std::cout << it;
    }
	std::cout << "}\n";
#endif
    
	v.hold = (hold == ' ') ? Piece::None : typeConvert(hold);
    v.b2b = b2b;
    v.combo = combo;
	v.upAtt = upcomeAtt;
    v.canHold = curCanHold;

	static TetrisDelegator bot = TetrisDelegator::launch({ .use_static = true,.delete_byself = true,.thread_num = threads });
    auto call = [&](TetrisNode &start, TetrisMap &field, const int limitTime) -> std::vector<Oper>{
        //std::cout << "start callbot.\n";
        bot.run(v, limitTime);
        //std::cout << "start callbot make_path..\n";
        auto path = bot.suggest(v, tag);
        if (!path) {
            path = std::vector<Oper>{ Oper::HardDrop };
        }
#ifdef USE_OUTPUT_INFO1
        os << tag.landpoint->mapping(field);
        os << tag.info << "\n";
#endif
        return *path;
    };

	auto path = call(v.cur, v.field, time_t(std::pow(base_time, level)));
    auto pathStr = operConvert(path);

#ifdef USE_OUTPUT_INFO1
    os << active << "[" << hold << "]" << " " << next << "\n";
    os << "upAtt:" << upcomeAtt << " path:" << pathStr <<"\n\n";
    std::cout << os.str();
    os.str("");
#endif

    std::memcpy(result, pathStr.data(), pathStr.size());
    result += pathStr.size();
    result[0] = '\0';
    //std::cout << "end\n\n";
#endif
    return result_buffer[player];
}

#undef USE_TEST
/*int main() {

    struct {
        int overfield[8]{ 0 };
        int field[23]{ 0 };
        const int field_w = 10;
        const int field_h = 22;
        int b2b = 0;
        int combo = 0;
        const char* next = "SZLJLS";
        char hold = 'T';
        bool curCanHold = true;
        bool can180spin = false;
        int upcomeAtt = 0;
        int comboTable[13]{ 0,0,1,1,2,2,3,3,4,4,4,5,-1 };
        int maxDepth = strlen(next);
        int level = 8;
        int player = 0;
        char active = 'L';
        int x = 3;
        int y = 0;
        int spin = 0;
        bool canhold = false;
    }v;

   // v.field[20] = 0b1111111011;
   // v.field[21] = 0b1111111101;
   // v.field[22] = 0b1111111110;
    const char* path = TetrisAI(v.overfield, v.field, v.field_w, v.field_h,
        v.b2b, v.combo, const_cast<char*>(v.next), v.hold, v.curCanHold,
        v.active, v.x, v.y, v.spin, v.canhold, v.can180spin, v.upcomeAtt, v.comboTable,
        v.maxDepth, v.level, v.player);

    std::cout << path;
    return 0;
}*/
