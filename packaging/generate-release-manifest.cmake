cmake_minimum_required(VERSION 3.24)

foreach(required PACKAGE_ROOT VERSION PLATFORM ARCHITECTURE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(GLOB_RECURSE package_files
    LIST_DIRECTORIES false
    RELATIVE "${PACKAGE_ROOT}"
    "${PACKAGE_ROOT}/*")
list(REMOVE_ITEM package_files "release-manifest.json")
list(SORT package_files)

set(file_entries "")
set(separator "")
foreach(relative_path IN LISTS package_files)
    set(absolute_path "${PACKAGE_ROOT}/${relative_path}")
    if(IS_SYMLINK "${absolute_path}")
        continue()
    endif()
    file(SHA256 "${absolute_path}" file_sha256)
    file(SIZE "${absolute_path}" file_size)
    string(REPLACE "\\" "\\\\" json_path "${relative_path}")
    string(REPLACE "\"" "\\\"" json_path "${json_path}")
    string(APPEND file_entries
        "${separator}    {\"path\": \"${json_path}\", \"size\": ${file_size}, \"sha256\": \"${file_sha256}\"}")
    set(separator ",\n")
endforeach()

string(CONCAT manifest
    "{\n"
    "  \"schemaVersion\": 1,\n"
    "  \"product\": \"IssueTrace\",\n"
    "  \"version\": \"${VERSION}\",\n"
    "  \"platform\": \"${PLATFORM}\",\n"
    "  \"architecture\": \"${ARCHITECTURE}\",\n"
    "  \"files\": [\n"
    "${file_entries}\n"
    "  ]\n"
    "}\n")
file(WRITE "${PACKAGE_ROOT}/release-manifest.json" "${manifest}")
