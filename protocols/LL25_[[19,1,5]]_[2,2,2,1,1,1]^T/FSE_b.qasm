OPENQASM 3.0;
include "stdgates.inc";

// Liou & Lai (LL25), [[19,1,5]], Appendix C, Fig. 22.
// Bare syndrome extraction obtained by deleting flags from the Fig. 22 circuit.
//
// IMPORTANT:
// The two-qubit gates below follow Fig. 22 strictly from left to right.
// The nine barriers are placed at the nine dashed vertical separators
// actually drawn in Fig. 22.  They are NOT produced by a scheduler and
// are NOT intended to represent a newly optimized depth decomposition.
//
// Direct controlled-Pauli convention:
//   syndrome ancilla: |+>, X-basis measurement
//   Z data coupling:  CZ(syn, data)
//   X data coupling:  CX(syn -> data)
//
// data[0..18] = q1..q19
// syn[0..8]   = m1..m9

qubit[19] data;
qubit[9] syn;

bit[18] m;

// ============================================================
// Z-type g1..g9: exact Fig. 22 left-to-right layout
// ============================================================
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
reset syn[8];
h syn[8];

// -- Fig. 22 segment 1 --
cz syn[3], data[0];  // Fig.22 gate 1: q1 - m4
cz syn[7], data[7];  // Fig.22 gate 2: q8 - m8
cz syn[8], data[4];  // Fig.22 gate 3: q5 - m9
barrier data, syn;
// -- Fig. 22 segment 2 --
cz syn[0], data[0];  // Fig.22 gate 4: q1 - m1
cz syn[4], data[8];  // Fig.22 gate 6: q9 - m5
barrier data, syn;
// -- Fig. 22 segment 3 --
cz syn[2], data[11];  // Fig.22 gate 10: q12 - m3
cz syn[3], data[1];  // Fig.22 gate 11: q2 - m4
cz syn[7], data[8];  // Fig.22 gate 13: q9 - m8
cz syn[8], data[6];  // Fig.22 gate 14: q7 - m9
barrier data, syn;
// -- Fig. 22 segment 4 --
cz syn[0], data[1];  // Fig.22 gate 15: q2 - m1
cz syn[4], data[5];  // Fig.22 gate 18: q6 - m5
cz syn[5], data[15];  // Fig.22 gate 19: q16 - m6
cz syn[6], data[9];  // Fig.22 gate 20: q10 - m7
barrier data, syn;
// -- Fig. 22 segment 5 --
cz syn[0], data[2];  // Fig.22 gate 23: q3 - m1
cz syn[1], data[0];  // Fig.22 gate 24: q1 - m2
cz syn[2], data[12];  // Fig.22 gate 25: q13 - m3
cz syn[3], data[4];  // Fig.22 gate 26: q5 - m4
cz syn[4], data[15];  // Fig.22 gate 27: q16 - m5
cz syn[7], data[10];  // Fig.22 gate 29: q11 - m8
cz syn[8], data[7];  // Fig.22 gate 30: q8 - m9
barrier data, syn;
// -- Fig. 22 segment 6 --
cz syn[2], data[13];  // Fig.22 gate 33: q14 - m3
cz syn[3], data[5];  // Fig.22 gate 34: q6 - m4
cz syn[5], data[18];  // Fig.22 gate 36: q19 - m6
cz syn[7], data[9];  // Fig.22 gate 38: q10 - m8
cz syn[8], data[10];  // Fig.22 gate 39: q11 - m9
barrier data, syn;
// -- Fig. 22 segment 7 --
cz syn[0], data[3];  // Fig.22 gate 40: q4 - m1
cz syn[1], data[2];  // Fig.22 gate 41: q3 - m2
cz syn[4], data[18];  // Fig.22 gate 44: q19 - m5
cz syn[5], data[16];  // Fig.22 gate 45: q17 - m6
cz syn[6], data[11];  // Fig.22 gate 46: q12 - m7
barrier data, syn;
// -- Fig. 22 segment 8 --
cz syn[1], data[4];  // Fig.22 gate 49: q5 - m2
cz syn[2], data[14];  // Fig.22 gate 50: q15 - m3
cz syn[3], data[7];  // Fig.22 gate 51: q8 - m4
cz syn[6], data[10];  // Fig.22 gate 53: q11 - m7
cz syn[7], data[15];  // Fig.22 gate 54: q16 - m8
cz syn[8], data[11];  // Fig.22 gate 55: q12 - m9
barrier data, syn;
// -- Fig. 22 segment 9 --
barrier data, syn;
// -- Fig. 22 segment 10 --
cz syn[1], data[6];  // Fig.22 gate 61: q7 - m2
cz syn[3], data[8];  // Fig.22 gate 62: q9 - m4
cz syn[5], data[17];  // Fig.22 gate 63: q18 - m6
cz syn[6], data[14];  // Fig.22 gate 64: q15 - m7
cz syn[7], data[16];  // Fig.22 gate 65: q17 - m8
cz syn[8], data[12];  // Fig.22 gate 66: q13 - m9

