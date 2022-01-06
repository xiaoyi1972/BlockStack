#pragma once
#include<chrono>
#include<thread>

class Timer
{
public:
Timer() : m_begin(std::chrono::high_resolution_clock::now()) {}
void reset() { m_begin = std::chrono::high_resolution_clock::now(); }

enum timeType { h, m, s, ms, us, ns };

template<timeType c = timeType::ms>
auto elapsed() const {
    int64_t cost;
	switch (c) {
	case timeType::ms : 
        cost = std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::high_resolution_clock::now() - m_begin).count();
        break;
    case timeType::us: 
        cost = std::chrono::duration_cast<std::chrono::microseconds>
			(std::chrono::high_resolution_clock::now() - m_begin).count();
        break;
    case timeType::ns :
        cost = std::chrono::duration_cast<std::chrono::nanoseconds>
			(std::chrono::high_resolution_clock::now() - m_begin).count();
        break;
    case timeType::s:
		cost = std::chrono::duration_cast<std::chrono::seconds>
			(std::chrono::high_resolution_clock::now() - m_begin).count();
		break;
    case timeType::m:
        cost = std::chrono::duration_cast<std::chrono::minutes>
			(std::chrono::high_resolution_clock::now() - m_begin).count();
       break;
	case timeType::h:
		cost = std::chrono::duration_cast<std::chrono::hours>
            (std::chrono::high_resolution_clock::now() - m_begin).count();
	   break;
    }
    return cost;
}

template<class T, class U>
static void sleep(std::chrono::duration<T, U> ss) {
    auto target = std::chrono::steady_clock::now() + ss; // the target end time
    // Sleep short. 5 ms is just an example. You need to trim that parameter.
    std::this_thread::sleep_until(target - std::chrono::milliseconds(5));
    // busy-wait the remaining time
    while (std::chrono::steady_clock::now() < target) {}
}

template<timeType c,class Func, class ...Args>
static auto measure(Func &&func, Args && ...args) {
    Timer t;
	std::forward<decltype(func)>(func)(std::forward<decltype(args)>(args)...);
	return t.elapsed<c>();
}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
};