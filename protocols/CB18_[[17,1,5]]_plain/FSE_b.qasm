OPENQASM 3.0;
include "stdgates.inc";

// Combined unflagged syndrome extraction for all 16 generators
// of the [[17,1,5]] color code, in Table-7 order.

// data[0] = q1, ..., data[16] = q17.
// syn = measurement ancilla, reused for every generator.
// Convention: syn is prepared in |+> and measured in the X basis.
// Z checks use CZ(syn,data); X checks use CX(syn->data).

qubit[17] data;
qubit syn;

bit[16] m;


// ------------------------------------------------------------
// Unflagged SE for G0 = Z1Z2Z3Z4
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[0];
cz syn, data[1];
cz syn, data[2];
cz syn, data[3];

h syn;  // X-basis readout
m[0] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G1 = X1X2X3X4
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[0];
cx syn, data[1];
cx syn, data[2];
cx syn, data[3];


h syn;  // X-basis readout
m[1] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G2 = Z1Z3Z5Z6
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[0];
cz syn, data[2];
cz syn, data[4];
cz syn, data[5];

h syn;  // X-basis readout
m[2] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G3 = X1X3X5X6
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[0];
cx syn, data[2];
cx syn, data[4];
cx syn, data[5];


h syn;  // X-basis readout
m[3] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G4 = Z5Z6Z9Z10
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[4];
cz syn, data[5];
cz syn, data[8];
cz syn, data[9];

h syn;  // X-basis readout
m[4] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G5 = X5X6X9X10
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[4];
cx syn, data[5];
cx syn, data[8];
cx syn, data[9];


h syn;  // X-basis readout
m[5] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G6 = Z7Z8Z11Z12
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[6];
cz syn, data[7];
cz syn, data[10];
cz syn, data[11];

h syn;  // X-basis readout
m[6] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G7 = X7X8X11X12
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[6];
cx syn, data[7];
cx syn, data[10];
cx syn, data[11];


h syn;  // X-basis readout
m[7] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G8 = Z9Z10Z13Z14
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[8];
cz syn, data[9];
cz syn, data[12];
cz syn, data[13];

h syn;  // X-basis readout
m[8] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G9 = X9X10X13X14
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[8];
cx syn, data[9];
cx syn, data[12];
cx syn, data[13];


h syn;  // X-basis readout
m[9] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G10 = Z11Z12Z15Z16
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[10];
cz syn, data[11];
cz syn, data[14];
cz syn, data[15];

h syn;  // X-basis readout
m[10] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G11 = X11X12X15X16
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[10];
cx syn, data[11];
cx syn, data[14];
cx syn, data[15];


h syn;  // X-basis readout
m[11] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G12 = Z8Z12Z16Z17
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[7];
cz syn, data[11];
cz syn, data[15];
cz syn, data[16];

h syn;  // X-basis readout
m[12] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G13 = X8X12X16X17
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[7];
cx syn, data[11];
cx syn, data[15];
cx syn, data[16];


h syn;  // X-basis readout
m[13] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G14 = Z3Z4Z6Z7Z10Z11Z14Z15
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>

cz syn, data[2];
cz syn, data[3];
cz syn, data[5];
cz syn, data[6];
cz syn, data[9];
cz syn, data[10];
cz syn, data[13];
cz syn, data[14];

h syn;  // X-basis readout
m[14] = measure syn;
barrier data, syn;

// ------------------------------------------------------------
// Unflagged SE for G15 = X3X4X6X7X10X11X14X15
// ------------------------------------------------------------
reset syn;
h syn;  // prepare syndrome ancilla in |+>


cx syn, data[2];
cx syn, data[3];
cx syn, data[5];
cx syn, data[6];
cx syn, data[9];
cx syn, data[10];
cx syn, data[13];
cx syn, data[14];


h syn;  // X-basis readout
m[15] = measure syn;
