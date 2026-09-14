#include "issuetrace/update_package.hpp"

#include <chrono>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef ISSUETRACE_UPDATE_PLATFORM
#error ISSUETRACE_UPDATE_PLATFORM must be defined
#endif
#ifndef ISSUETRACE_UPDATE_ARCHITECTURE
#error ISSUETRACE_UPDATE_ARCHITECTURE must be defined
#endif

namespace {

struct Options {
    std::filesystem::path archive;
    std::filesystem::path installRoot;
    std::filesystem::path executableRelative;
    std::uint64_t waitPid{};
};

std::string randomSuffix() {
    std::random_device source;
    constexpr char hex[] = "0123456789abcdef";
    std::string result(12, '0');
    for (auto& value : result) value = hex[source() & 0xfU];
    return result;
}

Options parseOptions(const int argc, char* argv[]) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (index + 1 >= argc) throw std::invalid_argument("升级参数不完整");
        const std::string value = argv[++index];
        if (option == "--archive") result.archive = std::filesystem::path(value);
        else if (option == "--install-root") {
            result.installRoot = std::filesystem::path(value);
        } else if (option == "--executable") {
            result.executableRelative = std::filesystem::path(value);
        } else if (option == "--wait-pid") {
            result.waitPid = std::stoull(value);
        } else {
            throw std::invalid_argument("未知升级参数：" + option);
        }
    }
    if (result.archive.empty() || result.installRoot.empty() ||
        result.executableRelative.empty() || result.waitPid == 0) {
        throw std::invalid_argument("缺少升级所需参数");
    }
    if (result.executableRelative.is_absolute()) {
        throw std::invalid_argument("启动程序路径必须相对于便携目录");
    }
    for (const auto& part : result.executableRelative.lexically_normal()) {
        if (part == "..") throw std::invalid_argument("启动程序路径不安全");
    }
    return result;
}

#ifdef _WIN32
Options parseOptions(const int argc, wchar_t* argv[]) {
    Options result;
    for (int index = 1; index < argc; ++index) {
        const std::wstring option = argv[index];
        if (index + 1 >= argc) throw std::invalid_argument("升级参数不完整");
        const std::wstring value = argv[++index];
        if (option == L"--archive") result.archive = value;
        else if (option == L"--install-root") result.installRoot = value;
        else if (option == L"--executable") result.executableRelative = value;
        else if (option == L"--wait-pid") result.waitPid = std::stoull(value);
        else throw std::invalid_argument("未知升级参数");
    }
    if (result.archive.empty() || result.installRoot.empty() ||
        result.executableRelative.empty() || result.waitPid == 0) {
        throw std::invalid_argument("缺少升级所需参数");
    }
    if (result.executableRelative.is_absolute()) {
        throw std::invalid_argument("启动程序路径必须相对于便携目录");
    }
    for (const auto& part : result.executableRelative.lexically_normal()) {
        if (part == "..") throw std::invalid_argument("启动程序路径不安全");
    }
    return result;
}
#endif

#ifdef _WIN32
std::wstring quoteArgument(const std::wstring& value) {
    if (value.find_first_of(L" \t\"") == std::wstring::npos) return value;
    std::wstring result{L"\""};
    unsigned backslashes = 0;
    for (const auto character : value) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'\"') {
            result.append(backslashes * 2U + 1U, L'\\');
            result.push_back(character);
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(character);
        }
    }
    result.append(backslashes * 2U, L'\\');
    result.push_back(L'\"');
    return result;
}

std::wstring commandLine(const std::filesystem::path& program,
                         const std::vector<std::filesystem::path>& arguments) {
    auto result = quoteArgument(program.wstring());
    for (const auto& argument : arguments) {
        result.push_back(L' ');
        result += quoteArgument(argument.wstring());
    }
    return result;
}

std::string runAndCapture(const std::filesystem::path& program,
                          const std::vector<std::filesystem::path>& arguments) {
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0) ||
        !SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error("无法创建升级进程管道");
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    auto command = commandLine(program, arguments);
    const auto created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr,
                                        &startup, &process);
    CloseHandle(writePipe);
    if (!created) {
        CloseHandle(readPipe);
        throw std::runtime_error("无法启动系统归档工具 tar.exe");
    }
    std::string output;
    std::array<char, 4096> buffer{};
    DWORD count = 0;
    while (ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()),
                    &count, nullptr) && count > 0) {
        output.append(buffer.data(), count);
    }
    CloseHandle(readPipe);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exitCode != 0) throw std::runtime_error("归档工具执行失败：" + output);
    return output;
}

void waitForParent(const std::uint64_t pid) {
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (process) {
        WaitForSingleObject(process, INFINITE);
        CloseHandle(process);
    }
}

