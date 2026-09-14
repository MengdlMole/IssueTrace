#include "issuetrace/update_package.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

void writeText(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << content;
    assert(output.good());
}

void testManifestValidation() {
    const auto root = std::filesystem::temp_directory_path() /
                      "issuetrace-update-package-tests-中文";
    std::filesystem::remove_all(root);
    writeText(root / "IssueTrace", "abc");
    writeText(root / "release-manifest.json",
        "{\n"
        "  \"schemaVersion\": 1,\n"
        "  \"product\": \"IssueTrace\",\n"
        "  \"version\": \"1.2.3\",\n"
        "  \"platform\": \"test\",\n"
        "  \"architecture\": \"test-arch\",\n"
        "  \"files\": [\n"
        "    {\"path\": \"IssueTrace\", \"size\": 3, \"sha256\": "
        "\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\"}\n"
        "  ]\n"
        "}\n");
    const auto manifest = issuetrace::readReleaseManifest(
        root / "release-manifest.json");
    assert(manifest.version == "1.2.3");
    issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch");
    writeText(root / ".DS_Store", "system metadata");
    bool rejectedUnexpectedFile = false;
    try {
        issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch");
    } catch (const std::runtime_error&) {
        rejectedUnexpectedFile = true;
    }
    assert(rejectedUnexpectedFile);
    issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch", false);
    std::filesystem::remove(root / ".DS_Store");
#ifndef _WIN32
    std::filesystem::create_symlink("IssueTrace", root / "IssueTrace-link");
    issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch");
    std::filesystem::remove(root / "IssueTrace-link");
    std::filesystem::create_symlink("../outside", root / "unsafe-link");
    bool rejectedUnsafeLink = false;
    try {
        issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch");
    } catch (const std::runtime_error&) {
        rejectedUnsafeLink = true;
    }
    assert(rejectedUnsafeLink);
    std::filesystem::remove(root / "unsafe-link");
#endif
    assert(issuetrace::compareVersions("1.2.3", "1.2.2") > 0);
    assert(issuetrace::compareVersions("1.2", "1.2.0") == 0);
    writeText(root / "IssueTrace", "tampered");
    bool rejectedTampering = false;
    try {
        issuetrace::validateReleaseDirectory(root, manifest, "test", "test-arch");
    } catch (const std::runtime_error&) {
        rejectedTampering = true;
    }
    assert(rejectedTampering);
    std::filesystem::remove_all(root);
}

}  // namespace

int main() {
    testManifestValidation();
    std::cout << "All IssueTrace update package tests passed.\n";
}
