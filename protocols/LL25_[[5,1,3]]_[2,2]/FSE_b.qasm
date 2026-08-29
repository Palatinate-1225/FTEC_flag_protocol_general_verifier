OPENQASM 3.0;
include "stdgates.inc";

// [[5,1,3]] complete RAW / unflagged SE for g1,g5,g3,g4.
// Serial version, matching the generator-by-generator raw-SE style used
// by Liou & Lai's flag-sharing serial simulation code.
//
//   g1 = X Z Z X I
//   g5 = Z Z X I X
//   g3 = X I X Z Z
//   g4 = Z X I X Z
//
// One measurement ancilla is reset and reused four times.
// m[0..3] = (m1,m5,m3,m4).

qubit[5] data;
qubit[2] syn;
bit[4] m;

reset syn[0];
reset syn[1];
h syn[0];
h syn[1];
cz syn[0], data[1];
cx syn[1], data[2];
cz syn[0], data[2];
cx syn[0], data[3];
cz syn[1], data[4];
cx syn[1], data[0];
cx syn[0], data[0];
cz syn[1], data[3];
h syn[0];
h syn[1];
m[0] = measure syn[0];
m[1] = measure syn[1];

reset syn[0];
reset syn[1];
h syn[0];
h syn[1];
cz syn[0], data[0];
cx syn[1], data[3];
cz syn[0], data[1];
cz syn[1], data[0];
cx syn[0], data[2];
cx syn[1], data[1];
cx syn[0], data[4];
cz syn[1], data[4];
h syn[0];
h syn[1];
m[2] = measure syn[0];
m[3] = measure syn[1];

