cmake_minimum_required(VERSION 3.16)

project(Obscura64 VERSION 2.0.0 LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "Obscura64 supports Windows only")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
set(OBSCURA64_VERSION_PRERELEASE "")
option(OBSCURA64_BUILD_TOOLS "Build command-line tools" ON)
option(OBSCURA64_INSTALL_TOOLS "Install command-line tools" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(OBSCURA64_DEFAULT_PROVIDER_SYMBOL "" CACHE STRING
    "Optional symbol naming the compile-time default obscura64_provider instance")

add_library(obscura64 STATIC
    src/obscura64_path.c
    src/obscura64_file_io.c
    src/obscura64_v2_file.c
    src/obscura64_v2_envelope.c
    src/obscura64_v2_dispatch.c
    src/obscura64_v2_casual.c
    src/obscura64_v2_current_user.c
    src/obscura64_v2_current_user_record.c
    src/obscura64_v2_legacy.c
    src/obscura64_sha256.c
    src/obscura64_core.c
    src/obscura64_codec.c
    src/obscura64_api.c
    src/obscura64_context.c
    src/obscura64_provider.c
    src/obscura64_profiles_v1.c
    src/obscura64_profile_runtime.c
    src/obscura64_state.c
    src/obscura64_project.c
    src/obscura64_history.c
    src/obscura64_persistence.c
    src/obscura64_managed_state.c
    src/obscura64_lock.c
    src/obscura64_redundancy.c
    src/obscura64_recovery.c
    src/obscura64_managed_payload.c
    src/obscura64_disaster.c
)
add_library(Obscura64::obscura64 ALIAS obscura64)

target_include_directories(obscura64
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(NOT OBSCURA64_DEFAULT_PROVIDER_SYMBOL STREQUAL "")
    target_compile_definitions(obscura64 PRIVATE
        OBSCURA64_DEFAULT_PROVIDER_SYMBOL=${OBSCURA64_DEFAULT_PROVIDER_SYMBOL}
    )
endif()

if(WIN32)
    target_link_libraries(obscura64 PUBLIC bcrypt crypt32 shell32 ole32 uuid)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    # clang-cl /W4 maps to Clang -Wall -Wextra; /Wall maps to -Weverything.
    target_compile_options(obscura64 PRIVATE /W4)
elseif(MSVC)
    target_compile_options(obscura64 PRIVATE /W4)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(obscura64 PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(OBSCURA64_BUILD_TOOLS)
    add_executable(obscura64_cli tools/obscura64.c)
    target_include_directories(obscura64_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(obscura64_cli PRIVATE obscura64)
    set_target_properties(obscura64_cli PROPERTIES OUTPUT_NAME obscura64)
    if(MINGW)
        target_link_options(obscura64_cli PRIVATE -municode)
    endif()
    add_executable(obscura64_recover tools/obscura64_recover.c)
    target_include_directories(obscura64_recover PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(obscura64_recover PRIVATE obscura64)
    if(MINGW)
        target_link_options(obscura64_recover PRIVATE -municode)
    endif()
    if(MSVC)
        target_compile_options(obscura64_cli PRIVATE /W4)
        target_compile_options(obscura64_recover PRIVATE /W4)
    endif()
endif()

if(BUILD_TESTING)

# Tests never access the developer's real LocalAppData. Without an explicit
# temporary test root the instrumented mirror is unavailable, not production.
get_target_property(OBSCURA64_TEST_SOURCES obscura64 SOURCES)
add_library(obscura64_test_support STATIC ${OBSCURA64_TEST_SOURCES})
target_compile_definitions(obscura64_test_support PRIVATE OBSCURA64_REDUNDANCY_TESTING)
target_include_directories(obscura64_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT OBSCURA64_DEFAULT_PROVIDER_SYMBOL STREQUAL "")
    target_compile_definitions(obscura64_test_support PRIVATE
        OBSCURA64_DEFAULT_PROVIDER_SYMBOL=${OBSCURA64_DEFAULT_PROVIDER_SYMBOL})
endif()
if(WIN32)
    target_link_libraries(obscura64_test_support PUBLIC bcrypt crypt32 shell32 ole32 uuid)
endif()
if(MSVC)
    target_compile_options(obscura64_test_support PRIVATE /W4)
endif()

add_executable(obscura64_test_smoke
    tests/test_obscura64_smoke.c
)

target_link_libraries(obscura64_test_smoke PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_smoke PRIVATE /W4)
endif()

add_test(NAME obscura64_smoke COMMAND obscura64_test_smoke)

add_executable(obscura64_test_v2_file tests/test_obscura64_v2_file.c)
target_include_directories(obscura64_test_v2_file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_file PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_file PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_file COMMAND obscura64_test_v2_file)

add_executable(obscura64_test_v2_path tests/test_obscura64_v2_path.c)
target_link_libraries(obscura64_test_v2_path PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_path PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_path COMMAND obscura64_test_v2_path)

add_executable(obscura64_test_v2_mutation tests/test_obscura64_v2_mutation.c)
target_include_directories(obscura64_test_v2_mutation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_mutation PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_mutation PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_mutation COMMAND obscura64_test_v2_mutation)

add_executable(obscura64_test_v2_threading tests/test_obscura64_v2_threading.c)
target_link_libraries(obscura64_test_v2_threading PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_threading PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_threading COMMAND obscura64_test_v2_threading)

add_executable(obscura64_test_v2_upgrade tests/test_obscura64_v2_upgrade.c)
target_include_directories(obscura64_test_v2_upgrade PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_upgrade PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_upgrade PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_upgrade COMMAND obscura64_test_v2_upgrade)

add_executable(obscura64_test_v2_cli tests/test_obscura64_v2_cli.c)
target_include_directories(obscura64_test_v2_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_cli PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_cli PRIVATE /W4)
endif()
if(OBSCURA64_BUILD_TOOLS)
    add_test(NAME obscura64_v2_cli COMMAND obscura64_test_v2_cli $<TARGET_FILE:obscura64_cli>)
endif()

add_executable(obscura64_test_v2_file_policy tests/test_obscura64_v2_file_policy.c)
target_include_directories(obscura64_test_v2_file_policy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_file_policy PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_file_policy PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_file_policy COMMAND obscura64_test_v2_file_policy)

add_executable(obscura64_test_v2_file_locking tests/test_obscura64_v2_file_locking.c)
target_include_directories(obscura64_test_v2_file_locking PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_v2_file_locking PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_file_locking PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_file_locking COMMAND obscura64_test_v2_file_locking)

add_executable(obscura64_test_v2_envelope tests/test_obscura64_v2_envelope.c)
target_link_libraries(obscura64_test_v2_envelope PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_v2_envelope PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_envelope COMMAND obscura64_test_v2_envelope)

foreach(v2_test protection legacy current_user_record)
    add_executable(obscura64_test_v2_${v2_test}
        tests/test_obscura64_v2_${v2_test}.c)
    target_include_directories(obscura64_test_v2_${v2_test} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(obscura64_test_v2_${v2_test} PRIVATE obscura64_test_support)
    if(MSVC)
        target_compile_options(obscura64_test_v2_${v2_test} PRIVATE /W4)
    endif()
    add_test(NAME obscura64_v2_${v2_test} COMMAND obscura64_test_v2_${v2_test})
endforeach()

add_executable(obscura64_test_profile
    tests/test_obscura64_profile.c
)

target_include_directories(obscura64_test_profile
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_profile PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_profile PRIVATE /W4)
endif()

add_test(NAME obscura64_profile COMMAND obscura64_test_profile)

add_executable(obscura64_test_codec
    tests/test_obscura64_codec.c
)

target_include_directories(obscura64_test_codec
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_codec PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_codec PRIVATE /W4)
endif()

add_test(NAME obscura64_codec COMMAND obscura64_test_codec)

add_executable(obscura64_profile_generator
    tools/obscura64_profile_generator.c
)

target_include_directories(obscura64_profile_generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(obscura64_profile_generator PRIVATE obscura64)

if(WIN32)
    target_link_libraries(obscura64_profile_generator PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_profile_generator PRIVATE /W4)
endif()

add_executable(obscura64_test_profile_generator
    tests/test_obscura64_profile_generator.c
    tools/obscura64_profile_generator.c
)

target_compile_definitions(obscura64_test_profile_generator
    PRIVATE OBSCURA64_PROFILE_GENERATOR_NO_MAIN
)

target_include_directories(obscura64_test_profile_generator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_link_libraries(obscura64_test_profile_generator PRIVATE obscura64_test_support)

if(WIN32)
    target_link_libraries(obscura64_test_profile_generator PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_test_profile_generator PRIVATE /W4)
endif()

add_test(NAME obscura64_profile_generator COMMAND obscura64_test_profile_generator)

add_executable(obscura64_test_profile_library
    tests/test_obscura64_profile_library.c
)

target_include_directories(obscura64_test_profile_library
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_profile_library PRIVATE obscura64_test_support)

if(WIN32)
    target_link_libraries(obscura64_test_profile_library PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_test_profile_library PRIVATE /W4)
endif()

add_test(NAME obscura64_profile_library COMMAND obscura64_test_profile_library)
set_tests_properties(obscura64_profile_library PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(obscura64_test_profile_runtime
    tests/test_obscura64_profile_runtime.c
)

target_include_directories(obscura64_test_profile_runtime
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_profile_runtime PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_profile_runtime PRIVATE /W4)
endif()

add_test(NAME obscura64_profile_runtime COMMAND obscura64_test_profile_runtime)

add_executable(obscura64_test_public_api
    tests/test_obscura64_public_api.c
)

target_link_libraries(obscura64_test_public_api PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_public_api PRIVATE /W4)
endif()

add_test(NAME obscura64_public_api COMMAND obscura64_test_public_api)

add_executable(obscura64_test_provider
    tests/test_obscura64_provider.c
)

target_link_libraries(obscura64_test_provider PRIVATE obscura64_test_support)

if(MSVC)
    target_compile_options(obscura64_test_provider PRIVATE /W4)
endif()

add_test(NAME obscura64_provider COMMAND obscura64_test_provider)

add_executable(obscura64_test_state
    tests/test_obscura64_state.c
)

target_include_directories(obscura64_test_state
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_state PRIVATE obscura64_test_support)

if(WIN32)
    target_link_libraries(obscura64_test_state PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_test_state PRIVATE /W4)
endif()

add_test(NAME obscura64_state COMMAND obscura64_test_state)

add_executable(obscura64_test_project
    tests/test_obscura64_project.c
)

target_include_directories(obscura64_test_project
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_project PRIVATE obscura64_test_support)

if(WIN32)
    target_link_libraries(obscura64_test_project PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_test_project PRIVATE /W4)
endif()

add_test(NAME obscura64_project COMMAND obscura64_test_project)

add_executable(obscura64_test_reinitialize tests/test_obscura64_reinitialize.c)
target_include_directories(obscura64_test_reinitialize PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_reinitialize PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_reinitialize PRIVATE /W4)
endif()
add_test(NAME obscura64_reinitialize COMMAND obscura64_test_reinitialize)

add_executable(obscura64_test_persistence tests/test_obscura64_persistence.c)
target_include_directories(obscura64_test_persistence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_persistence PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_persistence PRIVATE /W4)
endif()
add_test(NAME obscura64_persistence COMMAND obscura64_test_persistence)

add_executable(obscura64_test_managed_state tests/test_obscura64_managed_state.c)
target_include_directories(obscura64_test_managed_state PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_managed_state PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_managed_state PRIVATE /W4)
endif()
add_test(NAME obscura64_managed_state COMMAND obscura64_test_managed_state)

add_executable(obscura64_test_locking tests/test_obscura64_locking.c)
target_include_directories(obscura64_test_locking PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_locking PRIVATE obscura64_test_support)
if(MSVC)
    target_compile_options(obscura64_test_locking PRIVATE /W4)
endif()
add_test(NAME obscura64_locking COMMAND obscura64_test_locking)

# Dedicated instrumented executable: production obscura64 has no hooks or state.
get_target_property(OBSCURA64_FAULT_TEST_SOURCES obscura64 SOURCES)
add_executable(obscura64_test_failure tests/test_obscura64_failure.c ${OBSCURA64_FAULT_TEST_SOURCES})
target_compile_definitions(obscura64_test_failure PRIVATE OBSCURA64_TESTING OBSCURA64_REDUNDANCY_TESTING)
if(NOT OBSCURA64_DEFAULT_PROVIDER_SYMBOL STREQUAL "")
    target_compile_definitions(obscura64_test_failure PRIVATE
        OBSCURA64_DEFAULT_PROVIDER_SYMBOL=${OBSCURA64_DEFAULT_PROVIDER_SYMBOL})
endif()
target_include_directories(obscura64_test_failure PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(obscura64_test_failure PRIVATE bcrypt crypt32 shell32 ole32 uuid)
endif()
if(MSVC)
    target_compile_options(obscura64_test_failure PRIVATE /W4)
endif()
add_test(NAME obscura64_failure COMMAND obscura64_test_failure)

add_executable(obscura64_test_v2_file_failure
    tests/test_obscura64_v2_file_failure.c ${OBSCURA64_FAULT_TEST_SOURCES})
target_compile_definitions(obscura64_test_v2_file_failure PRIVATE
    OBSCURA64_TESTING OBSCURA64_REDUNDANCY_TESTING)
target_include_directories(obscura64_test_v2_file_failure PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(obscura64_test_v2_file_failure PRIVATE bcrypt crypt32 shell32 ole32 uuid)
endif()
if(MSVC)
    target_compile_options(obscura64_test_v2_file_failure PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_file_failure COMMAND obscura64_test_v2_file_failure)

add_executable(obscura64_test_v2_upgrade_failure
    tests/test_obscura64_v2_upgrade_failure.c ${OBSCURA64_FAULT_TEST_SOURCES})
target_compile_definitions(obscura64_test_v2_upgrade_failure PRIVATE
    OBSCURA64_TESTING OBSCURA64_REDUNDANCY_TESTING)
target_include_directories(obscura64_test_v2_upgrade_failure PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(obscura64_test_v2_upgrade_failure PRIVATE
        bcrypt crypt32 shell32 ole32 uuid)
endif()
if(MSVC)
    target_compile_options(obscura64_test_v2_upgrade_failure PRIVATE /W4)
endif()
add_test(NAME obscura64_v2_upgrade_failure COMMAND obscura64_test_v2_upgrade_failure)

foreach(stage6_test redundancy recovery)
    add_executable(obscura64_test_${stage6_test}
        tests/test_obscura64_${stage6_test}.c ${OBSCURA64_TEST_SOURCES})
    target_compile_definitions(obscura64_test_${stage6_test} PRIVATE
        OBSCURA64_TESTING OBSCURA64_REDUNDANCY_TESTING)
    if(NOT OBSCURA64_DEFAULT_PROVIDER_SYMBOL STREQUAL "")
        target_compile_definitions(obscura64_test_${stage6_test} PRIVATE
            OBSCURA64_DEFAULT_PROVIDER_SYMBOL=${OBSCURA64_DEFAULT_PROVIDER_SYMBOL})
    endif()
    target_include_directories(obscura64_test_${stage6_test} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(WIN32)
        target_link_libraries(obscura64_test_${stage6_test} PRIVATE bcrypt crypt32 shell32 ole32 uuid)
    endif()
    if(MSVC)
        target_compile_options(obscura64_test_${stage6_test} PRIVATE /W4)
    endif()
    add_test(NAME obscura64_${stage6_test} COMMAND obscura64_test_${stage6_test})
endforeach()

foreach(stage7_test managed_payload recover_tool)
    add_executable(obscura64_test_${stage7_test} tests/test_obscura64_${stage7_test}.c)
    target_include_directories(obscura64_test_${stage7_test} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(obscura64_test_${stage7_test} PRIVATE obscura64_test_support)
    if(MSVC)
        target_compile_options(obscura64_test_${stage7_test} PRIVATE /W4)
    endif()
endforeach()
add_test(NAME obscura64_managed_payload COMMAND obscura64_test_managed_payload)
if(OBSCURA64_BUILD_TOOLS)
    add_test(NAME obscura64_recover_tool COMMAND obscura64_test_recover_tool $<TARGET_FILE:obscura64_recover>)
endif()

add_executable(obscura64_test_disaster tests/test_obscura64_disaster.c ${OBSCURA64_TEST_SOURCES})
target_include_directories(obscura64_test_disaster PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(obscura64_test_disaster PRIVATE OBSCURA64_DISASTER_TESTING OBSCURA64_REDUNDANCY_TESTING)
add_executable(obscura64_test_managed_default tests/test_obscura64_managed_default.c ${OBSCURA64_TEST_SOURCES})
target_include_directories(obscura64_test_managed_default PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(obscura64_test_managed_default PRIVATE OBSCURA64_REDUNDANCY_TESTING
    OBSCURA64_DEFAULT_PROVIDER_SYMBOL=obscura64_test_managed_default_provider)
foreach(target obscura64_test_disaster obscura64_test_managed_default)
    if(WIN32)
        target_link_libraries(${target} PRIVATE bcrypt crypt32 shell32 ole32 uuid)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endforeach()
add_test(NAME obscura64_disaster COMMAND obscura64_test_disaster)
add_test(NAME obscura64_managed_default COMMAND obscura64_test_managed_default)

endif() # BUILD_TESTING

option(OBSCURA64_BUILD_EXAMPLES "Build public API examples" OFF)
if(OBSCURA64_BUILD_EXAMPLES)
    foreach(example managed_quickstart raw_codec)
        add_executable(obscura64_${example} examples/${example}.c)
        target_link_libraries(obscura64_${example} PRIVATE obscura64)
        if(MSVC)
            target_compile_options(obscura64_${example} PRIVATE /W4)
        endif()
    endforeach()
endif()

install(TARGETS obscura64 EXPORT Obscura64Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/obscura64.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(OBSCURA64_BUILD_TOOLS AND OBSCURA64_INSTALL_TOOLS)
    install(TARGETS obscura64_cli obscura64_recover
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
set(OBSCURA64_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Obscura64")
configure_package_config_file(cmake/Obscura64Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/Obscura64Config.cmake"
    INSTALL_DESTINATION "${OBSCURA64_PACKAGE_DIR}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/Obscura64ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(EXPORT Obscura64Targets NAMESPACE Obscura64::
    DESTINATION "${OBSCURA64_PACKAGE_DIR}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Obscura64Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/Obscura64ConfigVersion.cmake"
    DESTINATION "${OBSCURA64_PACKAGE_DIR}")

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
        set(OBSCURA64_WARNING_FLAGS /W4)
    else()
        set(OBSCURA64_WARNING_FLAGS -Wall -Wextra -Wpedantic)
    endif()
    get_property(OBSCURA64_LOCAL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(local_target IN LISTS OBSCURA64_LOCAL_TARGETS)
        get_target_property(local_type ${local_target} TYPE)
        if((local_type STREQUAL "STATIC_LIBRARY" OR
            local_type STREQUAL "EXECUTABLE") AND
            NOT local_target STREQUAL "obscura64")
            target_compile_options(${local_target} PRIVATE ${OBSCURA64_WARNING_FLAGS})
        endif()
    endforeach()
endif()
