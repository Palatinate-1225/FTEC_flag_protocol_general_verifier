OPENQASM 3.0;
include "stdgates.inc";

// Liou & Lai (LL25), [[19,1,5]] fully-parallel 1-vector SE.
// The Z block below is transcribed layer-by-layer from Fig. 21.
// The X block is its CSS Hadamard counterpart with the IDENTICAL layout:
//   Z data interaction -> CZ(syn,data)
//   X data interaction -> CX(syn,data)
// Flag interactions remain CX(syn->flag).
//
// Conventions:
//   measurement ancilla: |+>, X-basis measurement
//   flag ancilla:        |0>, Z-basis measurement
//   data[0] = q1, ..., data[18] = q19
//   syn[0..8] = m1..m9 within each CSS block
//   flag[0..11] = f1..f12 within each CSS block

qubit[19] data;
qubit[9] syn;
qubit[12] flag;

bit[18] m;
bit[24] f;

// ------------------------------------------------------------
// Z-type g1..g9: exact LL25 Fig. 21 10-layer layout
// No scheduler optimization is applied.
// Within Fig. 21 depths 1..6, gates are written in the exact left-to-right
// order m1,m2,m3,m5,m6,m7,m4,m8,m9 shown in the figure.
// ------------------------------------------------------------
reset syn[0];
h syn[0];  // prepare |+> measurement ancilla
reset syn[1];
h syn[1];  // prepare |+> measurement ancilla
reset syn[2];
h syn[2];  // prepare |+> measurement ancilla
reset syn[3];
h syn[3];  // prepare |+> measurement ancilla
reset syn[4];
h syn[4];  // prepare |+> measurement ancilla
reset syn[5];
h syn[5];  // prepare |+> measurement ancilla
reset syn[6];
h syn[6];  // prepare |+> measurement ancilla
reset syn[7];
h syn[7];  // prepare |+> measurement ancilla
reset syn[8];
h syn[8];  // prepare |+> measurement ancilla
reset flag[0];  // prepare |0> flag ancilla
reset flag[1];  // prepare |0> flag ancilla
reset flag[2];  // prepare |0> flag ancilla
reset flag[3];  // prepare |0> flag ancilla
reset flag[4];  // prepare |0> flag ancilla
reset flag[5];  // prepare |0> flag ancilla
reset flag[6];  // prepare |0> flag ancilla
reset flag[7];  // prepare |0> flag ancilla
reset flag[8];  // prepare |0> flag ancilla
reset flag[9];  // prepare |0> flag ancilla
reset flag[10];  // prepare |0> flag ancilla
reset flag[11];  // prepare |0> flag ancilla

// -- Fig. 21 depth 1 --
cz syn[0], data[0];
cz syn[1], data[2];
cz syn[2], data[11];
cz syn[4], data[5];
cz syn[5], data[15];
cz syn[6], data[9];
cz syn[3], data[1];
cz syn[7], data[8];
cz syn[8], data[4];
barrier data, syn, flag;
// -- Fig. 21 depth 2 --
cx syn[0], flag[0];
cx syn[1], flag[1];
cx syn[2], flag[2];
cx syn[4], flag[3];
cx syn[5], flag[4];
cx syn[6], flag[5];
cx syn[3], flag[6];
cx syn[7], flag[8];
cx syn[8], flag[10];
barrier data, syn, flag;
// -- Fig. 21 depth 3 --
cz syn[0], data[1];
cz syn[1], data[0];
cz syn[2], data[12];
cz syn[4], data[8];
cz syn[5], data[16];
cz syn[6], data[10];
cz syn[3], data[4];
cz syn[7], data[7];
cz syn[8], data[6];
barrier data, syn, flag;
// -- Fig. 21 depth 4 --
cz syn[0], data[2];
cz syn[1], data[4];
cz syn[2], data[13];
cz syn[4], data[15];
cz syn[5], data[18];
cz syn[6], data[14];
cx syn[3], flag[7];
cx syn[7], flag[9];
cx syn[8], flag[11];
barrier data, syn, flag;
// -- Fig. 21 depth 5 --
cx syn[0], flag[0];
cx syn[1], flag[1];
cx syn[2], flag[2];
cx syn[4], flag[3];
cx syn[5], flag[4];
cx syn[6], flag[5];
cz syn[3], data[0];
cz syn[7], data[10];
cz syn[8], data[7];
barrier data, syn, flag;
// -- Fig. 21 depth 6 --
cz syn[0], data[3];
cz syn[1], data[6];
cz syn[2], data[14];
cz syn[4], data[18];
cz syn[5], data[17];
cz syn[6], data[11];
cz syn[3], data[5];
cz syn[7], data[9];
cz syn[8], data[10];
barrier data, syn, flag;
// -- Fig. 21 depth 7 --
cx syn[3], flag[6];
cx syn[7], flag[8];
cx syn[8], flag[10];
barrier data, syn, flag;

// -- Fig. 21 depth 8 --
cz syn[3], data[7];
cz syn[7], data[15];
cz syn[8], data[11];
barrier data, syn, flag;

// -- Fig. 21 depth 9 --
cx syn[3], flag[7];
cx syn[7], flag[9];
cx syn[8], flag[11];
barrier data, syn, flag;

// -- Fig. 21 depth 10 --
cz syn[3], data[8];
cz syn[7], data[16];
cz syn[8], data[12];

