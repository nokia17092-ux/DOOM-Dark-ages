#include <SDL.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {
fs::path executableDirectory() {
    char* base = SDL_GetBasePath();
    if (!base) return fs::current_path();
    fs::path result(base);
    SDL_free(base);
    return result;
}

bool installIsComplete(const fs::path& directory) {
#ifdef _WIN32
    const std::array<fs::path, 15> required = {
        "vkQuake.exe", "SDL3.dll", "libFLAC-8.dll", "libmad-0.dll", "libmikmod-3.dll",
        "libmpg123-0.dll", "libogg-0.dll", "libopus-0.dll", "libopusfile-0.dll",
        "libvorbis-0.dll", "libvorbisfile-3.dll", "libxmp.dll", "id1/pak0.pak",
        "id1/pak1.pak", "id1/docs/COPYING"
    };
#else
    const std::array<fs::path, 4> required = {
        "vkQuake", "id1/pak0.pak", "id1/pak1.pak", "id1/docs/COPYING"
    };
#endif
    for (const auto& path : required) {
        if (!fs::is_regular_file(directory / path)) return false;
    }
    return fs::is_regular_file(directory / "id1/docs/CREDITS");
}

int launchEngine(const fs::path& directory) {
#ifdef _WIN32
    const fs::path engine = directory / "vkQuake.exe";
    std::wstring command = L"\"" + engine.wstring() + L"\" +map lq_e0m1";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(engine.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        directory.c_str(), &startup, &process)) {
        std::cerr << "Could not start vkQuake. Windows error: " << GetLastError() << '\n';
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
#else
    const fs::path engine = directory / "vkQuake";
    if (!fs::is_regular_file(engine)) {
        std::cerr << "vkQuake is missing next to the launcher.\n";
        return 1;
    }
    const pid_t process = fork();
    if (process < 0) {
        std::cerr << "Could not start vkQuake: " << errno << '\n';
        return 1;
    }
    if (process == 0) {
        if (chdir(directory.c_str()) != 0) _exit(126);
        execl(engine.c_str(), engine.filename().c_str(), "+map", "lq_e0m1", nullptr);
        _exit(127);
    }
    return 0;
#endif
}

int showLauncher(const fs::path& directory) {
    SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "PLAY CAMPAIGN"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "EXIT"}
    };
    const SDL_MessageBoxData dialog = {
        SDL_MESSAGEBOX_INFORMATION,
        nullptr,
        "DOOM DARK AGES",
        "Launch the LibreQuake campaign?\n\nReal-time 3D, animated monsters, weapons, sound and music.",
        SDL_arraysize(buttons),
        buttons,
        nullptr
    };
    int selected = 0;
    if (SDL_ShowMessageBox(&dialog, &selected) != 0) {
        std::cerr << "Could not show launcher: " << SDL_GetError() << '\n';
        return 1;
    }
    if (selected != 1) return 0;
    return launchEngine(directory);
}
}

int main(int argc, char** argv) {
    const fs::path directory = executableDirectory();
    if (argc > 1 && std::string(argv[1]) == "--verify-install") {
        if (!installIsComplete(directory)) {
            std::cerr << "The portable package is missing vkQuake, runtime DLLs, or LibreQuake data.\n";
            return 2;
        }
        std::cout << "vkQuake runtime and LibreQuake data are installed.\n";
        return 0;
    }
    if (!installIsComplete(directory)) {
        std::cerr << "The portable package is incomplete. Keep the launcher, vkQuake files, and id1 folder together.\n";
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "Could not initialize launcher UI: " << SDL_GetError() << '\n';
        return 1;
    }
    const int result = showLauncher(directory);
    SDL_Quit();
    return result;
}
