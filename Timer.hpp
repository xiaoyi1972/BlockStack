#pragma once
#include<chrono>
#include<thread>

class timer
{
public:
timer() : m_begin(std::chrono::high_resolution_clock::now()) {}
void reset() { m_begin = std::chrono::high_resolution_clock::now(); }
//ƒ¨»œ ‰≥ˆ∫¡√Î
int64_t elapsed() const
{
return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}
//Œ¢√Î
int64_t elapsed_micro() const
{
return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}
//ƒ…√Î
int64_t elapsed_nano() const
{
return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}
//√Î
int64_t elapsed_seconds() const
{
return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
}
//∑÷
int64_t elapsed_minutes() const
{
return std::chrono::duration_cast<std::chrono::minutes>(std::chrono::high_resolution_clock::now() - m_begin).count();
}
// ±
int64_t elapsed_hours() const
{
return std::chrono::duration_cast<std::chrono::hours>(std::chrono::high_resolution_clock::now() - m_begin).count();
}

template<class T, class U>
static void Sleep(std::chrono::duration<T, U> ss) {
    auto target = std::chrono::steady_clock::now() + ss; // the target end time

    // Sleep short. 5 ms is just an example. You need to trim that parameter.
    std::this_thread::sleep_until(target - std::chrono::milliseconds(5));

    // busy-wait the remaining time
    while (std::chrono::steady_clock::now() < target) {}
}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
};