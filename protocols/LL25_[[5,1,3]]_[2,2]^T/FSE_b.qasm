OPENQASM 3.0;
include "stdgates.inc";

// [[5,1,3]] complete RAW / unflagged SE for g1,g5,g3,g4.
// Fully-parallel 4-depth schedule corresponding to the raw-SE schedule
// used in Liou & Lai's parallel flag-sharing simulation code.
//
//   g1 = X Z Z X I -> syn[0]
//   g5 = Z Z X I X -> syn[1]
//   g3 = X I X Z Z -> syn[2]
//   g4 = Z X I X Z -> syn[3]
//
// m[0..3] = (m1,m5,m3,m4).
// Two-qubit data-SE depth = 4, total two-qubit gates = 16.

qubit[5] data;
qubit[4] syn;
bit[4] m;

reset syn[0];
h syn[0];
reset syn[1];
h syn[1];
reset syn[2];
h syn[2];
reset syn[3];
h syn[3];

cz syn[2], data[0];
cz syn[0], data[1];
cx syn[1], data[2];
cx syn[3], data[3];

cz syn[2], data[1];
cz syn[0], data[2];

cz syn[3], data[0];
cx syn[2], data[2];
cx syn[0], data[3];
cz syn[1], data[4];

cx syn[1], data[0];
cx syn[3], data[1];

cx syn[0], data[0];
cx syn[2], data[4];

cz syn[1], data[3];
cz syn[3], data[4];

h syn[0];
m[0] = measure syn[0];
h syn[1];
m[1] = measure syn[1];
h syn[2];
m[2] = measure syn[2];
h syn[3];
m[3] = measure syn[3];
