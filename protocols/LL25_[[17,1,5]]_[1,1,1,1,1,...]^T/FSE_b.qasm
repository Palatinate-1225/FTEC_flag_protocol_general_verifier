OPENQASM 3.0;
include "stdgates.inc";

// [[17,1,5]] CSS code: fully parallel [1,1,1,1,1,1,1,1]^T bare syndrome extraction (FSE_b).
// Gate order and data-syndrome interaction structure follow
// from Liou & Lai, arXiv:2407.00607, Appendix A (Fig. 19, Z-type block),
// with all flag qubits and all two-qubit gates incident on flag qubits removed;
// the X-type block applies the same bare topology to the CSS X-generators.
// Convention: measurement ancilla |+>, measured in X basis;
//   Z-data coupling: CZ(measurement, data); X-data coupling: CX(measurement -> data);
// data[0] = q1, ..., data[16] = q17.
// Greedy list-scheduling of the extracted gate order reproduces circuit depth 14,
// matching Table I of the paper.

qubit[17] data;
qubit[8] syn;

bit[16] m;

// ------------------------------------------------------------
// Z-type stabilizers g1..g8 (fully parallel [1,1,1,1,1,1,1,1]^T): depth 14
// Fig. 19 schedule; barrier-separated layers 1..14.
// ------------------------------------------------------------
reset syn[0];
h syn[0];
reset syn[1];
h syn[1];
reset syn[2];
h syn[2];
reset syn[3];
h syn[3];
reset syn[4];
h syn[4];
reset syn[5];
h syn[5];
reset syn[6];
h syn[6];
reset syn[7];
h syn[7];

// -- depth 1 --
cz syn[0], data[2];
cz syn[1], data[0];
cz syn[2], data[4];
cz syn[3], data[6];
cz syn[4], data[8];
cz syn[5], data[10];
cz syn[6], data[7];
cz syn[7], data[13];
barrier data, syn;

// -- depth 2 --
barrier data, syn;

// -- depth 3 --
cz syn[7], data[9];
barrier data, syn;

// -- depth 4 --
cz syn[0], data[0];
cz syn[1], data[2];
cz syn[2], data[5];
cz syn[3], data[7];
cz syn[4], data[9];
cz syn[5], data[11];
cz syn[6], data[15];
barrier data, syn;

// -- depth 5 --
cz syn[7], data[14];
barrier data, syn;

// -- depth 6 --
cz syn[0], data[3];
cz syn[1], data[4];
cz syn[2], data[8];
cz syn[3], data[10];
cz syn[4], data[12];
cz syn[5], data[14];
cz syn[6], data[11];
cz syn[7], data[5];
barrier data, syn;

// -- depth 7 --
barrier data, syn;

// -- depth 8 --
cz syn[0], data[1];
cz syn[1], data[5];
cz syn[2], data[9];
cz syn[3], data[11];
cz syn[4], data[13];
cz syn[5], data[15];
cz syn[6], data[16];
cz syn[7], data[2];
barrier data, syn;

// -- depth 9 --
cz syn[7], data[6];
barrier data, syn;

// -- depth 10 --
barrier data, syn;

// -- depth 11 --
cz syn[7], data[3];
barrier data, syn;

// -- depth 12 --
barrier data, syn;

// -- depth 13 --
barrier data, syn;

// -- depth 14 --
cz syn[7], data[10];
barrier data, syn;

// Measurement ancillas in X basis.
h syn[0];
m[0] = measure syn[0];
h syn[1];
m[1] = measure syn[1];
h syn[2];
m[2] = measure syn[2];
h syn[3];
m[3] = measure syn[3];
h syn[4];
m[4] = measure syn[4];
h syn[5];
m[5] = measure syn[5];
h syn[6];
m[6] = measure syn[6];
h syn[7];
m[7] = measure syn[7];

barrier data, syn;

// ------------------------------------------------------------
// X-type stabilizers g9..g16: CSS Hadamard counterpart of Fig. 19
// Same 14-layer topology/order as the verified Z-type block.
// Z-data CZ couplings are replaced by CX(measurement -> data);
// ------------------------------------------------------------
reset syn[0];
h syn[0];
reset syn[1];
h syn[1];
reset syn[2];
h syn[2];
reset syn[3];
h syn[3];
reset syn[4];
h syn[4];
reset syn[5];
h syn[5];
reset syn[6];
h syn[6];
reset syn[7];
h syn[7];

// -- depth 1 --
cx syn[0], data[2];
cx syn[1], data[0];
cx syn[2], data[4];
cx syn[3], data[6];
cx syn[4], data[8];
cx syn[5], data[10];
cx syn[6], data[7];
cx syn[7], data[13];
barrier data, syn;

// -- depth 2 --
barrier data, syn;

// -- depth 3 --
cx syn[7], data[9];
barrier data, syn;

// -- depth 4 --
cx syn[0], data[0];
cx syn[1], data[2];
cx syn[2], data[5];
cx syn[3], data[7];
cx syn[4], data[9];
cx syn[5], data[11];
cx syn[6], data[15];
barrier data, syn;

// -- depth 5 --
cx syn[7], data[14];
barrier data, syn;

// -- depth 6 --
cx syn[0], data[3];
cx syn[1], data[4];
cx syn[2], data[8];
cx syn[3], data[10];
cx syn[4], data[12];
cx syn[5], data[14];
cx syn[6], data[11];
cx syn[7], data[5];
barrier data, syn;

// -- depth 7 --
barrier data, syn;

// -- depth 8 --
cx syn[0], data[1];
cx syn[1], data[5];
cx syn[2], data[9];
cx syn[3], data[11];
cx syn[4], data[13];
cx syn[5], data[15];
cx syn[6], data[16];
cx syn[7], data[2];
barrier data, syn;

// -- depth 9 --
cx syn[7], data[6];
barrier data, syn;

// -- depth 10 --
barrier data, syn;

// -- depth 11 --
cx syn[7], data[3];
barrier data, syn;

// -- depth 12 --
barrier data, syn;

// -- depth 13 --
barrier data, syn;

// -- depth 14 --
cx syn[7], data[10];
barrier data, syn;

// Measurement ancillas in X basis.
h syn[0];
m[8] = measure syn[0];
h syn[1];
m[9] = measure syn[1];
h syn[2];
m[10] = measure syn[2];
h syn[3];
m[11] = measure syn[3];
h syn[4];
m[12] = measure syn[4];
h syn[5];
m[13] = measure syn[5];
h syn[6];
m[14] = measure syn[6];
h syn[7];
m[15] = measure syn[7];
