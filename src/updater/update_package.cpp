#include "issuetrace/update_package.hpp"

#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace issuetrace {
namespace {

constexpr std::array<std::uint32_t, 64> roundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

std::uint32_t rotateRight(const std::uint32_t value, const unsigned count) {
    return (value >> count) | (value << (32U - count));
}

std::string sha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("无法读取程序文件：" + path.generic_string());
    std::vector<unsigned char> data{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto bitLength = static_cast<std::uint64_t>(data.size()) * 8U;
    data.push_back(0x80U);
    while (data.size() % 64U != 56U) data.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) {
        data.push_back(static_cast<unsigned char>(bitLength >> shift));
    }

    std::array<std::uint32_t, 8> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t offset = 0; offset < data.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const auto position = offset + i * 4U;
            words[i] = (static_cast<std::uint32_t>(data[position]) << 24U) |
                       (static_cast<std::uint32_t>(data[position + 1]) << 16U) |
                       (static_cast<std::uint32_t>(data[position + 2]) << 8U) |
                       data[position + 3];
        }
        for (std::size_t i = 16; i < words.size(); ++i) {
            const auto s0 = rotateRight(words[i - 15], 7) ^
                            rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3U);
            const auto s1 = rotateRight(words[i - 2], 17) ^
                            rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10U);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state;
        for (std::size_t i = 0; i < words.size(); ++i) {
            const auto sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^
                              rotateRight(e, 25);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temporary1 = h + sum1 + choice + roundConstants[i] + words[i];
            const auto sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^
                              rotateRight(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = sum0 + majority;
            h = g; g = f; f = e; e = d + temporary1;
            d = c; c = b; b = a; a = temporary1 + temporary2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto value : state) result << std::setw(8) << value;
    return result.str();
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("无法读取发布清单");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string jsonStringField(const std::string& content, const char* name) {
    const std::regex expression(std::string("\\\"") + name +
                                "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(content, match, expression)) {
        throw std::runtime_error(std::string("发布清单缺少字段：") + name);
    }
    return match[1].str();
}

int jsonIntegerField(const std::string& content, const char* name) {
    const std::regex expression(std::string("\\\"") + name +
                                "\\\"\\s*:\\s*([0-9]+)");
    std::smatch match;
    if (!std::regex_search(content, match, expression)) {
        throw std::runtime_error(std::string("发布清单缺少字段：") + name);
    }
    return std::stoi(match[1].str());
}

bool safeRelativePath(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    for (const auto& part : path.lexically_normal()) {
        if (part == ".." || part == ".") return false;
    }
    return true;
}

bool pathIsWithin(const std::filesystem::path& candidate,
                  const std::filesystem::path& parent) {
    const auto normalizedCandidate = std::filesystem::absolute(candidate).lexically_normal();
    const auto normalizedParent = std::filesystem::absolute(parent).lexically_normal();
    auto candidatePart = normalizedCandidate.begin();
    for (auto parentPart = normalizedParent.begin(); parentPart != normalizedParent.end();
         ++parentPart, ++candidatePart) {
        if (candidatePart == normalizedCandidate.end() || *candidatePart != *parentPart) {
            return false;
        }
    }
    return true;
}

std::vector<int> versionParts(const std::string& version) {
    if (!std::regex_match(version, std::regex("[0-9]+(?:\\.[0-9]+){0,3}"))) {
        throw std::runtime_error("版本号格式不受支持：" + version);
    }
    std::vector<int> parts;
    std::stringstream stream(version);
    std::string part;
    while (std::getline(stream, part, '.')) parts.push_back(std::stoi(part));
    while (parts.size() < 4) parts.push_back(0);
    return parts;
}

}  // namespace

