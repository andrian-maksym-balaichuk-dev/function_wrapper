cmake_minimum_required(VERSION 3.20)

project(
    fw
    VERSION 0.1.0
    DESCRIPTION "Header-only callable wrapper with multi-signature dispatch."
    LANGUAGES CXX
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(CMakeDependentOption)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Project-wide language defaults. Consumers still receive the requirement
# through the exported fw::wrapper target as well.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Public project switches.
option(BUILD_TESTING "Enable testing for this project." ${PROJECT_IS_TOP_LEVEL})
option(FW_BUILD_TESTS "Build the fw test suite." ${PROJECT_IS_TOP_LEVEL})
cmake_dependent_option(
    FW_ENABLE_COVERAGE
    "Enable LLVM coverage instrumentation for the fw test suite."
    OFF
    "FW_BUILD_TESTS;BUILD_TESTING"
    OFF
)

# Canonical project paths used by packaging and tests.
set(FW_PACKAGE_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fw")
set(FW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(FW_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
set(FW_PACKAGE_CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/fwConfig.cmake.in")

# Header-only public target.
add_library(fw_wrapper INTERFACE)
add_library(fw::wrapper ALIAS fw_wrapper)
set_target_properties(
    fw_wrapper
    PROPERTIES
        EXPORT_NAME wrapper
        FOLDER "fw"
)

target_include_directories(fw_wrapper
    INTERFACE
        $<BUILD_INTERFACE:${FW_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(fw_wrapper INTERFACE cxx_std_23)
set_target_properties(fw_wrapper PROPERTIES CXX_EXTENSIONS OFF)

# Install headers and exported target metadata.
install(
    TARGETS fw_wrapper
    EXPORT fwTargets
)

install(
    DIRECTORY ${FW_INCLUDE_DIR}/fw
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/fwConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${FW_PACKAGE_CONFIG_TEMPLATE}
    ${CMAKE_CURRENT_BINARY_DIR}/fwConfig.cmake
    INSTALL_DESTINATION ${FW_PACKAGE_CMAKE_DIR}
)

install(
    EXPORT fwTargets
    FILE fwTargets.cmake
    NAMESPACE fw::
    DESTINATION ${FW_PACKAGE_CMAKE_DIR}
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fwConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/fwConfigVersion.cmake
    DESTINATION ${FW_PACKAGE_CMAKE_DIR}
)

# Local validation suite. This is not part of the installed package surface.
if(FW_BUILD_TESTS AND BUILD_TESTING)
    enable_testing()

    # Keep both focused test binaries and one aggregate binary for IDE use.
    set(FW_TEST_SOURCE_FILES
        concepts_test.cpp
        exceptions_test.cpp
        function_ref_test.cpp
        function_wrapper_test.cpp
        move_only_function_wrapper_test.cpp
        signature_interface_test.cpp
        vtable_test.cpp
    )
    set(FW_TEST_TARGETS)
    set(FW_COVERAGE_HEADERS
        ${FW_INCLUDE_DIR}/fw/function_ref.hpp
        ${FW_INCLUDE_DIR}/fw/function_wrapper.hpp
        ${FW_INCLUDE_DIR}/fw/move_only_function_wrapper.hpp
        ${FW_INCLUDE_DIR}/fw/call_result.hpp
        ${FW_INCLUDE_DIR}/fw/exceptions.hpp
        ${FW_INCLUDE_DIR}/fw/detail/concepts.hpp
        ${FW_INCLUDE_DIR}/fw/detail/signature_interface.hpp
        ${FW_INCLUDE_DIR}/fw/detail/vtable.hpp
    )

    include(GoogleTest)

    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        message(STATUS "GTest was not found. Fetching GoogleTest v1.15.2 for the fw test suite.")
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    function(fw_add_test_executable target_name source_name)
        add_executable(${target_name} ${FW_TEST_DIR}/${source_name})
        target_link_libraries(${target_name} PRIVATE fw::wrapper GTest::gtest_main)
        target_compile_features(${target_name} PRIVATE cxx_std_23)
        set_target_properties(
            ${target_name}
            PROPERTIES
                CXX_EXTENSIONS OFF
                FOLDER "tests"
        )

        gtest_discover_tests(
            ${target_name}
            TEST_PREFIX "fw."
            DISCOVERY_TIMEOUT 60
        )

        list(APPEND FW_TEST_TARGETS ${target_name})
        set(FW_TEST_TARGETS "${FW_TEST_TARGETS}" PARENT_SCOPE)
    endfunction()

    set(FW_ALL_TEST_SOURCE_PATHS)
    foreach(fw_test_source IN LISTS FW_TEST_SOURCE_FILES)
        list(APPEND FW_ALL_TEST_SOURCE_PATHS ${FW_TEST_DIR}/${fw_test_source})
    endforeach()

    add_executable(fw_all_tests ${FW_ALL_TEST_SOURCE_PATHS})
    target_link_libraries(fw_all_tests PRIVATE fw::wrapper GTest::gtest_main)
    target_compile_features(fw_all_tests PRIVATE cxx_std_23)
    set_target_properties(
        fw_all_tests
        PROPERTIES
            CXX_EXTENSIONS OFF
            FOLDER "tests"
    )

    fw_add_test_executable(fw_concepts_tests concepts_test.cpp)
    fw_add_test_executable(fw_exceptions_tests exceptions_test.cpp)
    fw_add_test_executable(fw_function_ref_tests function_ref_test.cpp)
    fw_add_test_executable(fw_function_wrapper_tests function_wrapper_test.cpp)
    fw_add_test_executable(fw_move_only_function_wrapper_tests move_only_function_wrapper_test.cpp)
    fw_add_test_executable(fw_signature_interface_tests signature_interface_test.cpp)
    fw_add_test_executable(fw_vtable_tests vtable_test.cpp)

    add_custom_target(fw_check DEPENDS ${FW_TEST_TARGETS})
    set_target_properties(fw_check PROPERTIES FOLDER "tests")

    if(FW_ENABLE_COVERAGE)
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "FW_ENABLE_COVERAGE currently requires Clang/LLVM tools.")
        endif()

        find_program(FW_LLVM_PROFDATA llvm-profdata REQUIRED)
        find_program(FW_LLVM_COV llvm-cov REQUIRED)

        foreach(fw_test_target IN LISTS FW_TEST_TARGETS)
            target_compile_options(${fw_test_target} PRIVATE
                -fprofile-instr-generate
                -fcoverage-mapping
                -O0
                -g
            )
            target_link_options(${fw_test_target} PRIVATE
                -fprofile-instr-generate
                -fcoverage-mapping
            )
        endforeach()

        set(FW_PRIMARY_COVERAGE_TARGET fw_function_wrapper_tests)
        set(FW_COVERAGE_OBJECT_ARGS)
        foreach(fw_test_target IN LISTS FW_TEST_TARGETS)
            if(NOT fw_test_target STREQUAL FW_PRIMARY_COVERAGE_TARGET)
                list(APPEND FW_COVERAGE_OBJECT_ARGS -object $<TARGET_FILE:${fw_test_target}>)
            endif()
        endforeach()

        add_custom_target(fw_wrapper_coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND ${CMAKE_COMMAND} -E env
                    LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage/%p.profraw
                    ${CMAKE_CTEST_COMMAND}
                    --output-on-failure
                    --test-dir ${CMAKE_BINARY_DIR}
            COMMAND ${FW_LLVM_PROFDATA} merge
                    -sparse
                    ${CMAKE_BINARY_DIR}/coverage/*.profraw
                    -o ${CMAKE_BINARY_DIR}/coverage/fw_wrapper_tests.profdata
            COMMAND ${FW_LLVM_COV} report
                    $<TARGET_FILE:${FW_PRIMARY_COVERAGE_TARGET}>
                    ${FW_COVERAGE_OBJECT_ARGS}
                    -instr-profile=${CMAKE_BINARY_DIR}/coverage/fw_wrapper_tests.profdata
                    ${FW_COVERAGE_HEADERS}
            DEPENDS ${FW_TEST_TARGETS}
            COMMENT "Run wrapper tests and print LLVM coverage for the library headers."
        )
    endif()
endif()
