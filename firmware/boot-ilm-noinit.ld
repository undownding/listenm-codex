/* Early interrupt vectors are populated at runtime and must remain in ILM. */
.boot_ilm_noinit (NOLOAD) : ALIGN(512) {
    KEEP(*(.ilm.noinit))
    KEEP(*(.ilm.noinit.*))
} > ILM
