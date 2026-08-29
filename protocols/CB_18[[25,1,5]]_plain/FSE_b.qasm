OPENQASM 3.0;
include "stdgates.inc";

// Combined serial unflagged extraction for the same Figure-8 [[25,1,5]]
// rotated surface-code generators and syndrome-bit order as FSE_f.qasm.
// Direct controlled-Pauli convention used in this file:
//   syndrome ancilla: |+> preparation, X-basis measurement
//   Z check coupling:  CZ(syn, data)
//   X check coupling:  CX(syn -> data)
// This removes the per-data H gates used by the original X-check form.
qubit[25] data;
qubit syn;
bit[24] m;

// Z checks g0..g11.
reset syn; h syn; cz syn,data[0]; cz syn,data[5]; h syn; m[0]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[1]; cz syn,data[2]; cz syn,data[6]; cz syn,data[7];
h syn; m[1]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[3]; cz syn,data[4]; cz syn,data[8]; cz syn,data[9];
h syn; m[2]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[5]; cz syn,data[6]; cz syn,data[10]; cz syn,data[11];
h syn; m[3]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[7]; cz syn,data[8]; cz syn,data[12]; cz syn,data[13];
h syn; m[4]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[9]; cz syn,data[14]; h syn; m[5]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[10]; cz syn,data[15]; h syn; m[6]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[11]; cz syn,data[12]; cz syn,data[16]; cz syn,data[17];
h syn; m[7]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[13]; cz syn,data[14]; cz syn,data[18]; cz syn,data[19];
h syn; m[8]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[15]; cz syn,data[16]; cz syn,data[20]; cz syn,data[21];
h syn; m[9]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[17]; cz syn,data[18]; cz syn,data[22]; cz syn,data[23];
h syn; m[10]=measure syn; barrier data,syn;

reset syn; h syn; cz syn,data[19]; cz syn,data[24]; h syn; m[11]=measure syn; barrier data,syn;

// X checks g12..g23. Direct CX(syn -> data) form; no data-basis H gates.
reset syn; h syn; cx syn,data[1]; cx syn,data[2];
h syn; m[12]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[3]; cx syn,data[4];
h syn; m[13]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[0]; cx syn,data[1]; cx syn,data[5]; cx syn,data[6];
h syn; m[14]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[2]; cx syn,data[3]; cx syn,data[7]; cx syn,data[8];
h syn; m[15]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[6]; cx syn,data[7]; cx syn,data[11]; cx syn,data[12];
h syn; m[16]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[8]; cx syn,data[9]; cx syn,data[13]; cx syn,data[14];
h syn; m[17]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[10]; cx syn,data[11]; cx syn,data[15]; cx syn,data[16];
h syn; m[18]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[12]; cx syn,data[13]; cx syn,data[17]; cx syn,data[18];
h syn; m[19]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[16]; cx syn,data[17]; cx syn,data[21]; cx syn,data[22];
h syn; m[20]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[18]; cx syn,data[19]; cx syn,data[23]; cx syn,data[24];
h syn; m[21]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[20]; cx syn,data[21];
h syn; m[22]=measure syn; barrier data,syn;

reset syn; h syn; cx syn,data[22]; cx syn,data[23];
h syn; m[23]=measure syn;