// Syndrome ancillas: X-basis readout.
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
h syn[8];
m[8] = measure syn[8];


barrier data, syn;

// ============================================================
// X-type g10..g18: CSS mirror of the exact Fig. 22 layout
// ============================================================
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
reset syn[8];
h syn[8];

// -- Fig. 22 segment 1 --
cx syn[3], data[0];  // Fig.22 gate 1: q1 - m4
cx syn[7], data[7];  // Fig.22 gate 2: q8 - m8
cx syn[8], data[4];  // Fig.22 gate 3: q5 - m9
barrier data, syn;
// -- Fig. 22 segment 2 --
cx syn[0], data[0];  // Fig.22 gate 4: q1 - m1
cx syn[4], data[8];  // Fig.22 gate 6: q9 - m5
barrier data, syn;
// -- Fig. 22 segment 3 --
cx syn[2], data[11];  // Fig.22 gate 10: q12 - m3
cx syn[3], data[1];  // Fig.22 gate 11: q2 - m4
cx syn[7], data[8];  // Fig.22 gate 13: q9 - m8
cx syn[8], data[6];  // Fig.22 gate 14: q7 - m9
barrier data, syn;
// -- Fig. 22 segment 4 --
cx syn[0], data[1];  // Fig.22 gate 15: q2 - m1
cx syn[4], data[5];  // Fig.22 gate 18: q6 - m5
cx syn[5], data[15];  // Fig.22 gate 19: q16 - m6
cx syn[6], data[9];  // Fig.22 gate 20: q10 - m7
barrier data, syn;
// -- Fig. 22 segment 5 --
cx syn[0], data[2];  // Fig.22 gate 23: q3 - m1
cx syn[1], data[0];  // Fig.22 gate 24: q1 - m2
cx syn[2], data[12];  // Fig.22 gate 25: q13 - m3
cx syn[3], data[4];  // Fig.22 gate 26: q5 - m4
cx syn[4], data[15];  // Fig.22 gate 27: q16 - m5
cx syn[7], data[10];  // Fig.22 gate 29: q11 - m8
cx syn[8], data[7];  // Fig.22 gate 30: q8 - m9
barrier data, syn;
// -- Fig. 22 segment 6 --
cx syn[2], data[13];  // Fig.22 gate 33: q14 - m3
cx syn[3], data[5];  // Fig.22 gate 34: q6 - m4
cx syn[5], data[18];  // Fig.22 gate 36: q19 - m6
cx syn[7], data[9];  // Fig.22 gate 38: q10 - m8
cx syn[8], data[10];  // Fig.22 gate 39: q11 - m9
barrier data, syn;
// -- Fig. 22 segment 7 --
cx syn[0], data[3];  // Fig.22 gate 40: q4 - m1
cx syn[1], data[2];  // Fig.22 gate 41: q3 - m2
cx syn[4], data[18];  // Fig.22 gate 44: q19 - m5
cx syn[5], data[16];  // Fig.22 gate 45: q17 - m6
cx syn[6], data[11];  // Fig.22 gate 46: q12 - m7
barrier data, syn;
// -- Fig. 22 segment 8 --
cx syn[1], data[4];  // Fig.22 gate 49: q5 - m2
cx syn[2], data[14];  // Fig.22 gate 50: q15 - m3
cx syn[3], data[7];  // Fig.22 gate 51: q8 - m4
cx syn[6], data[10];  // Fig.22 gate 53: q11 - m7
cx syn[7], data[15];  // Fig.22 gate 54: q16 - m8
cx syn[8], data[11];  // Fig.22 gate 55: q12 - m9
barrier data, syn;
// -- Fig. 22 segment 9 --
barrier data, syn;
// -- Fig. 22 segment 10 --
cx syn[1], data[6];  // Fig.22 gate 61: q7 - m2
cx syn[3], data[8];  // Fig.22 gate 62: q9 - m4
cx syn[5], data[17];  // Fig.22 gate 63: q18 - m6
cx syn[6], data[14];  // Fig.22 gate 64: q15 - m7
cx syn[7], data[16];  // Fig.22 gate 65: q17 - m8
cx syn[8], data[12];  // Fig.22 gate 66: q13 - m9

// Syndrome ancillas: X-basis readout.
h syn[0];
m[9] = measure syn[0];
h syn[1];
m[10] = measure syn[1];
h syn[2];
m[11] = measure syn[2];
h syn[3];
m[12] = measure syn[3];
h syn[4];
m[13] = measure syn[4];
h syn[5];
m[14] = measure syn[5];
h syn[6];
m[15] = measure syn[6];
h syn[7];
m[16] = measure syn[7];
h syn[8];
m[17] = measure syn[8];

