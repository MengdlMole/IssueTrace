cmake_minimum_required(VERSION 3.24)

foreach(required TEST_ROOT UPDATER MANIFEST_SCRIPT PLATFORM ARCHITECTURE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/IssueTrace-current")

function(make_package DIRECTORY VERSION HEALTHY)
    file(MAKE_DIRECTORY "${DIRECTORY}/bin")
    if(HEALTHY)
        file(WRITE "${DIRECTORY}/bin/IssueTrace"
            "#!/bin/sh\n"
            "if test \"$1\" = \"--update-health-marker\"; then\n"
            "  printf 'healthy\\n' > \"$2\"\n"
            "fi\n"
            "exit 0\n")
    else()
        file(WRITE "${DIRECTORY}/bin/IssueTrace" "#!/bin/sh\nexit 2\n")
    endif()
    file(CHMOD "${DIRECTORY}/bin/IssueTrace"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        -DPACKAGE_ROOT=${DIRECTORY}
        -DVERSION=${VERSION}
        -DPLATFORM=${PLATFORM}
        -DARCHITECTURE=${ARCHITECTURE}
        -P "${MANIFEST_SCRIPT}"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(make_archive VERSION HEALTHY OUTPUT)
    set(name "IssueTrace-${VERSION}-${PLATFORM}-${ARCHITECTURE}")
    set(directory "${TEST_ROOT}/incoming/${name}")
    file(REMOVE_RECURSE "${TEST_ROOT}/incoming")
    make_package("${directory}" "${VERSION}" "${HEALTHY}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT}" --format=zip -- "${name}"
        WORKING_DIRECTORY "${TEST_ROOT}/incoming"
        COMMAND_ERROR_IS_FATAL ANY)
endfunction()

make_package("${TEST_ROOT}/IssueTrace-current" "0.1.0" TRUE)
make_archive("0.2.0" TRUE "${TEST_ROOT}/update-success.zip")
execute_process(COMMAND "${UPDATER}"
    --archive "${TEST_ROOT}/update-success.zip"
    --install-root "${TEST_ROOT}/IssueTrace-current"
    --executable "bin/IssueTrace"
    --wait-pid 99999999
    RESULT_VARIABLE success_result)
if(NOT success_result EQUAL 0)
    message(FATAL_ERROR "Successful update returned ${success_result}")
endif()
file(READ "${TEST_ROOT}/IssueTrace-current/release-manifest.json" current_manifest)
file(READ "${TEST_ROOT}/IssueTrace-current.previous/release-manifest.json" previous_manifest)
if(NOT current_manifest MATCHES "\\\"version\\\": \\\"0.2.0\\\"")
    message(FATAL_ERROR "New version was not installed")
endif()
if(NOT previous_manifest MATCHES "\\\"version\\\": \\\"0.1.0\\\"")
    message(FATAL_ERROR "Previous version was not retained")
endif()

make_archive("0.3.0" FALSE "${TEST_ROOT}/update-failure.zip")
execute_process(COMMAND "${UPDATER}"
    --archive "${TEST_ROOT}/update-failure.zip"
    --install-root "${TEST_ROOT}/IssueTrace-current"
    --executable "bin/IssueTrace"
    --wait-pid 99999999
    RESULT_VARIABLE failure_result)
if(failure_result EQUAL 0)
    message(FATAL_ERROR "Unhealthy update unexpectedly succeeded")
endif()
file(READ "${TEST_ROOT}/IssueTrace-current/release-manifest.json" restored_manifest)
file(READ "${TEST_ROOT}/IssueTrace-current.previous/release-manifest.json"
     restored_previous_manifest)
if(NOT restored_manifest MATCHES "\\\"version\\\": \\\"0.2.0\\\"")
    message(FATAL_ERROR "Failed update did not restore current version")
endif()
if(NOT restored_previous_manifest MATCHES "\\\"version\\\": \\\"0.1.0\\\"")
    message(FATAL_ERROR "Failed update lost the retained previous version")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
