# The SAT backend: symbolic Pauli propagation plus a CryptoMiniSat encoding
# of the paper's SAT queries, behind the same ftec::Backend interface the dd
# backend implements.

find_package(cryptominisat5 CONFIG REQUIRED)

add_library(ftec_sat
    src/pauli_gf2.cpp
    src/stabilizer_gf2.cpp
    src/sat_backend.cpp
)

target_include_directories(ftec_sat PUBLIC include)
target_include_directories(ftec_sat PRIVATE ${CRYPTOMINISAT5_INCLUDE_DIRS})
target_link_libraries(ftec_sat PUBLIC ftec_core PRIVATE cryptominisat5)
target_compile_options(ftec_sat PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    foreach(_t pauli_gf2_test sat_backend_test stabilizer_gf2_test)
        add_executable(sat_${_t} test/${_t}.cpp)
        target_link_libraries(sat_${_t} PRIVATE ftec_sat)
        target_compile_options(sat_${_t} PRIVATE -Wall -Wextra -Wpedantic)
        add_test(NAME sat.${_t}
                 COMMAND sat_${_t} "${CMAKE_SOURCE_DIR}"
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
endif()
