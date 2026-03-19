# Copyright (c) 2014-2024, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# CodeCoverage.cmake - lcov/genhtml based coverage report generation
#
# Usage:
#   include(cmake/CodeCoverage.cmake)
#   setup_target_for_coverage(
#     NAME coverage
#     EXECUTABLE ctest
#     DEPENDENCIES unit_tests core_tests
#   )
#
# Then run: cmake --build build --target coverage

find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

if(NOT LCOV_PATH)
  message(STATUS "lcov not found - coverage target will not be available")
endif()

if(NOT GENHTML_PATH)
  message(STATUS "genhtml not found - coverage target will not be available")
endif()

# setup_target_for_coverage(
#   NAME <target_name>
#   EXECUTABLE <executable or command>
#   DEPENDENCIES <targets...>
# )
#
# Creates a custom target that:
#   1. Resets coverage counters
#   2. Runs the test executable
#   3. Captures coverage data with lcov
#   4. Filters out external, test, and system paths
#   5. Generates an HTML report with genhtml
function(setup_target_for_coverage)
  cmake_parse_arguments(COVERAGE "" "NAME;EXECUTABLE" "DEPENDENCIES" ${ARGN})

  if(NOT LCOV_PATH)
    message(FATAL_ERROR "lcov is required for coverage target '${COVERAGE_NAME}'")
  endif()

  if(NOT GENHTML_PATH)
    message(FATAL_ERROR "genhtml is required for coverage target '${COVERAGE_NAME}'")
  endif()

  if(NOT COVERAGE_NAME)
    message(FATAL_ERROR "NAME must be specified for setup_target_for_coverage")
  endif()

  if(NOT COVERAGE_EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE must be specified for setup_target_for_coverage")
  endif()

  set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${COVERAGE_NAME}_report")

  add_custom_target(${COVERAGE_NAME}
    # 1. Reset coverage counters
    COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --zerocounters

    # 2. Capture baseline (before tests run)
    COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --capture --initial
            --output-file ${COVERAGE_NAME}.base
            --rc lcov_branch_coverage=1
            --ignore-errors mismatch,gcov

    # 3. Run the test executable
    COMMAND ${COVERAGE_EXECUTABLE} --output-on-failure

    # 4. Capture coverage data after tests
    COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --capture
            --output-file ${COVERAGE_NAME}.info
            --rc lcov_branch_coverage=1
            --ignore-errors mismatch,gcov

    # 5. Combine baseline and test coverage
    COMMAND ${LCOV_PATH}
            -a ${COVERAGE_NAME}.base
            -a ${COVERAGE_NAME}.info
            --output-file ${COVERAGE_NAME}.total
            --rc lcov_branch_coverage=1
            --ignore-errors mismatch,gcov

    # 6. Filter out external, test, and system paths
    COMMAND ${LCOV_PATH}
            --remove ${COVERAGE_NAME}.total
            '*/external/*'
            '*/tests/*'
            '/usr/*'
            '*/boost/*'
            '*/gtest/*'
            --output-file ${COVERAGE_NAME}.filtered
            --rc lcov_branch_coverage=1
            --ignore-errors mismatch,gcov

    # 7. Generate HTML report
    COMMAND ${GENHTML_PATH}
            --output-directory ${COVERAGE_OUTPUT_DIR}
            --title "Monero Code Coverage"
            --legend --show-details
            --branch-coverage
            --ignore-errors mismatch,gcov
            ${COVERAGE_NAME}.filtered

    # 8. Print summary
    COMMAND ${LCOV_PATH} --summary ${COVERAGE_NAME}.filtered
            --rc lcov_branch_coverage=1
            --ignore-errors mismatch,gcov

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${COVERAGE_DEPENDENCIES}
    COMMENT "Generating code coverage report in ${COVERAGE_OUTPUT_DIR}/index.html"
    VERBATIM
  )

  # Clean up intermediate files
  add_custom_command(TARGET ${COVERAGE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove
            ${COVERAGE_NAME}.base
            ${COVERAGE_NAME}.info
            ${COVERAGE_NAME}.total
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning up intermediate coverage files"
  )
endfunction()
