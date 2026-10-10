/* TLSR8258 retains the lower 32 KiB of SRAM. Reject oversized retained state. */
ASSERT(_end_bss_ < 0x848000, "TLSR8258 retention RAM overflow")
