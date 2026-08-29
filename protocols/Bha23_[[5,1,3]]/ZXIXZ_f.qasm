OPENQASM 3.0;
include "stdgates.inc";

// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn  = syndrome ancilla, initialized to |+>
// flag = flag ancilla, initialized to |+>
//
// m = syndrome bit
// f = flag bit
//
// Basis-transformed flag convention:
//   syndrome ancilla: |+>, X-basis measurement
//   flag ancilla:     |0>, Z-basis measurement
//   Z data component: CZ(ancilla, data)
//   X data component: CX(ancilla -> data)
//   Y data component: CY(ancilla -> data)
//   flag-syndrome coupling: CX(syndrome -> flag).
// The syndrome line is Hadamard-transformed, and the flag line is also
// Hadamard-transformed: H_flag CZ(syn,flag) H_flag = CX(syn -> flag).

qubit[5] data;
qubit syn;
qubit flag;

bit m;
bit f;

// Measure a Z component using phase kickback with syn in |+>:
// CZ couples the Z component to the syndrome ancilla.
gate meas_z_component d, a {
    cz d, a;
}

// Measure an X component directly.
// Equivalent to H_d; CZ(d,a); H_d, but written as CX(ancilla -> data).
gate meas_x_component d, a {
    cx a, d;
}

// ------------------------------------------------------------
// Flagged syndrome extraction for G3 = Z X I X Z
// ------------------------------------------------------------

reset syn;
h syn;               // prepare |+> syndrome ancilla
reset flag;

// a: Z on q1
meas_z_component data[0], syn;

// couple syndrome ancilla to flag in the doubly transformed basis
cx syn, flag;

// b: X on q2
meas_x_component data[1], syn;

// c: X on q4
meas_x_component data[3], syn;

// couple syndrome ancilla to flag in the doubly transformed basis
cx syn, flag;

// d: Z on q5
meas_z_component data[4], syn;

// syndrome X-basis measurement
h syn;               // rotate X basis to Z basis for measurement
m = measure syn;

// flag Z-basis measurement
f = measure flag;
