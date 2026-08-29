OPENQASM 3.0;
include "stdgates.inc";

// Unflagged syndrome extraction for G5 = YIYXX.
//
// data[0] = q1, data[1] = q2, data[2] = q3,
// data[3] = q4, data[4] = q5.
//
// syn = syndrome ancilla, initialized to |+>.
// m = syndrome measurement bit.

qubit[5] data;
qubit syn;

bit m;

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

// Measure a Y component directly.
// The previous Sdg-H-CZ-H-S sequence is exactly CY(ancilla -> data).
gate meas_y_component d, a {
    cy a, d;
}


// ------------------------------------------------------------
// G5 = YIYXX
// ------------------------------------------------------------

reset syn;

h syn;               // prepare |+> syndrome ancilla
// Y on q1
meas_y_component data[0], syn;

// Y on q3
meas_y_component data[2], syn;

// X on q4
meas_x_component data[3], syn;

// X on q5
meas_x_component data[4], syn;

// syndrome X-basis measurement
h syn;               // rotate X basis to Z basis for measurement
m = measure syn;
