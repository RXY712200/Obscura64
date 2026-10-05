cmake_minimum_required(VERSION 3.16)

project(Obscura64 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(OBSCURA64_DEFAULT_PROVIDER_SYMBOL "" CACHE STRING
    "Optional symbol naming the compile-time default obscura64_provider instance")

add_library(obscura64 STATIC
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
)

target_include_directories(obscura64
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(NOT OBSCURA64_DEFAULT_PROVIDER_SYMBOL STREQUAL "")
    target_compile_definitions(obscura64 PRIVATE
        OBSCURA64_DEFAULT_PROVIDER_SYMBOL=${OBSCURA64_DEFAULT_PROVIDER_SYMBOL}
    )
endif()

if(WIN32)
    target_link_libraries(obscura64 PUBLIC bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64 PRIVATE /W4)
endif()

enable_testing()

add_executable(obscura64_test_smoke
    tests/test_obscura64_smoke.c
)

target_link_libraries(obscura64_test_smoke PRIVATE obscura64)

if(MSVC)
    target_compile_options(obscura64_test_smoke PRIVATE /W4)
endif()

add_test(NAME obscura64_smoke COMMAND obscura64_test_smoke)

add_executable(obscura64_test_profile
    tests/test_obscura64_profile.c
)

target_include_directories(obscura64_test_profile
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obscura64_test_profile PRIVATE obscura64)

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

target_link_libraries(obscura64_test_codec PRIVATE obscura64)

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

target_link_libraries(obscura64_test_profile_generator PRIVATE obscura64)

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

target_link_libraries(obscura64_test_profile_library PRIVATE obscura64)

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

target_link_libraries(obscura64_test_profile_runtime PRIVATE obscura64)

if(MSVC)
    target_compile_options(obscura64_test_profile_runtime PRIVATE /W4)
endif()

add_test(NAME obscura64_profile_runtime COMMAND obscura64_test_profile_runtime)

add_executable(obscura64_test_public_api
    tests/test_obscura64_public_api.c
)

target_link_libraries(obscura64_test_public_api PRIVATE obscura64)

if(MSVC)
    target_compile_options(obscura64_test_public_api PRIVATE /W4)
endif()

add_test(NAME obscura64_public_api COMMAND obscura64_test_public_api)

add_executable(obscura64_test_provider
    tests/test_obscura64_provider.c
)

target_link_libraries(obscura64_test_provider PRIVATE obscura64)

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

target_link_libraries(obscura64_test_state PRIVATE obscura64)

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

target_link_libraries(obscura64_test_project PRIVATE obscura64)

if(WIN32)
    target_link_libraries(obscura64_test_project PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(obscura64_test_project PRIVATE /W4)
endif()

add_test(NAME obscura64_project COMMAND obscura64_test_project)

add_executable(obscura64_test_reinitialize tests/test_obscura64_reinitialize.c)
target_include_directories(obscura64_test_reinitialize PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_reinitialize PRIVATE obscura64)
if(MSVC)
    target_compile_options(obscura64_test_reinitialize PRIVATE /W4)
endif()
add_test(NAME obscura64_reinitialize COMMAND obscura64_test_reinitialize)

add_executable(obscura64_test_persistence tests/test_obscura64_persistence.c)
target_include_directories(obscura64_test_persistence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_persistence PRIVATE obscura64)
if(MSVC)
    target_compile_options(obscura64_test_persistence PRIVATE /W4)
endif()
add_test(NAME obscura64_persistence COMMAND obscura64_test_persistence)

add_executable(obscura64_test_managed_state tests/test_obscura64_managed_state.c)
target_include_directories(obscura64_test_managed_state PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_managed_state PRIVATE obscura64)
if(MSVC)
    target_compile_options(obscura64_test_managed_state PRIVATE /W4)
endif()
add_test(NAME obscura64_managed_state COMMAND obscura64_test_managed_state)

add_executable(obscura64_test_locking tests/test_obscura64_locking.c)
target_include_directories(obscura64_test_locking PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(obscura64_test_locking PRIVATE obscura64)
if(MSVC)
    target_compile_options(obscura64_test_locking PRIVATE /W4)
endif()
add_test(NAME obscura64_locking COMMAND obscura64_test_locking)

# Dedicated instrumented executable: production obscura64 has no hooks or state.
get_target_property(OBSCURA64_FAULT_TEST_SOURCES obscura64 SOURCES)
add_executable(obscura64_test_failure tests/test_obscura64_failure.c ${OBSCURA64_FAULT_TEST_SOURCES})
target_compile_definitions(obscura64_test_failure PRIVATE OBSCURA64_TESTING)
target_include_directories(obscura64_test_failure PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(obscura64_test_failure PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(obscura64_test_failure PRIVATE /W4)
endif()
add_test(NAME obscura64_failure COMMAND obscura64_test_failure)
