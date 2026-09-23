#include "platform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

bool copy_clipboard(const std::string& text) {
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (!length || !OpenClipboard(nullptr)) return false;
    HGLOBAL allocation = GlobalAlloc(GMEM_MOVEABLE, length * sizeof(wchar_t));
    bool copied = false;
    if (allocation) {
        auto buffer = (wchar_t*) GlobalLock(allocation);
        if (buffer) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buffer, length);
            GlobalUnlock(allocation);
            if (EmptyClipboard()) copied = SetClipboardData(CF_UNICODETEXT, allocation) != nullptr;
        }
        if (!copied) GlobalFree(allocation);
    }
    CloseClipboard();
    return copied;
}

bool open_external_url(const std::string& url) {
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) return false;
    return ((INT_PTR) ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;

namespace {
    bool run_program(std::vector<std::string> arguments, FILE* input = nullptr) {
        std::vector<char*> pointers;
        for (auto& argument : arguments) pointers.push_back(argument.data());
        pointers.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions) != 0) return false;
        if (input) posix_spawn_file_actions_adddup2(&actions, fileno(input), STDIN_FILENO);
        else posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        pid_t child;
        int result = posix_spawnp(&child, pointers.front(), &actions, nullptr, pointers.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (result != 0) return false;
        int status = 0;
        while (waitpid(child, &status, 0) < 0) {
            if (errno != EINTR) return false;
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
} // namespace

bool copy_clipboard(const std::string& text) {
    FILE* input = std::tmpfile();
    if (!input) return false;
    if (std::fwrite(text.data(), 1, text.size(), input) != text.size()) {
        std::fclose(input);
        return false;
    }
    std::rewind(input);
#ifdef __APPLE__
    bool copied = run_program({"pbcopy"}, input);
#else
    bool copied = false;
    if (std::getenv("WAYLAND_DISPLAY")) copied = run_program({"wl-copy"}, input);
    if (!copied && std::getenv("DISPLAY")) {
        std::rewind(input);
        copied = run_program({"xclip", "-selection", "clipboard"}, input);
    }
#endif
    std::fclose(input);
    return copied;
}

bool open_external_url(const std::string& url) {
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) return false;
#ifdef __APPLE__
    return run_program({"open", url});
#else
    return run_program({"xdg-open", url});
#endif
}
#endif