ReleaseManifest readReleaseManifest(const std::filesystem::path& manifestPath) {
    const auto content = readFile(manifestPath);
    ReleaseManifest manifest;
    manifest.schemaVersion = jsonIntegerField(content, "schemaVersion");
    manifest.product = jsonStringField(content, "product");
    manifest.version = jsonStringField(content, "version");
    manifest.platform = jsonStringField(content, "platform");
    manifest.architecture = jsonStringField(content, "architecture");
    const std::regex fileExpression(
        R"manifest(\{"path": "([^"]+)", "size": ([0-9]+), "sha256": "([0-9a-f]{64})"\})manifest");
    for (auto iterator = std::sregex_iterator(content.begin(), content.end(),
                                              fileExpression);
         iterator != std::sregex_iterator(); ++iterator) {
        manifest.files.push_back({(*iterator)[1].str(),
                                  static_cast<std::uintmax_t>(
                                      std::stoull((*iterator)[2].str())),
                                  (*iterator)[3].str()});
    }
    if (manifest.files.empty()) throw std::runtime_error("发布清单没有程序文件");
    return manifest;
}

void validateReleaseDirectory(const std::filesystem::path& packageRoot,
                              const ReleaseManifest& manifest,
                              const std::string& expectedPlatform,
                              const std::string& expectedArchitecture,
                              const bool rejectUnexpectedFiles) {
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(packageRoot)) ||
        !std::filesystem::is_directory(packageRoot) ||
        manifest.schemaVersion != 1 || manifest.product != "IssueTrace") {
        throw std::runtime_error("不是受支持的 IssueTrace 发布包");
    }
    if (manifest.platform != expectedPlatform ||
        manifest.architecture != expectedArchitecture) {
        throw std::runtime_error("发布包平台或架构与当前程序不匹配");
    }
    static_cast<void>(versionParts(manifest.version));
    std::map<std::string, const ReleaseFile*> expected;
    for (const auto& file : manifest.files) {
        const auto normalized = file.path.lexically_normal();
        if (!safeRelativePath(normalized)) {
            throw std::runtime_error("发布清单包含不安全路径");
        }
        const auto key = normalized.generic_string();
        if (!expected.emplace(key, &file).second) {
            throw std::runtime_error("发布清单包含重复路径：" + key);
        }
    }
    std::set<std::string> actual;
    for (const auto& item : std::filesystem::recursive_directory_iterator(packageRoot)) {
        if (item.is_symlink()) {
            const auto target = std::filesystem::read_symlink(item.path());
            const auto resolved = target.is_absolute()
                ? target.lexically_normal()
                : (item.path().parent_path() / target).lexically_normal();
            if (target.is_absolute() || !pathIsWithin(resolved, packageRoot) ||
                !std::filesystem::exists(resolved)) {
                throw std::runtime_error("发布包包含越界或失效的符号链接");
            }
            const auto targetRelative =
                std::filesystem::relative(resolved, packageRoot).generic_string();
            bool pointsToOwnedContent = expected.contains(targetRelative);
            const auto directoryPrefix = targetRelative + "/";
            if (!pointsToOwnedContent && std::filesystem::is_directory(resolved)) {
                for (const auto& [ownedPath, ignored] : expected) {
                    if (ownedPath.starts_with(directoryPrefix)) {
                        pointsToOwnedContent = true;
                        break;
                    }
                }
            }
            if (!pointsToOwnedContent) {
                throw std::runtime_error("发布包符号链接未指向已声明内容");
            }
            continue;
        }
        if (!item.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(item.path(), packageRoot);
        const auto key = relative.generic_string();
        if (key == "release-manifest.json") continue;
        const auto expectedFile = expected.find(key);
        if (expectedFile == expected.end()) {
            if (rejectUnexpectedFiles) {
                throw std::runtime_error("发布包包含未声明文件：" + key);
            }
            continue;
        }
        if (!actual.insert(key).second) {
            throw std::runtime_error("发布包包含重复文件：" + key);
        }
        if (item.file_size() != expectedFile->second->size ||
            sha256(item.path()) != expectedFile->second->sha256) {
            throw std::runtime_error("程序文件校验失败：" + key);
        }
    }
    if (actual.size() != expected.size()) {
        for (const auto& [path, ignored] : expected) {
            if (!actual.contains(path)) throw std::runtime_error("发布包缺少文件：" + path);
        }
    }
}

int compareVersions(const std::string& left, const std::string& right) {
    const auto leftParts = versionParts(left);
    const auto rightParts = versionParts(right);
    if (leftParts < rightParts) return -1;
    if (leftParts > rightParts) return 1;
    return 0;
}

}  // namespace issuetrace
