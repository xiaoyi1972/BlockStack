// system header:
#ifdef _WIN32
#include <conio.h>
#else // (not) _WIN32
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#endif // _WIN32

/// reads a character from console without echo.
#ifdef _WIN32
inline int getChar() { return _getch(); }
#else // (not) _WIN32
int getChar()
{
    struct termios oldattr;
    tcgetattr(STDIN_FILENO, &oldattr);
    struct termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
    const int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
    return ch;
}
#endif // _WIN32

// standard C/C++ header:
#include <cstring>
#include <mutex>
#include <string>

/* provides a class for a simple thread-safe mini-shell.
 *
 * It is assumed that one thread may await user input (read()) while
 * another thread may (or may not) output text from time to time.
 * The mini-shell grants that the input line is always the last line.
 */
class Console {

    // variable:
private:
    // mutex for console I/O
    std::mutex _mtx;
    // current input
    std::string _input;
    // prompt output
    std::string _prompt;

    // methods:
public:
    /// constructor.
    Console() { }

    // disabled:
    Console(const Console&) = delete;
    Console& operator = (const Console&) = delete;

    // reads a line from console and returns input string
    std::string read();

    /* writes text to console.
     *
     * text the text
     * size size of text
     */
    void write(const char* text, size_t size);
    void write(const char* text) { write(text, strlen(text)); }
    void write(const std::string& text) { write(text.c_str(), text.size()); }
};

// standard C/C++ header:
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

std::string Console::read()
{
    { // activate prompt
        std::lock_guard<std::mutex> lock(_mtx);
        _prompt = "> "; _input.clear();
        std::cout << _prompt << std::flush;
    }
#ifdef _WIN32
    enum { Enter = '\r', BackSpc = '\b' };
#else // (not) _WIN32
    enum { Enter = '\n', BackSpc = 127 };
#endif // _WIN32
    // input loop
    for (;;) {
        switch (int c = getChar()) {
        case Enter: {
            std::lock_guard<std::mutex> lock(_mtx);
            std::string input = _input;
            _prompt.clear(); _input.clear();
            std::cout << std::endl;
            return input;
        } // unreachable: break;
        case BackSpc: {
            std::lock_guard<std::mutex> lock(_mtx);
            if (_input.empty()) break; // nothing to do
            _input.pop_back();
            std::cout << "\b \b" << std::flush;
        } break;
        default: {
            if (c < ' ' || c >= '\x7f') break;
            std::lock_guard<std::mutex> lock(_mtx);
            _input += c;
            std::cout << (char)c << std::flush;
        } break;
        }
    }
}

void Console::write(const char* text, size_t len)
{
    if (!len) return; // nothing to do
    bool eol = text[len - 1] == '\n';
    std::lock_guard<std::mutex> lock(_mtx);
    // remove current input echo
    if (size_t size = _prompt.size() + _input.size()) {
        std::cout
            << std::setfill('\b') << std::setw(size) << ""
            << std::setfill(' ') << std::setw(size) << ""
            << std::setfill('\b') << std::setw(size) << "";
    }
    // print text
    std::cout << text;
    if (!eol) std::cout << std::endl;
    // print current input echo
    std::cout << _prompt << _input << std::flush;
}

// a sample application
