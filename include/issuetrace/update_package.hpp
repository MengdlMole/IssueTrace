#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace issuetrace {

struct ReleaseFile {
    std::filesystem::path path;
    std::uintmax_t size{};
    std::string sha256;
};

struct ReleaseManifest {
    int schemaVersion{};
    std::string product;
    std::string version;
    std::string platform;
    std::string architecture;
    std::vector<ReleaseFile> files;
};

[[nodiscard]] ReleaseManifest readReleaseManifest(
    const std::filesystem::path& manifestPath);
void validateReleaseDirectory(const std::filesystem::path& packageRoot,
                              const ReleaseManifest& manifest,
                              const std::string& expectedPlatform,
                              const std::string& expectedArchitecture,
                              bool rejectUnexpectedFiles = true);
[[nodiscard]] int compareVersions(const std::string& left,
                                  const std::string& right);

}  // namespace issuetrace
