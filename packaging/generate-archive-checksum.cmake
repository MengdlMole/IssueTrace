cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED ARTIFACT OR "${ARTIFACT}" STREQUAL "")
    message(FATAL_ERROR "ARTIFACT is required")
endif()
if(NOT EXISTS "${ARTIFACT}")
    message(FATAL_ERROR "Artifact does not exist: ${ARTIFACT}")
endif()

file(SHA256 "${ARTIFACT}" artifact_sha256)
get_filename_component(artifact_name "${ARTIFACT}" NAME)
file(WRITE "${ARTIFACT}.sha256" "${artifact_sha256}  ${artifact_name}\n")
message(STATUS "${artifact_sha256}  ${artifact_name}")

