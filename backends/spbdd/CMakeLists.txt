# The SPBDD backend: the same error model as backends/dd, expressed in the
# vocabulary of the SPBDD library (https://github.com/jtsai1120/SPBDD) instead
# of being spelled out over BuDDy.
#
# It exists beside the dd backend rather than in place of it so the two can be
# run against each other: they must agree on every verdict, and what is left
# over is the performance difference. tools/bench_backends.py measures it.

add_library(ftec_spbdd
    src/spbdd_backend.cpp)

target_include_directories(ftec_spbdd PUBLIC include)
target_link_libraries(ftec_spbdd PUBLIC SPBDD::spbdd ftec_core)
target_compile_options(ftec_spbdd PRIVATE -Wall -Wextra)

# Which package SPBDD is sitting on is not cosmetic here: the CUDD-backed
# version hands out its DdManager through Manager::raw(), which is the only
# way to reach CUDD's reordering methods and thresholds, and the BuDDy-backed
# version has no such hatch and offers reordering as one bit. The backend
# compiles against both and says so at run time.
#
# PRIVATE: nothing above this library should have to know what is down there.
if(FTEC_SPBDD_PACKAGE STREQUAL "cudd")
    target_link_libraries(ftec_spbdd PRIVATE CUDD::headers)
    target_compile_definitions(ftec_spbdd PRIVATE FTEC_SPBDD_HAS_CUDD)
endif()
