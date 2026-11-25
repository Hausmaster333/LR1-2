#ifndef TERMINAL_H
#define TERMINAL_H

#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

class Terminal {
    std::string pending, prompt_path;
    bool eof = false;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    bool console = GetConsoleMode(input, &mode);
    UINT old_output_cp = GetConsoleOutputCP();
    std::wstring typed;

    bool read_key(std::string& line) {
        DWORD count;
        INPUT_RECORD event;
        while (GetNumberOfConsoleInputEvents(input, &count) && count &&
               ReadConsoleInputW(input, &event, 1, &count)) {
            if (event.EventType != KEY_EVENT || !event.Event.KeyEvent.bKeyDown) continue;
            auto key = event.Event.KeyEvent;
            wchar_t ch = key.uChar.UnicodeChar;
            if (ch == L'\r') {
                int size = WideCharToMultiByte(CP_UTF8, 0, typed.data(), typed.size(), nullptr, 0, nullptr, nullptr);
                line.resize(size);
                WideCharToMultiByte(CP_UTF8, 0, typed.data(), typed.size(), line.data(), size, nullptr, nullptr);
                typed.clear();
                std::cout << '\n';
                return true;
            }
            for (unsigned i = 0; i < key.wRepeatCount; ++i) {
                if (ch == L'\b' && !typed.empty()) {
                    typed.pop_back();
                    std::cout << "\b \b" << std::flush;
                } else if (ch >= L' ') {
                    typed += ch;
                    DWORD written;
                    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &ch, 1, &written, nullptr);
                }
            }
        }
        return false;
    }
#endif
public:
    Terminal() {
#ifdef _WIN32
        if (old_output_cp) SetConsoleOutputCP(CP_UTF8);
#endif
    }
    ~Terminal() {
#ifdef _WIN32
        if (old_output_cp) SetConsoleOutputCP(old_output_cp);
#endif
    }
    void prompt(const std::string& path, bool force = false) {
        if (eof || (!force && prompt_path == path)) return;
        if (!force && !prompt_path.empty()) std::cout << '\n';
        prompt_path = path;
        std::cout << "vfs:" << path << "> " << std::flush;
#ifdef _WIN32
        if (console && !typed.empty()) {
            DWORD written;
            WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), typed.data(), typed.size(), &written, nullptr);
        }
#endif
    }
    bool poll_line(std::string& line) {
#ifdef _WIN32
        if (console) return read_key(line);
#endif
        if (!eof && pending.find('\n') == std::string::npos) {
            char bytes[4096];
#ifdef _WIN32
            DWORD count = 0;
            bool pipe = GetFileType(input) == FILE_TYPE_PIPE;
            if (pipe && !PeekNamedPipe(input, nullptr, 0, nullptr, &count, nullptr)) eof = true;
            if (pipe && !eof && !count) return false;
            if (!eof && ReadFile(input, bytes, sizeof(bytes), &count, nullptr) && count)
                pending.append(bytes, count);
            else eof = true;
#else
            pollfd fd{STDIN_FILENO, POLLIN, 0};
            if (poll(&fd, 1, 0) <= 0) return false;
            auto count = read(STDIN_FILENO, bytes, sizeof(bytes));
            if (count > 0) pending.append(bytes, count);
            else if (count == 0) eof = true;
#endif
        }
        auto end = pending.find('\n');
        if (end == std::string::npos && (!eof || pending.empty())) return false;
        line = pending.substr(0, end);
        pending.erase(0, end == std::string::npos ? end : end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return true;
    }
};

#endif