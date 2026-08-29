OPENQASM 3.0;
include "stdgates.inc";

// Combined serial flagged syndrome extraction for the [[17,1,5]] 2D color code.
// Source: Chamberland & Beverland, arXiv:1708.02246, Table 7.
// Generator order follows Table 7.

// data[0] = q1, ..., data[16] = q17.
// syn is the syndrome/measurement ancilla, reused for every generator.
// Convention: syn is prepared in |+> and measured in the X basis.
// Z checks use CZ(syn,data); X checks use CX(syn->data).
// flag qubits are prepared in |0>, measured in Z, and coupled by CX(syn->flag).
// flag[0..2] are reusable physical flag ancillas.
// m[i] stores the syndrome bit for generator gi.
// f is a compact classical flag record with no unused bits.
// For generator gi, its flag outcomes start at the offset shown in comments below.

qubit[17] data;
qubit syn;
qubit[3] flag;

bit[16] m;
bit[20] f;

// Compact flag-record layout:
// g0 = Z1Z2Z3Z4: f[0]
// g1 = X1X2X3X4: f[1]
// g2 = Z1Z3Z5Z6: f[2]
// g3 = X1X3X5X6: f[3]
// g4 = Z5Z6Z9Z10: f[4]
// g5 = X5X6X9X10: f[5]
// g6 = Z7Z8Z11Z12: f[6]
// g7 = X7X8X11X12: f[7]
// g8 = Z9Z10Z13Z14: f[8]
// g9 = X9X10X13X14: f[9]
// g10 = Z11Z12Z15Z16: f[10]
// g11 = X11X12X15X16: f[11]
// g12 = Z8Z12Z16Z17: f[12]
// g13 = X8X12X16X17: f[13]
// g14 = Z3Z4Z6Z7Z10Z11Z14Z15: f[14]..f[16]
// g15 = X3X4X6X7X10X11X14X15: f[17]..f[19]

// ------------------------------------------------------------
// Flagged SE for g0 = Z1Z2Z3Z4
// weight 4; uses 1 flag qubit(s); records to f[0]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[0];
cx syn, flag[0];
cz syn, data[1];
cz syn, data[2];
cx syn, flag[0];
cz syn, data[3];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[0] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[0] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g1 = X1X2X3X4
// weight 4; uses 1 flag qubit(s); records to f[1]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[0];
cx syn, flag[0];
cx syn, data[1];
cx syn, data[2];
cx syn, flag[0];
cx syn, data[3];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[1] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[1] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g2 = Z1Z3Z5Z6
// weight 4; uses 1 flag qubit(s); records to f[2]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[0];
cx syn, flag[0];
cz syn, data[2];
cz syn, data[4];
cx syn, flag[0];
cz syn, data[5];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[2] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[2] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g3 = X1X3X5X6
// weight 4; uses 1 flag qubit(s); records to f[3]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[0];
cx syn, flag[0];
cx syn, data[2];
cx syn, data[4];
cx syn, flag[0];
cx syn, data[5];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[3] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[3] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g4 = Z5Z6Z9Z10
// weight 4; uses 1 flag qubit(s); records to f[4]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[4];
cx syn, flag[0];
cz syn, data[5];
cz syn, data[8];
cx syn, flag[0];
cz syn, data[9];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[4] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[4] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g5 = X5X6X9X10
// weight 4; uses 1 flag qubit(s); records to f[5]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[4];
cx syn, flag[0];
cx syn, data[5];
cx syn, data[8];
cx syn, flag[0];
cx syn, data[9];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[5] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[5] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g6 = Z7Z8Z11Z12
// weight 4; uses 1 flag qubit(s); records to f[6]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[6];
cx syn, flag[0];
cz syn, data[7];
cz syn, data[10];
cx syn, flag[0];
cz syn, data[11];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[6] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[6] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g7 = X7X8X11X12
// weight 4; uses 1 flag qubit(s); records to f[7]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[6];
cx syn, flag[0];
cx syn, data[7];
cx syn, data[10];
cx syn, flag[0];
cx syn, data[11];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[7] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[7] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g8 = Z9Z10Z13Z14
// weight 4; uses 1 flag qubit(s); records to f[8]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[8];
cx syn, flag[0];
cz syn, data[9];
cz syn, data[12];
cx syn, flag[0];
cz syn, data[13];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[8] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[8] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g9 = X9X10X13X14
// weight 4; uses 1 flag qubit(s); records to f[9]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[8];
cx syn, flag[0];
cx syn, data[9];
cx syn, data[12];
cx syn, flag[0];
cx syn, data[13];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[9] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[9] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g10 = Z11Z12Z15Z16
// weight 4; uses 1 flag qubit(s); records to f[10]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[10];
cx syn, flag[0];
cz syn, data[11];
cz syn, data[14];
cx syn, flag[0];
cz syn, data[15];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[10] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[10] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g11 = X11X12X15X16
// weight 4; uses 1 flag qubit(s); records to f[11]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[10];
cx syn, flag[0];
cx syn, data[11];
cx syn, data[14];
cx syn, flag[0];
cx syn, data[15];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[11] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[11] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g12 = Z8Z12Z16Z17
// weight 4; uses 1 flag qubit(s); records to f[12]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[7];
cx syn, flag[0];
cz syn, data[11];
cz syn, data[15];
cx syn, flag[0];
cz syn, data[16];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[12] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[12] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g13 = X8X12X16X17
// weight 4; uses 1 flag qubit(s); records to f[13]
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];


// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[7];
cx syn, flag[0];
cx syn, data[11];
cx syn, data[15];
cx syn, flag[0];
cx syn, data[16];


// Measure syndrome in X basis.
h syn;  // X-basis readout
m[13] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[13] = measure flag[0];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g14 = Z3Z4Z6Z7Z10Z11Z14Z15
// weight 8; uses 3 flag qubit(s); records to f[14]..f[16]
// Fig. 7(b) flag/data ordering:
// D1-F1-D2-F2-D3-D4-F3-D5-D6-F1-D7-F3-F2-D8
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];
reset flag[1];
reset flag[2];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cz syn, data[2];
cx syn, flag[0];
cz syn, data[3];
cx syn, flag[1];
cz syn, data[5];
cz syn, data[6];
cx syn, flag[2];
cz syn, data[9];
cz syn, data[10];
cx syn, flag[0];
cz syn, data[13];
cx syn, flag[2];
cx syn, flag[1];
cz syn, data[14];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[14] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[14] = measure flag[0];
f[15] = measure flag[1];
f[16] = measure flag[2];

barrier data, syn, flag;

// ------------------------------------------------------------
// Flagged SE for g15 = X3X4X6X7X10X11X14X15
// weight 8; uses 3 flag qubit(s); records to f[17]..f[19]
// Fig. 7(b) flag/data ordering:
// D1-F1-D2-F2-D3-D4-F3-D5-D6-F1-D7-F3-F2-D8
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>
reset flag[0];
reset flag[1];
reset flag[2];

// Interleaved direct data-syndrome and syndrome-to-flag couplings.
cx syn, data[2];
cx syn, flag[0];
cx syn, data[3];
cx syn, flag[1];
cx syn, data[5];
cx syn, data[6];
cx syn, flag[2];
cx syn, data[9];
cx syn, data[10];
cx syn, flag[0];
cx syn, data[13];
cx syn, flag[2];
cx syn, flag[1];
cx syn, data[14];

// Measure syndrome in X basis.
h syn;  // X-basis readout
m[15] = measure syn;

// Measure used flag qubit(s) in Z basis, compactly stored.
f[17] = measure flag[0];
f[18] = measure flag[1];
f[19] = measure flag[2];

barrier data, syn, flag;