struct ChildProcess { HANDLE handle{}; };

ChildProcess launchApplication(const std::filesystem::path& executable,
                               const std::filesystem::path& healthMarker) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    auto command = commandLine(executable,
        {"--update-health-marker", healthMarker});
    if (!CreateProcessW(executable.wstring().c_str(), command.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, executable.parent_path().wstring().c_str(),
                        &startup, &process)) {
        throw std::runtime_error("新版程序无法启动");
    }
    CloseHandle(process.hThread);
    return {process.hProcess};
}

bool waitForHealth(ChildProcess child, const std::filesystem::path& marker) {
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (std::filesystem::is_regular_file(marker)) {
            CloseHandle(child.handle);
            return true;
        }
        if (WaitForSingleObject(child.handle, 0) == WAIT_OBJECT_0) {
            CloseHandle(child.handle);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    TerminateProcess(child.handle, 2);
    WaitForSingleObject(child.handle, 5000);
    CloseHandle(child.handle);
    return false;
}
#else
std::string runAndCapture(const std::filesystem::path& program,
                          const std::vector<std::filesystem::path>& arguments) {
    int descriptor[2]{};
    if (pipe(descriptor) != 0) throw std::runtime_error("无法创建升级进程管道");
    const auto child = fork();
    if (child < 0) throw std::runtime_error("无法启动系统归档工具");
    if (child == 0) {
        close(descriptor[0]);
        dup2(descriptor[1], STDOUT_FILENO);
        dup2(descriptor[1], STDERR_FILENO);
        close(descriptor[1]);
        std::vector<std::string> storage{program.string()};
        for (const auto& argument : arguments) storage.push_back(argument.string());
        std::vector<char*> values;
        for (auto& value : storage) values.push_back(value.data());
        values.push_back(nullptr);
        execvp(values[0], values.data());
        _exit(127);
    }
    close(descriptor[1]);
    std::string output;
    std::array<char, 4096> buffer{};
    ssize_t count = 0;
    while ((count = read(descriptor[0], buffer.data(), buffer.size())) > 0) {
        output.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(descriptor[0]);
    int status = 0;
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("归档工具执行失败：" + output);
    }
    return output;
}

void waitForParent(const std::uint64_t pid) {
    while (kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

struct ChildProcess { pid_t pid{}; };

ChildProcess launchApplication(const std::filesystem::path& executable,
                               const std::filesystem::path& healthMarker) {
    const auto child = fork();
    if (child < 0) throw std::runtime_error("新版程序无法启动");
    if (child == 0) {
        const auto executableText = executable.string();
        const auto markerText = healthMarker.string();
        execl(executableText.c_str(), executableText.c_str(),
              "--update-health-marker", markerText.c_str(), nullptr);
        _exit(127);
    }
    return {child};
}

bool waitForHealth(const ChildProcess child, const std::filesystem::path& marker) {
    for (int attempt = 0; attempt < 300; ++attempt) {
        if (std::filesystem::is_regular_file(marker)) return true;
        int status = 0;
        if (waitpid(child.pid, &status, WNOHANG) == child.pid) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    kill(child.pid, SIGTERM);
    waitpid(child.pid, nullptr, 0);
    return false;
}
#endif

bool safeArchiveEntry(const std::string& entry) {
    if (entry.empty() || entry.front() == '/' || entry.front() == '\\' ||
        entry.find('\\') != std::string::npos || entry.find(':') != std::string::npos) {
        return false;
    }
    const auto original = std::filesystem::path(entry);
    for (const auto& part : original) {
        if (part == ".." || part == ".") return false;
    }
    const auto path = original.lexically_normal();
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    return true;
}

std::string validateArchiveListing(const std::string& listing) {
    std::istringstream lines(listing);
    std::string entry;
    std::string topLevel;
    std::set<std::string> normalized;
    bool hasManifest = false;
    while (std::getline(lines, entry)) {
        if (!entry.empty() && entry.back() == '\r') entry.pop_back();
        if (!safeArchiveEntry(entry)) throw std::runtime_error("压缩包包含不安全路径");
        const auto path = std::filesystem::path(entry).lexically_normal();
        const auto first = path.begin()->string();
        if (topLevel.empty()) topLevel = first;
        if (first != topLevel) throw std::runtime_error("压缩包必须只有一个顶层目录");
        auto key = path.generic_string();
        if (!key.empty() && key.back() == '/') key.pop_back();
        if (!normalized.insert(key).second) {
            throw std::runtime_error("压缩包包含重复路径：" + key);
        }
        if (path == std::filesystem::path(topLevel) / "release-manifest.json") {
            hasManifest = true;
        }
    }
    if (topLevel.empty() || !hasManifest) {
        throw std::runtime_error("压缩包缺少顶层目录或 release-manifest.json");
    }
    return topLevel;
}

void appendLog(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::app);
    output << text << '\n';
}

int applyUpdate(const Options& options) {
    if (!std::filesystem::is_regular_file(options.archive) ||
        !std::filesystem::is_directory(options.installRoot)) {
        throw std::runtime_error("升级包或当前程序目录不存在");
    }
    const auto currentManifest = issuetrace::readReleaseManifest(
        options.installRoot / "release-manifest.json");
    issuetrace::validateReleaseDirectory(options.installRoot, currentManifest,
        ISSUETRACE_UPDATE_PLATFORM, ISSUETRACE_UPDATE_ARCHITECTURE, false);

    const auto suffix = randomSuffix();
    const auto parent = options.installRoot.parent_path();
    const auto staging = parent / (".issuetrace-update-stage-" + suffix);
    const auto previous = parent / (options.installRoot.filename().string() + ".previous");
    const auto olderPrevious = parent / (".issuetrace-old-previous-" + suffix);
    const auto healthMarker = parent / (".issuetrace-health-" + suffix);
    const auto logPath = parent / "IssueTrace-update.log";
    bool switched = false;
    bool movedOlderPrevious = false;
    try {
        const auto listing = runAndCapture("tar", {"-tf", options.archive});
        const auto topLevel = validateArchiveListing(listing);
        std::filesystem::create_directories(staging);
        static_cast<void>(runAndCapture(
            "tar", {"-xf", options.archive, "-C", staging}));
        const auto incomingRoot = staging / topLevel;
        const auto incomingManifest = issuetrace::readReleaseManifest(
            incomingRoot / "release-manifest.json");
        issuetrace::validateReleaseDirectory(incomingRoot, incomingManifest,
            ISSUETRACE_UPDATE_PLATFORM, ISSUETRACE_UPDATE_ARCHITECTURE);
        if (issuetrace::compareVersions(incomingManifest.version,
                                      currentManifest.version) <= 0) {
            throw std::runtime_error("升级包版本必须高于当前版本");
        }
        if (!std::filesystem::is_regular_file(incomingRoot /
                                               options.executableRelative)) {
            throw std::runtime_error("升级包缺少主程序");
        }
        waitForParent(options.waitPid);
        if (std::filesystem::exists(previous)) {
            std::filesystem::rename(previous, olderPrevious);
            movedOlderPrevious = true;
        }
        std::filesystem::rename(options.installRoot, previous);
        switched = true;
        std::filesystem::rename(incomingRoot, options.installRoot);
        std::filesystem::remove_all(staging);
        const auto child = launchApplication(
            options.installRoot / options.executableRelative, healthMarker);
        if (!waitForHealth(child, healthMarker)) {
            throw std::runtime_error("新版未能通过启动健康检查");
        }
        std::filesystem::remove(healthMarker);
        if (movedOlderPrevious) std::filesystem::remove_all(olderPrevious);
        appendLog(logPath, "升级成功：" + currentManifest.version + " -> " +
                           incomingManifest.version);
        return 0;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(healthMarker, ignored);
        std::filesystem::remove_all(staging, ignored);
        if (switched) {
            std::filesystem::remove_all(options.installRoot, ignored);
            std::filesystem::rename(previous, options.installRoot, ignored);
        }
        if (movedOlderPrevious) {
            if (!std::filesystem::exists(previous)) {
                std::filesystem::rename(olderPrevious, previous, ignored);
            }
        }
        throw;
    }
}

int run(const Options& options) {
    try {
        return applyUpdate(options);
    } catch (const std::exception& error) {
        std::cerr << "IssueTrace update failed: " << error.what() << '\n';
        try {
            if (!options.installRoot.empty()) {
                appendLog(options.installRoot.parent_path() / "IssueTrace-update.log",
                          std::string("升级失败：") + error.what());
            }
        } catch (...) {
        }
        return 1;
    }
}

}  // namespace

#ifdef _WIN32
int main() {
    int argumentCount = 0;
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) return 1;
    try {
        const auto options = parseOptions(argumentCount, arguments);
        LocalFree(arguments);
        return run(options);
    } catch (const std::exception& error) {
        LocalFree(arguments);
        std::cerr << "IssueTrace update failed: " << error.what() << '\n';
        return 1;
    }
}
#else
int main(int argc, char* argv[]) {
    try {
        return run(parseOptions(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "IssueTrace update failed: " << error.what() << '\n';
        return 1;
    }
}
#endif
