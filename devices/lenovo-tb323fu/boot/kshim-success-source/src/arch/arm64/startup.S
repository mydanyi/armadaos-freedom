#include <asm/asm_macro.h>
#include <arm64_sysreg.h>
#include <ld/arm64_ld.h>

.section .text.boot
.align 3

/* entry point */
.global _start

_start:
    b       _platform_init        // Code 1, go to ShimLoader

_LinuxStart:
    nop                 // Code 2
    .quad   0           // Image load offset
    .quad   LD_IMAGE_SIZE // Effective Image size
    .long   0           // Flags L
    .long   0           // Flags H

_StackBase:
    .long   0           // reserved2, Stack Base LWORD
    .long   0           // reserved2, Stack Base HWORD

_StackSize:
    .quad LD_STACK_SIZE // reserved3

_KernelSize:
    .long   0           // reserved4, Kernel Size L
    .long   0           // reserved4, Kernel Size H

    .ascii  "ARM\x64"   // magic_64
    .long   0           // reserve5

_platform_init:
    /* Store linux kernel params */
    mov x19, x0
    mov x20, x1
    mov x21, x2
    mov x22, x3

#if defined(KSHIM_STANDALONE) && KSHIM_STANDALONE
    // A directly loaded standalone image has no base-image manifest metadata.
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
#else
    // The attached Shim is entered by BL at BaseImage + 0x0.
    // LR therefore points to BaseImage + 0x4.
    sub x23, x30, #4
    ldr x24, [x23, #0x20]
    ldr x25, [x23, #0x28]
    add x24, x23, x24
#endif

    /* Clear registers */
    mov x29, xzr // Clean frame

    /* Configure the current EL without changing the bootloader-selected EL. */
    mrs x9, CurrentEL
    cmp x9, #0x8
    b.eq _setup_el2
    cmp x9, #0x4
    b.ne _dead

_setup_el1:
    mrs x0, sctlr_el1
    orr x0, x0, #SCTLR_SPAN_BIT
    msr sctlr_el1, x0
    mrs x0, cpacr_el1
    orr x0, x0, CPACR_FPEN_BIT
    msr cpacr_el1, x0
    isb sy
    b _setup_gots

_setup_el2:
    mrs x0, sctlr_el2
    orr x0, x0, #SCTLR_SPAN_BIT
    msr sctlr_el2, x0
    mrs x0, cptr_el2
    bic x0, x0, #(1 << 10) // CPTR_EL2.TFP
    msr cptr_el2, x0
    isb sy

// Calculate and fix got address
_setup_gots:
    adrp x0, _start         // load address of program
    add x0, x0, :lo12:_start
    mov x14, x0             // save start address to x14
    adrp x1, __gots_start__ // offset of gots start
    add x1, x1, :lo12:__gots_start__
    adrp x2, __gots_end__   // offset of gots end
    add x2, x2, :lo12:__gots_end__
    sub x2, x2, x1          // size of gots (x2 -x1)

    // check gots size
    cbz x2, _setup_bss      // BSS is required even when no GOT is emitted

    // Pre sub size
    subs x2, x2, #0x8       // 8 bytes per got entry

_setup_gots_loop:
    ldr x3, [x1]        // load got entry
    add x3, x3, x0      // add load address
    str x3, [x1], #0x8  // store back to got entry
    subs x2, x2, #0x8   // 8 bytes per got entry
    b.cs _setup_gots_loop    // x2 >= 0, continue loop

_setup_bss:
    /* Clear bss */
    adrp x0, __bss_start__
    add x0, x0, :lo12:__bss_start__
    adrp x1, __bss_end__
    add x1, x1, :lo12:__bss_end__
    sub x1, x1, x0              // size of bss (x1 -x0)
    cbz x1, _setup_stack       // go over if bss size is 0
    bl _aligned_mem64_set_zero_simd

_setup_stack:
    /* Clear Stack */
    adrp x0, __stack_base__     // stack base in memory
    add x0, x0, :lo12:__stack_base__ 
    adr x1, _StackSize          // use adr here because _StackSize can never be far from here.
    ldr x1, [x1]                // stack size
    add x2, x0, x1              // stack top

    // check stack base and size
    cbz x1, _dead // Invalid stack size

    // clear stack
    bl _aligned_mem64_set_zero_simd

    /* store stack top to sp */
    add sp, x2, xzr

_setup_relocation:
    /* Relocation */
    // Rela
    mov  x0, #0                     // old_image_start
    mov  x1, x14                    // new_image_start
    stp  x0, x1, [sp, #-0x10]!      // push within the reserved stack
    adrp x2, __reladyn_start__      // rela start address
    add  x2, x2, :lo12:__reladyn_start__
    adrp x3, __reladyn_end__        // rela end address
    add  x3, x3, :lo12:__reladyn_end__
    bl do_relocation_rela           // call relocation function

    // Rel
    ldp x0, x1, [sp], #0x10         // pop old_image_start and new_image_start
    adrp x2, __reldyn_start__       // rela start address
    add  x2, x2, :lo12:__reldyn_start__
    adrp x3, __reldyn_end__         // rela end address
    add  x3, x3, :lo12:__reldyn_end__
    bl do_relocation_rel            // call relocation function

_start_kshim:
    /* Publish the address this image is actually running from.  The attached
     * shim is appended to the base image and entered there, so it never runs
     * at its link address; every MMU mapping must be derived from this. */
    adrp x9, _start
    add  x9, x9, :lo12:_start
    adrp x10, __image_runtime_base__
    add  x10, x10, :lo12:__image_runtime_base__
    str  x9, [x10]

    mov  x0, x19
    mov  x1, x20
    mov  x2, x21
    mov  x3, x22
    mov  x4, x23
    mov  x5, x24
    mov  x6, x25
    adrp x7, kshim_main
    add  x7, x7, :lo12:kshim_main
    br   x7

.global arm64_boot_linux
arm64_boot_linux:
    mov  x8, x0
    mov  x10, x2
    mov  x0, x1
    mov  x1, xzr
    mov  x2, xzr
    mov  x3, xzr
    msr  daifset, #0xf
    mrs  x9, cntp_ctl_el0
    bic  x9, x9, #1
    msr  cntp_ctl_el0, x9
    dsb  sy
    isb
    add  x8, x8, x10
    br   x8

.global arm64_boot_image
arm64_boot_image:
    mov  x8, x0
    mov  x0, x1
    mov  x1, x2
    mov  x2, x3
    mov  x3, x4
    msr  daifset, #0xf
    mrs  x9, cntp_ctl_el0
    bic  x9, x9, #1
    msr  cntp_ctl_el0, x9
    dsb  sy
    isb
    br   x8

/* Should never get here */
_dead:
    b .

// x0: start
// x1: size
_aligned_mem64_set_zero_simd:
    // Clear vfp registers
    movi v3.2d, #0
    mov v4.16b, v3.16b
    mov v5.16b, v3.16b
    mov v6.16b, v3.16b
    subs x1, x1, #0x40
_aligned_mem64_set_zero_simd_loop:
    /* Use SIMD to clear bss*/
    st4 {v3.2d, v4.2d, v5.2d, v6.2d}, [X0], #0x40
    subs x1, x1, #0x40
    // stp xzr, xzr, [x0], #0x20 // store 64 bytes
    // subs x1, x1, #0x20
    b.hs _aligned_mem64_set_zero_simd_loop // x1 >= 0, continue loop
_ret:
    ret

#if defined(KSHIM_DIAGNOSTIC_MEMORY_ONLY) && KSHIM_DIAGNOSTIC_MEMORY_ONLY
	.section .text.arm64_diag_head_tail,"ax",%progbits
	.align	2
	.global	arm64_diag_head_tail
	.type	arm64_diag_head_tail, %function
arm64_diag_head_tail:
	/* x0 = kernel_base, x1 = dtb(Linux), x2 = android_entry,
	 * x3 = android_dtb, x4 = flag_address (single DT byte).
	 * Branches into the PATCHED Linux Image entry (x8); the pinned kernel
	 * prefix runs the early kernel through start_kernel and setup_arch to
	 * the inline 128-byte M30 stub, which restores the caller's x25/x26
	 * from SP+224 and x27/x28 from SP+240 (the start_kernel 144-byte frame
	 * plus the setup_arch 160-byte frame) and branches to the local
	 * callback via 'br x27'.  This is not the completed C-call restore used
	 * in M28. */
	mov	x28, x2
	mov	x26, x3
	mov	x25, x4
	adr	x27, .Lm30_callback
	mov	x8, x0
	mov	x0, x1
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	msr	daifset, #0xf
	mrs	x9, cntp_ctl_el0
	bic	x9, x9, #1
	msr	cntp_ctl_el0, x9
	dsb	sy
	isb
	br	x8
.Lm30_callback:
	/* Reached via the patched Image entry (br x27).
	 * x24 holds the full captured SCTLR_EL1 when count > 0 and total > 0
	 * and the current kernel PC is a high VA (bit 63 set), else 0.
	 * x21/x22 hold the copied memblock count/total bytes.  The callback
	 * tests bit 0 (MMU)==1; SCTLR can be nonzero even when M == 0.  The MMU
	 * is already off here; the callback itself performs no stack access and
	 * does not return to the old C context.
	 * First publish the 64-bit count (x21) and total (x22) as 16 lowercase
	 * hex chars each into the DT report value at [x25+4] and [x25+23] using
	 * two small inline nibble loops (no stack, no calls, no RET; only
	 * x9..x15 are clobbered).  Then flip the one DT flag byte only on
	 * success, and hand a valid Linux EL1 / MMU-off / DAIF-masked state to
	 * Android (x0 = android_dtb, x1..x3 = 0).  This does NOT restore the old
	 * C context and does NOT prove that full Linux booted. */
	mov	x9, #60
	add	x10, x25, #4
.Lm30_hexcnt:
	lsrv	x11, x21, x9
	and	x11, x11, #15
	cmp	x11, #10
	add	x11, x11, #48
	b.lt	.Lm30_hexcnt_lo
	add	x11, x11, #39
.Lm30_hexcnt_lo:
	strb	w11, [x10], #1
	subs	x9, x9, #4
	b.ge	.Lm30_hexcnt
	mov	x9, #60
	add	x10, x25, #23
.Lm30_hextot:
	lsrv	x11, x22, x9
	and	x11, x11, #15
	cmp	x11, #10
	add	x11, x11, #48
	b.lt	.Lm30_hextot_lo
	add	x11, x11, #39
.Lm30_hextot_lo:
	strb	w11, [x10], #1
	subs	x9, x9, #4
	b.ge	.Lm30_hextot
	tbz	x24, #0, .Lm30_android
	mov	w9, #49
	strb	w9, [x25]
.Lm30_android:
	dsb	sy
	mov	x8, x28
	mov	x0, x26
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	msr	daifset, #0xf
	mrs	x9, cntp_ctl_el0
	bic	x9, x9, #1
	msr	cntp_ctl_el0, x9
	dsb	sy
	isb
	br	x8
	.size	arm64_diag_head_tail, .-arm64_diag_head_tail
#endif
