#
# experimental/tiledb/common/ports/CMakeLists.txt
#
#
# The MIT License
#
# Copyright (c) 2022 TileDB, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

include(common NO_POLICY_SCOPE)

list(APPEND PORT_SOURCES
    ports.cc
)
gather_sources(${PORT_SOURCES})

#
# Object library for other units to depend upon
#
add_library(ports OBJECT ${PORT_SOURCES})
target_link_libraries(ports PUBLIC baseline $<TARGET_OBJECTS:baseline>)

#
# Test-compile of object library ensures link-completeness
#
add_executable(compile_ports EXCLUDE_FROM_ALL)
target_link_libraries(compile_ports PRIVATE ports)
target_sources(compile_ports PRIVATE test/compile_ports_main.cc)

if (TILEDB_TESTS)
    add_executable(unit_ports EXCLUDE_FROM_ALL)
    target_link_libraries(unit_ports PUBLIC ports)
    find_package(Catch_EP REQUIRED)
    target_link_libraries(unit_ports PUBLIC Catch2::Catch2)
    target_link_libraries(unit_ports PUBLIC $<TARGET_OBJECTS:thread_pool>)

    # Sources for code elsewhere required for tests
    target_sources(unit_ports PUBLIC ${DEPENDENT_SOURCES})

    # Sources for tests
    target_sources(unit_ports PUBLIC
            test/main.cc
            test/unit_ports.cc
            )

    add_test(
            NAME "unit_ports"
            COMMAND $<TARGET_FILE:unit_ports> --durations=yes
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

endif()


list(APPEND FSM_SOURCES
    fsm.cc
)
gather_sources(${FSM_SOURCES})


#
# Object library for other units to depend upon
#
add_library(fsm OBJECT ${FSM_SOURCES})
target_link_libraries(fsm PUBLIC baseline $<TARGET_OBJECTS:baseline>)

#
# Test-compile of object library ensures link-completeness
#
add_executable(compile_fsm EXCLUDE_FROM_ALL)
target_link_libraries(compile_fsm PRIVATE fsm)
target_sources(compile_fsm PRIVATE test/compile_fsm_main.cc)

if (TILEDB_TESTS)
    add_executable(unit_fsm EXCLUDE_FROM_ALL)
    target_link_libraries(unit_fsm PUBLIC fsm)
    find_package(Catch_EP REQUIRED)
    target_link_libraries(unit_fsm PUBLIC Catch2::Catch2)
    target_link_libraries(unit_fsm PUBLIC $<TARGET_OBJECTS:thread_pool>)

    # Sources for code elsewhere required for tests
    target_sources(unit_fsm PUBLIC ${DEPENDENT_SOURCES})

    # Sources for tests
    target_sources(unit_fsm PUBLIC
            test/main.cc
            test/unit_fsm.cc
            )

    add_test(
            NAME "unit_fsm"
            COMMAND $<TARGET_FILE:unit_fsm> --durations=yes
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
