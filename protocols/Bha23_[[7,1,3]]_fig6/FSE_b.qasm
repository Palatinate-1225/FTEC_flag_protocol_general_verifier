OPENQASM 3.0;
include "stdgates.inc";

// Bhatnagar et al. Steane [[7,1,3]] Fig. 5
// Full bare (unflagged) syndrome extraction for all six standard generators.
// qd[0..6] = data qubits.
// qm[0..5] = syndrome ancillas for:
//   qm[0]: IIIXXXX
//   qm[1]: IXXIIXX
//   qm[2]: XIXIXIX
//   qm[3]: IIIZZZZ
//   qm[4]: IZZIIZZ
//   qm[5]: ZIZIZIZ
// Each syndrome ancilla is prepared in |+> and read in the X basis: the
// final H rotates it back, so a plain Z-basis measurement reads it.

qubit[7] qd;
qubit[6] qm;
bit[6] cm;

// Prepare all syndrome ancillas in |+>.
h qm[0];
h qm[1];
h qm[2];
h qm[3];
h qm[4];
h qm[5];

// -----------------------------------------------------------------------------
// IIIXXXX  -> qm[0]
// -----------------------------------------------------------------------------
cx qm[0], qd[3];
cx qm[0], qd[4];
cx qm[0], qd[5];
cx qm[0], qd[6];

barrier qd, qm;  // separate generators

// -----------------------------------------------------------------------------
// IXXIIXX  -> qm[1]
// -----------------------------------------------------------------------------
cx qm[1], qd[1];
cx qm[1], qd[2];
cx qm[1], qd[5];
cx qm[1], qd[6];

barrier qd, qm;  // separate generators

// -----------------------------------------------------------------------------
// XIXIXIX  -> qm[2]
// -----------------------------------------------------------------------------
cx qm[2], qd[0];
cx qm[2], qd[2];
cx qm[2], qd[4];
cx qm[2], qd[6];

barrier qd, qm;  // separate generators

// -----------------------------------------------------------------------------
// IIIZZZZ  -> qm[3]
// -----------------------------------------------------------------------------
cz qm[3], qd[3];
cz qm[3], qd[4];
cz qm[3], qd[5];
cz qm[3], qd[6];

barrier qd, qm;  // separate generators

// -----------------------------------------------------------------------------
// IZZIIZZ  -> qm[4]
// -----------------------------------------------------------------------------
cz qm[4], qd[1];
cz qm[4], qd[2];
cz qm[4], qd[5];
cz qm[4], qd[6];

barrier qd, qm;  // separate generators

// -----------------------------------------------------------------------------
// ZIZIZIZ  -> qm[5]
// -----------------------------------------------------------------------------
cz qm[5], qd[0];
cz qm[5], qd[2];
cz qm[5], qd[4];
cz qm[5], qd[6];

barrier qd, qm;  // separate generators

// Rotate syndrome ancillas back for X-basis readout.
h qm[0];
h qm[1];
h qm[2];
h qm[3];
h qm[4];
h qm[5];

// Syndrome readout: the final H above rotated the X-basis value into
// the Z basis, so these are plain measurements.
cm[0] = measure qm[0];
cm[1] = measure qm[1];
cm[2] = measure qm[2];
cm[3] = measure qm[3];
cm[4] = measure qm[4];
cm[5] = measure qm[5];
