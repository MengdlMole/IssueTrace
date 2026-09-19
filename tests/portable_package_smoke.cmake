cmake_minimum_required(VERSION 3.24)

foreach(required TEST_ROOT ARCHIVE PACKAGE_NAME VERSION)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/current")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${ARCHIVE}"
    WORKING_DIRECTORY "${TEST_ROOT}/current" COMMAND_ERROR_IS_FATAL ANY)
set(install_root "${TEST_ROOT}/current/${PACKAGE_NAME}")
set(manifest "${install_root}/release-manifest.json")
file(READ "${manifest}" content)
string(REPLACE "\"version\": \"${VERSION}\"" "\"version\": \"0.1.0\""
       content "${content}")
file(WRITE "${manifest}" "${content}")
file(COPY_FILE "${install_root}/bin/IssueTraceUpdater"
     "${TEST_ROOT}/IssueTraceUpdater" ONLY_IF_DIFFERENT)
file(CHMOD "${TEST_ROOT}/IssueTraceUpdater"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE)
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/workspace
    ISSUETRACE_EXIT_AFTER_HEALTH=1
    "${TEST_ROOT}/IssueTraceUpdater"
    --archive "${ARCHIVE}"
    --install-root "${install_root}"
    --executable "IssueTrace.app/Contents/MacOS/IssueTrace"
    --wait-pid 99999999
    RESULT_VARIABLE update_result)
if(NOT update_result EQUAL 0)
    message(FATAL_ERROR "Portable package update smoke test returned ${update_result}")
endif()
file(READ "${install_root}/release-manifest.json" updated_manifest)
file(READ "${install_root}.previous/release-manifest.json" previous_manifest)
if(NOT updated_manifest MATCHES "\\\"version\\\": \\\"${VERSION}\\\"")
    message(FATAL_ERROR "Portable package did not install version ${VERSION}")
endif()
if(NOT previous_manifest MATCHES "\\\"version\\\": \\\"0.1.0\\\"")
    message(FATAL_ERROR "Portable package did not retain version 0.1.0")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/scroll-workspace
    "${install_root}/IssueTrace.app/Contents/MacOS/IssueTrace"
    --verify-scroll-layout
    RESULT_VARIABLE scroll_result)
if(NOT scroll_result EQUAL 0)
    message(FATAL_ERROR "Packaged scroll layout check returned ${scroll_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/status-workspace
    "${install_root}/IssueTrace.app/Contents/MacOS/IssueTrace"
    --verify-status-refresh
    RESULT_VARIABLE status_result)
if(NOT status_result EQUAL 0)
    message(FATAL_ERROR "Packaged status refresh check returned ${status_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/custom-reminder-workspace
    "${install_root}/IssueTrace.app/Contents/MacOS/IssueTrace"
    --verify-custom-reminder
    RESULT_VARIABLE reminder_result)
if(NOT reminder_result EQUAL 0)
    message(FATAL_ERROR "Packaged custom reminder check returned ${reminder_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/metadata-save-workspace
    "${install_root}/IssueTrace.app/Contents/MacOS/IssueTrace"
    --verify-explicit-metadata-save
    RESULT_VARIABLE metadata_save_result)
if(NOT metadata_save_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged explicit metadata save check returned ${metadata_save_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    QT_QPA_PLATFORM=offscreen
    ISSUETRACE_WORKSPACE=${TEST_ROOT}/lifecycle-calendar-workspace
    "${install_root}/IssueTrace.app/Contents/MacOS/IssueTrace"
    --verify-lifecycle-calendar
    RESULT_VARIABLE lifecycle_calendar_result)
if(NOT lifecycle_calendar_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged lifecycle/calendar check returned ${lifecycle_calendar_result}")
endif()
file(REMOVE_RECURSE "${TEST_ROOT}")
