OPENQASM 3.0;
include "stdgates.inc";

// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn  = syndrome ancilla, initialized to |+>
// flag = flag ancilla, initialized to |0>
//
// m = syndrome bit
// f = flag bit
//
// Direct CX/CZ convention:
//   syn  : |+> preparation, X-basis measurement
//   X component: CX(syn -> data)
//   Z component: CZ(syn, data)
//   flag : |0> preparation, Z-basis measurement
//   flag coupling: CX(syn -> flag)

qubit[5] data;
qubit syn;
qubit flag;

bit m;
bit f;

// Measure a Z component directly with syndrome ancilla as control.
gate meas_z_component d, a {
    cz a, d;
}

// Measure an X component directly with syndrome ancilla as control.
gate meas_x_component d, a {
    cx a, d;
}

// ------------------------------------------------------------
// Flagged syndrome extraction for G0 = X Z Z X I
// ------------------------------------------------------------

reset syn;
h syn;               // prepare |+> syndrome ancilla
reset flag;

// a: X on q1
meas_x_component data[0], syn;

// couple syndrome ancilla to flag
cx syn, flag;

// b: Z on q2
meas_z_component data[1], syn;

// c: Z on q3
meas_z_component data[2], syn;

// couple syndrome ancilla to flag
cx syn, flag;

// d: X on q4
meas_x_component data[3], syn;

// syndrome X-basis measurement
h syn;               // rotate X basis to Z basis for measurement
m = measure syn;

// flag Z-basis measurement
f = measure flag;
