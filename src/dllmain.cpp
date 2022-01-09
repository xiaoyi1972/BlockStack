// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include <windows.h>
#include <ctime>
#include <vector>
#include "tetrisGame.h"
#include "tetrisCore.h"
#include <iostream>
#define USE_CONSOLE 0
BOOL APIENTRY DllMain(HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    FILE* stream1;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
#if USE_CONSOLE
        AllocConsole();
        freopen_s(&stream1,"CONOUT$", "w+t", stdout);
        freopen_s(&stream1,"CONIN$", "r+t", stdin);
#endif
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

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
    strcpy_s(name, name1.c_str());
    return name;
}

extern "C" DECLSPEC_EXPORT char* __cdecl TetrisAI(int overfield[], int field[], int field_w, int field_h, int b2b,
    int combo, char next[], char hold, bool curCanHold, char active, int x, int y, int spin, bool canhold, 
    bool can180spin, int upcomeAtt, int comboTable[], int maxDepth, int level, int player)
{
    double const base_time = std::pow(100, 1.0 / 8);
    auto typeConvert = [](char type) {
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

    auto operConvert = [](std::vector<Oper>& path) {
        std::string a;
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

    static char result_buffer[8][1024];
    char* result = result_buffer[player];

    if (field_w != 10 || field_h != 22)
    {
        *result = '\0';
        return result;
    }

    TetrisMapEx map{ 10, 40 };

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

	for (size_t d = 0, s = 22; d < 23; ++d, --s)
	{
		map[d] = field[s];
	}
	for (size_t d = 23, s = 0; s < 8; ++d, ++s)
	{
		map[d] = overfield[s];
	}

#if USE_TEST
    for (size_t d = 23, s = 0; s < 8; ++d, ++s)
    {
      //  map.data[map.height - 23 - (8 - s)] = overfield[s];
    }
#endif

    auto drawField = [&](TetrisMap&map) {
        for (int i = 0; i < map.height; i++) {//draw field
            for (int j = 0; j < map.width; j++)
            {
                std::cout << (map(i, j) ? "[]" : "  ");
            }
            std::cout << "\n";
        }
    };

    //drawField(map);
    map.update();
    struct {
        std::vector<Piece> nexts;
		std::optional<Piece> hold;
        Piece cur;
        int b2b = 0;
        int combo = 0;
        int upcomeAtt = 0;
    }v;
   // printf("x %d y %d spin %d",x,y, spin);

    v.cur = typeConvert(active);
    auto dySpawn = -18;
	auto node = TetrisNode::spawn(v.cur, &map, dySpawn);
    for (auto it = next; *it != '\0'; it++) { v.nexts.push_back(typeConvert(*it)); }
	v.hold = (hold == ' ') ? std::nullopt : std::make_optional(typeConvert(hold));
    v.b2b = b2b;
    v.combo = combo;
	v.upcomeAtt = upcomeAtt;

    auto call = [&](TetrisNode &start, TetrisMap &field, const int limitTime) {
        using std::chrono::high_resolution_clock;
        using std::chrono::milliseconds;
       // static
            TreeContext ctx;
        auto dp = std::vector(v.nexts.begin(), v.nexts.end());
        ctx.createRoot(start, field, dp, v.hold, !!v.b2b, v.combo,v.upcomeAtt, dySpawn);
        auto now = high_resolution_clock::now(), end = now + milliseconds(limitTime);
        do {
            ctx.run();
        } while ((now = high_resolution_clock::now()) < end);
        auto [best, ishold] = ctx.getBest();
        std::vector<Oper>path;
        if (!ishold) {
			path = Search::make_path(start, best, field, true);
        }
        else {
            auto holdNode = TetrisNode::spawn(v.hold ? *v.hold : dp.front(), &map, dySpawn);
			path = Search::make_path(holdNode, best, field, true);
            path.insert(path.begin(), Oper::Hold);
        }
        if (path.empty()) path.push_back(Oper::HardDrop);
        return path;
    };

	auto path = call(node, map, time_t(std::pow(base_time, level)));
    auto pathStr = operConvert(path);
  //  std::cout << path<<"\n";
    std::memcpy(result, pathStr.data(), pathStr.size());
    result += pathStr.size();
    result[0] = '\0';
    //printf("%s", result_buffer[player]);
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