// Measurement ancillas: X-basis readout.
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
// Flags: Z-basis readout.
f[0] = measure flag[0];
f[1] = measure flag[1];
f[2] = measure flag[2];
f[3] = measure flag[3];
f[4] = measure flag[4];
f[5] = measure flag[5];
f[6] = measure flag[6];
f[7] = measure flag[7];
f[8] = measure flag[8];
f[9] = measure flag[9];
f[10] = measure flag[10];
f[11] = measure flag[11];

barrier data, syn, flag;

// ------------------------------------------------------------
// X-type g10..g18: exact LL25 Fig. 21 10-layer layout
// No scheduler optimization is applied.
// Within Fig. 21 depths 1..6, gates are written in the exact left-to-right
// order m1,m2,m3,m5,m6,m7,m4,m8,m9 shown in the figure.
// ------------------------------------------------------------
reset syn[0];
h syn[0];  // prepare |+> measurement ancilla
reset syn[1];
h syn[1];  // prepare |+> measurement ancilla
reset syn[2];
h syn[2];  // prepare |+> measurement ancilla
reset syn[3];
h syn[3];  // prepare |+> measurement ancilla
reset syn[4];
h syn[4];  // prepare |+> measurement ancilla
reset syn[5];
h syn[5];  // prepare |+> measurement ancilla
reset syn[6];
h syn[6];  // prepare |+> measurement ancilla
reset syn[7];
h syn[7];  // prepare |+> measurement ancilla
reset syn[8];
h syn[8];  // prepare |+> measurement ancilla
reset flag[0];  // prepare |0> flag ancilla
reset flag[1];  // prepare |0> flag ancilla
reset flag[2];  // prepare |0> flag ancilla
reset flag[3];  // prepare |0> flag ancilla
reset flag[4];  // prepare |0> flag ancilla
reset flag[5];  // prepare |0> flag ancilla
reset flag[6];  // prepare |0> flag ancilla
reset flag[7];  // prepare |0> flag ancilla
reset flag[8];  // prepare |0> flag ancilla
reset flag[9];  // prepare |0> flag ancilla
reset flag[10];  // prepare |0> flag ancilla
reset flag[11];  // prepare |0> flag ancilla

// -- Fig. 21 depth 1 --
cx syn[0], data[0];
cx syn[1], data[2];
cx syn[2], data[11];
cx syn[4], data[5];
cx syn[5], data[15];
cx syn[6], data[9];
cx syn[3], data[1];
cx syn[7], data[8];
cx syn[8], data[4];
barrier data, syn, flag;
// -- Fig. 21 depth 2 --
cx syn[0], flag[0];
cx syn[1], flag[1];
cx syn[2], flag[2];
cx syn[4], flag[3];
cx syn[5], flag[4];
cx syn[6], flag[5];
cx syn[3], flag[6];
cx syn[7], flag[8];
cx syn[8], flag[10];
barrier data, syn, flag;
// -- Fig. 21 depth 3 --
cx syn[0], data[1];
cx syn[1], data[0];
cx syn[2], data[12];
cx syn[4], data[8];
cx syn[5], data[16];
cx syn[6], data[10];
cx syn[3], data[4];
cx syn[7], data[7];
cx syn[8], data[6];
barrier data, syn, flag;
// -- Fig. 21 depth 4 --
cx syn[0], data[2];
cx syn[1], data[4];
cx syn[2], data[13];
cx syn[4], data[15];
cx syn[5], data[18];
cx syn[6], data[14];
cx syn[3], flag[7];
cx syn[7], flag[9];
cx syn[8], flag[11];
barrier data, syn, flag;
// -- Fig. 21 depth 5 --
cx syn[0], flag[0];
cx syn[1], flag[1];
cx syn[2], flag[2];
cx syn[4], flag[3];
cx syn[5], flag[4];
cx syn[6], flag[5];
cx syn[3], data[0];
cx syn[7], data[10];
cx syn[8], data[7];
barrier data, syn, flag;
// -- Fig. 21 depth 6 --
cx syn[0], data[3];
cx syn[1], data[6];
cx syn[2], data[14];
cx syn[4], data[18];
cx syn[5], data[17];
cx syn[6], data[11];
cx syn[3], data[5];
cx syn[7], data[9];
cx syn[8], data[10];
barrier data, syn, flag;
// -- Fig. 21 depth 7 --
cx syn[3], flag[6];
cx syn[7], flag[8];
cx syn[8], flag[10];
barrier data, syn, flag;

// -- Fig. 21 depth 8 --
cx syn[3], data[7];
cx syn[7], data[15];
cx syn[8], data[11];
barrier data, syn, flag;

// -- Fig. 21 depth 9 --
cx syn[3], flag[7];
cx syn[7], flag[9];
cx syn[8], flag[11];
barrier data, syn, flag;

// -- Fig. 21 depth 10 --
cx syn[3], data[8];
cx syn[7], data[16];
cx syn[8], data[12];

// Measurement ancillas: X-basis readout.
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
// Flags: Z-basis readout.
f[12] = measure flag[0];
f[13] = measure flag[1];
f[14] = measure flag[2];
f[15] = measure flag[3];
f[16] = measure flag[4];
f[17] = measure flag[5];
f[18] = measure flag[6];
f[19] = measure flag[7];
f[20] = measure flag[8];
f[21] = measure flag[9];
f[22] = measure flag[10];
f[23] = measure flag[11];
