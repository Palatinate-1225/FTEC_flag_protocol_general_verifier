# Build SPBDD, and the CUDD it sits on, as ordinary CMake targets.
#
# This mirrors cmake/BuDDy.cmake: fetch the sources, define the targets here,
# and keep the first configure free of anything but a compiler. The difference
# is CUDD, which unlike BuDDy really does need its autotools build -- config.h
# carries a few dozen probed macros rather than three version numbers, so
# generating it by hand is not the small job it was there. cmake/build_cudd.sh
# runs that build and explains the one non-obvious step in it.
#
# Set FTEC_SPBDD_SOURCE_DIR / FTEC_CUDD_SOURCE_DIR to build from checkouts you
# already have instead of fetching them (useful offline, or when iterating on
# SPBDD itself).

set(FTEC_SPBDD_SOURCE_DIR "" CACHE PATH
    "Existing SPBDD checkout to build from; fetched automatically when empty")
set(FTEC_CUDD_SOURCE_DIR "" CACHE PATH
    "Existing CUDD checkout to build from; fetched automatically when empty")

if(NOT UNIX)
    message(FATAL_ERROR
        "The spbdd backend needs CUDD's autotools build, which wants a POSIX "
        "shell. Configure with -DFTEC_ENABLE_SPBDD=OFF on this platform.")
endif()

include(FetchContent)

# --- sources ---------------------------------------------------------------

if(FTEC_SPBDD_SOURCE_DIR)
    if(NOT EXISTS "${FTEC_SPBDD_SOURCE_DIR}/include/spbdd/spbdd.hpp")
        message(FATAL_ERROR
            "FTEC_SPBDD_SOURCE_DIR=${FTEC_SPBDD_SOURCE_DIR} does not look like an "
            "SPBDD checkout (no include/spbdd/spbdd.hpp)")
    endif()
    set(_spbdd_src "${FTEC_SPBDD_SOURCE_DIR}")
    message(STATUS "SPBDD: using ${_spbdd_src}")
else()
    FetchContent_Declare(spbdd
        GIT_REPOSITORY https://github.com/jtsai1120/SPBDD.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE)
    # SPBDD ships a Makefile rather than a CMakeLists.txt, so this only
    # downloads; the target is defined below.
    FetchContent_MakeAvailable(spbdd)
    set(_spbdd_src "${spbdd_SOURCE_DIR}")
    message(STATUS "SPBDD: fetched into ${_spbdd_src}")
endif()

if(FTEC_CUDD_SOURCE_DIR)
    if(NOT EXISTS "${FTEC_CUDD_SOURCE_DIR}/cudd/cudd.h")
        message(FATAL_ERROR
            "FTEC_CUDD_SOURCE_DIR=${FTEC_CUDD_SOURCE_DIR} does not look like a "
            "CUDD checkout (no cudd/cudd.h)")
    endif()
    set(_cudd_src "${FTEC_CUDD_SOURCE_DIR}")
    message(STATUS "CUDD: using ${_cudd_src}")
else()
    # A tag rather than a branch: CUDD's default branch is `release` and has
    # not moved since 3.0.0, so pinning costs nothing and makes the autotools
    # build below something that either works or does not, rather than
    # something that works until upstream moves.
    FetchContent_Declare(cudd
        GIT_REPOSITORY https://github.com/ivmai/cudd.git
        GIT_TAG        cudd-3.0.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(cudd)
    set(_cudd_src "${cudd_SOURCE_DIR}")
    message(STATUS "CUDD: fetched into ${_cudd_src}")
endif()

# --- CUDD ------------------------------------------------------------------
# Built in its own tree by its own build system, once, and consumed as a plain
# archive. Nothing is installed anywhere.

set(_cudd_lib "${_cudd_src}/cudd/.libs/libcudd.a")

include(ProcessorCount)
ProcessorCount(_cudd_jobs)
if(_cudd_jobs EQUAL 0)
    set(_cudd_jobs 1)
endif()

add_custom_command(
    OUTPUT  "${_cudd_lib}"
    COMMAND "${CMAKE_COMMAND}" -E env
            "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
            bash "${CMAKE_CURRENT_LIST_DIR}/build_cudd.sh" "${_cudd_src}" "${_cudd_jobs}"
    COMMENT "Building CUDD in ${_cudd_src} (autotools; first time only)"
    VERBATIM)

add_custom_target(cudd_build DEPENDS "${_cudd_lib}")

# --- SPBDD -----------------------------------------------------------------
# Six translation units and a public header directory. SPBDD's own Makefile
# also bundles CUDD's objects into libspbdd.a so that a program links one
# archive; here CMake propagates the second archive itself, so there is nothing
# to bundle.

file(GLOB _spbdd_sources CONFIGURE_DEPENDS "${_spbdd_src}/src/*.cpp")
if(NOT _spbdd_sources)
    message(FATAL_ERROR "SPBDD: no sources found under ${_spbdd_src}/src")
endif()

add_library(spbdd STATIC ${_spbdd_sources})
add_dependencies(spbdd cudd_build)

# SPBDD's public headers forward-declare DdManager and DdNode instead of
# including CUDD, so CUDD's headers are this library's business alone.
target_include_directories(spbdd
    PUBLIC  "${_spbdd_src}/include"
    PRIVATE "${_cudd_src}/cudd")
target_link_libraries(spbdd PUBLIC "${_cudd_lib}" m)
target_compile_features(spbdd PUBLIC cxx_std_17)
set_target_properties(spbdd PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(SPBDD::spbdd ALIAS spbdd)

# CUDD's headers on their own, for code that has to reach past SPBDD's public
# API. SPBDD exposes only "reordering on or off" and hard-codes the method to
# sifting; choosing another one, or moving the threshold that decides when a
# reordering fires, means calling CUDD on the DdManager that Manager::raw()
# hands out. That is what this target is for and the only thing it is for --
# link it PRIVATE, so the escape hatch does not leak into anyone's public API.
add_library(cudd_headers INTERFACE)
target_include_directories(cudd_headers INTERFACE "${_cudd_src}/cudd")
add_library(CUDD::headers ALIAS cudd_headers)
