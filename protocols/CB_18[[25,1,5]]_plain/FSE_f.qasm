OPENQASM 3.0;
include "stdgates.inc";

// Combined serial flagged extraction for the [[25,1,5]] rotated surface code.
// Source geometry: Chamberland--Beverland Figure 8.  Data qubits are numbered
// row-major on the 5x5 lattice.  Syndrome bits m[0..11] are Z checks and
// m[12..23] are X checks.  The reusable flag implements Figure 2(b).
// Direct controlled-Pauli convention used in this file:
//   syndrome ancilla: |+> preparation, X-basis measurement
//   Z check coupling:  CZ(syn, data)
//   X check coupling:  CX(syn -> data)
//   flag ancilla:      |0> preparation, Z-basis measurement
//   flag coupling:     CX(syn -> flag)
// Thus no per-data H gates and no flag H gates are needed.
qubit[25] data;
qubit syn;
qubit flag;
bit[24] m;
bit[16] f;

// g0 = Z1 Z6 (left boundary, weight 2; no flag required).
reset syn; h syn;
cz syn,data[0]; cz syn,data[5];
h syn; m[0]=measure syn;
barrier data,syn,flag;

// g1 = Z2 Z3 Z7 Z8; flag f0.
reset syn; h syn; reset flag;
cz syn,data[1]; cx syn,flag; cz syn,data[2];
cz syn,data[6]; cx syn,flag; cz syn,data[7];
h syn; m[1]=measure syn; f[0]=measure flag;
barrier data,syn,flag;

// g2 = Z4 Z5 Z9 Z10; flag f1.
reset syn; h syn; reset flag;
cz syn,data[3]; cx syn,flag; cz syn,data[4];
cz syn,data[8]; cx syn,flag; cz syn,data[9];
h syn; m[2]=measure syn; f[1]=measure flag;
barrier data,syn,flag;

// g3 = Z6 Z7 Z11 Z12; flag f2.
reset syn; h syn; reset flag;
cz syn,data[5]; cx syn,flag; cz syn,data[6];
cz syn,data[10]; cx syn,flag; cz syn,data[11];
h syn; m[3]=measure syn; f[2]=measure flag;
barrier data,syn,flag;

// g4 = Z8 Z9 Z13 Z14; flag f3.
reset syn; h syn; reset flag;
cz syn,data[7]; cx syn,flag; cz syn,data[8];
cz syn,data[12]; cx syn,flag; cz syn,data[13];
h syn; m[4]=measure syn; f[3]=measure flag;
barrier data,syn,flag;

// g5 = Z10 Z15 (right boundary, weight 2; no flag required).
reset syn; h syn;
cz syn,data[9]; cz syn,data[14];
h syn; m[5]=measure syn;
barrier data,syn,flag;

// g6 = Z11 Z16 (left boundary, weight 2; no flag required).
reset syn; h syn;
cz syn,data[10]; cz syn,data[15];
h syn; m[6]=measure syn;
barrier data,syn,flag;

// g7 = Z12 Z13 Z17 Z18; flag f4.
reset syn; h syn; reset flag;
cz syn,data[11]; cx syn,flag; cz syn,data[12];
cz syn,data[16]; cx syn,flag; cz syn,data[17];
h syn; m[7]=measure syn; f[4]=measure flag;
barrier data,syn,flag;

// g8 = Z14 Z15 Z19 Z20; flag f5.
reset syn; h syn; reset flag;
cz syn,data[13]; cx syn,flag; cz syn,data[14];
cz syn,data[18]; cx syn,flag; cz syn,data[19];
h syn; m[8]=measure syn; f[5]=measure flag;
barrier data,syn,flag;

// g9 = Z16 Z17 Z21 Z22; flag f6.
reset syn; h syn; reset flag;
cz syn,data[15]; cx syn,flag; cz syn,data[16];
cz syn,data[20]; cx syn,flag; cz syn,data[21];
h syn; m[9]=measure syn; f[6]=measure flag;
barrier data,syn,flag;

