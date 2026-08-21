#ifndef QPSX_LAYOUT_PAD_BYTES
#define QPSX_LAYOUT_PAD_BYTES 0
#endif

/*
 * Keep QPSX cache-colouring experiments independent of diagnostic code.
 * The section is linked immediately before the adapter/core text and is
 * never executed; changing its size changes only downstream virtual
 * addresses.  The link uses -u on the anchor so --gc-sections retains it.
 */
    .section .text.sf2000_qpsx_layout_pad,"ax",@progbits
    .balign 4
    .globl sf2000_qpsx_layout_pad_start
    .type sf2000_qpsx_layout_pad_start, @function
sf2000_qpsx_layout_pad_start:
    .space QPSX_LAYOUT_PAD_BYTES
    .size sf2000_qpsx_layout_pad_start, .-sf2000_qpsx_layout_pad_start

    .section .note.GNU-stack,"",@progbits
