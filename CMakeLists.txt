cmake_minimum_required(VERSION 3.20)
project(ftec_verifier LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CTest)

# BuDDy first: the decision-diagram backend links against it.
include(BuDDy)

add_library(fpdl_parser
    src/fpdl/parser.cpp
)
target_include_directories(fpdl_parser PUBLIC include)
target_compile_options(fpdl_parser PRIVATE -Wall -Wextra -Wpedantic)

add_executable(fpdlc tools/fpdlc_main.cpp)
target_link_libraries(fpdlc PRIVATE fpdl_parser)

add_library(fpdl_path_graph
    src/fpdl/path_graph.cpp
)
target_include_directories(fpdl_path_graph PUBLIC include)
target_compile_options(fpdl_path_graph PRIVATE -Wall -Wextra -Wpedantic)

add_executable(fpdl-path-graph tools/path_graph_main.cpp)
target_link_libraries(fpdl-path-graph PRIVATE fpdl_path_graph)

add_executable(fpdl-path-dag tools/path_dag_main.cpp)
target_link_libraries(fpdl-path-dag PRIVATE fpdl_path_graph)

add_executable(fpdl-dag-graph tools/dag_graph_main.cpp)
target_link_libraries(fpdl-dag-graph PRIVATE fpdl_path_graph)

# The verifier core: merge the symbolic paths into a trie and walk it,
# handing each circuit to whichever backend was chosen.
add_library(ftec_core
    src/ftec/qasm.cpp
    src/ftec/dag.cpp
    src/ftec/verify.cpp
)
target_include_directories(ftec_core PUBLIC include)
target_link_libraries(ftec_core PUBLIC fpdl_parser)
target_compile_options(ftec_core PRIVATE -Wall -Wextra -Wpedantic)

add_subdirectory(backends/dd)

add_executable(ftec-verify tools/ftec_verify_main.cpp)
# ftec_dd links ftec_core publicly, so naming it here too would be a duplicate.
target_link_libraries(ftec-verify PRIVATE ftec_dd)
target_compile_options(ftec-verify PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(fpdl_parser_tests tests/parser_tests.cpp)
    target_link_libraries(fpdl_parser_tests PRIVATE fpdl_parser)

    add_executable(fpdl_path_graph_tests tests/path_graph_tests.cpp)
    target_link_libraries(fpdl_path_graph_tests PRIVATE fpdl_path_graph)

    file(GLOB FPDL_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/protocols/*/*.fpdl")
    add_test(NAME parser_examples
             COMMAND fpdl_parser_tests ${FPDL_EXAMPLES})
    add_test(NAME path_graph COMMAND fpdl_path_graph_tests)

    file(GLOB QASM_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/protocols/*/*.qasm")
    add_executable(qasm_tests tests/qasm_tests.cpp)
    target_link_libraries(qasm_tests PRIVATE ftec_core)
    add_test(NAME qasm_frontend COMMAND qasm_tests ${QASM_EXAMPLES})

    add_executable(ftec_tests tests/ftec_tests.cpp)
    target_link_libraries(ftec_tests PRIVATE ftec_core)
    add_test(NAME ftec_core COMMAND ftec_tests ${FPDL_EXAMPLES})
endif()