// g10 = Z18 Z19 Z23 Z24; flag f7.
reset syn; h syn; reset flag;
cz syn,data[17]; cx syn,flag; cz syn,data[18];
cz syn,data[22]; cx syn,flag; cz syn,data[23];
h syn; m[10]=measure syn; f[7]=measure flag;
barrier data,syn,flag;

// g11 = Z20 Z25 (right boundary, weight 2; no flag required).
reset syn; h syn;
cz syn,data[19]; cz syn,data[24];
h syn; m[11]=measure syn;
barrier data,syn,flag;

// g12 = X2 X3 (top boundary, weight 2; no flag required).
reset syn; h syn; cx syn,data[1]; cx syn,data[2];
h syn; m[12]=measure syn;
barrier data,syn,flag;

// g13 = X4 X5 (top boundary, weight 2; no flag required).
reset syn; h syn; cx syn,data[3]; cx syn,data[4];
h syn; m[13]=measure syn;
barrier data,syn,flag;

// g14 = X1 X2 X6 X7; flag f8.
reset syn; h syn; reset flag;
cx syn,data[0]; cx syn,flag; cx syn,data[1];
cx syn,data[5]; cx syn,flag; cx syn,data[6];
h syn; m[14]=measure syn; f[8]=measure flag;
barrier data,syn,flag;

// g15 = X3 X4 X8 X9; flag f9.
reset syn; h syn; reset flag;
cx syn,data[2]; cx syn,flag; cx syn,data[3];
cx syn,data[7]; cx syn,flag; cx syn,data[8];
h syn; m[15]=measure syn; f[9]=measure flag;
barrier data,syn,flag;

// g16 = X7 X8 X12 X13; flag f10.
reset syn; h syn; reset flag;
cx syn,data[6]; cx syn,flag; cx syn,data[7];
cx syn,data[11]; cx syn,flag; cx syn,data[12];
h syn; m[16]=measure syn; f[10]=measure flag;
barrier data,syn,flag;

// g17 = X9 X10 X14 X15; flag f11.
reset syn; h syn; reset flag;
cx syn,data[8]; cx syn,flag; cx syn,data[9];
cx syn,data[13]; cx syn,flag; cx syn,data[14];
h syn; m[17]=measure syn; f[11]=measure flag;
barrier data,syn,flag;

// g18 = X11 X12 X16 X17; flag f12.
reset syn; h syn; reset flag;
cx syn,data[10]; cx syn,flag; cx syn,data[11];
cx syn,data[15]; cx syn,flag; cx syn,data[16];
h syn; m[18]=measure syn; f[12]=measure flag;
barrier data,syn,flag;

// g19 = X13 X14 X18 X19; flag f13.
reset syn; h syn; reset flag;
cx syn,data[12]; cx syn,flag; cx syn,data[13];
cx syn,data[17]; cx syn,flag; cx syn,data[18];
h syn; m[19]=measure syn; f[13]=measure flag;
barrier data,syn,flag;

// g20 = X17 X18 X22 X23; flag f14.
reset syn; h syn; reset flag;
cx syn,data[16]; cx syn,flag; cx syn,data[17];
cx syn,data[21]; cx syn,flag; cx syn,data[22];
h syn; m[20]=measure syn; f[14]=measure flag;
barrier data,syn,flag;

// g21 = X19 X20 X24 X25; flag f15.
reset syn; h syn; reset flag;
cx syn,data[18]; cx syn,flag; cx syn,data[19];
cx syn,data[23]; cx syn,flag; cx syn,data[24];
h syn; m[21]=measure syn; f[15]=measure flag;
barrier data,syn,flag;

// g22 = X21 X22 (bottom boundary, weight 2; no flag required).
reset syn; h syn; cx syn,data[20]; cx syn,data[21];
h syn; m[22]=measure syn;
barrier data,syn,flag;

// g23 = X23 X24 (bottom boundary, weight 2; no flag required).
reset syn; h syn; cx syn,data[22]; cx syn,data[23];
h syn; m[23]=measure syn;
