cmake_minimum_required(VERSION 3.24)

foreach(required OUTPUT VERSION PLATFORM ARCHITECTURE QT_VERSION TOOLCHAIN)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
if(NOT DEFINED SQLITE_VERSION)
    set(SQLITE_VERSION 3.53.4)
endif()
if(NOT DEFINED ZLIB_VERSION)
    set(ZLIB_VERSION 1.3.2)
endif()
if(TOOLCHAIN MATCHES "LLVM|Clang")
    set(toolchain_license "Apache-2.0 WITH LLVM-exception")
else()
    set(toolchain_license "GPL-3.0-or-later WITH GCC-exception-3.1")
endif()

string(TIMESTAMP generated_at "%Y-%m-%dT%H:%M:%SZ" UTC)
string(UUID serial NAMESPACE 6ba7b810-9dad-11d1-80b4-00c04fd430c8
       NAME "IssueTrace-${VERSION}-${PLATFORM}-${ARCHITECTURE}" TYPE SHA1)
string(CONCAT content
    "{\n"
    "  \"bomFormat\": \"CycloneDX\",\n"
    "  \"specVersion\": \"1.5\",\n"
    "  \"serialNumber\": \"urn:uuid:${serial}\",\n"
    "  \"version\": 1,\n"
    "  \"metadata\": {\"timestamp\": \"${generated_at}\", \"component\": "
    "{\"type\": \"application\", \"name\": \"IssueTrace\", "
    "\"version\": \"${VERSION}\", \"licenses\": [{\"license\": "
    "{\"id\": \"GPL-3.0-or-later\"}}]}},\n"
    "  \"components\": [\n"
    "    {\"type\": \"framework\", \"name\": \"Qt Community\", "
    "\"version\": \"${QT_VERSION}\", \"licenses\": [{\"expression\": "
    "\"LGPL-3.0-only OR GPL-3.0-only\"}]},\n"
    "    {\"type\": \"library\", \"name\": \"SQLite\", \"version\": "
    "\"${SQLITE_VERSION}\", \"licenses\": [{\"license\": {\"name\": \"Public Domain\"}}]},\n"
    "    {\"type\": \"library\", \"name\": \"zlib\", \"version\": "
    "\"${ZLIB_VERSION}\", \"licenses\": [{\"license\": {\"id\": \"Zlib\"}}]},\n"
    "    {\"type\": \"library\", \"name\": \"libxlsxwriter\", \"version\": "
    "\"1.2.4\", \"licenses\": [{\"expression\": "
    "\"BSD-2-Clause AND BSD-3-Clause AND Zlib AND MPL-2.0\"}]},\n"
    "    {\"type\": \"library\", \"name\": \"${TOOLCHAIN} runtime\", "
    "\"version\": \"platform build\", \"licenses\": [{\"expression\": "
    "\"${toolchain_license}\"}]}\n"
    "  ],\n"
    "  \"properties\": [\n"
    "    {\"name\": \"issuetrace:platform\", \"value\": \"${PLATFORM}\"},\n"
    "    {\"name\": \"issuetrace:architecture\", \"value\": \"${ARCHITECTURE}\"}\n"
    "  ]\n"
    "}\n")
file(WRITE "${OUTPUT}" "${content}")
