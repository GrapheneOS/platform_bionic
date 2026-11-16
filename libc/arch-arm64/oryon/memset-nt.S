/* Copyright (c) 2012, Linaro Limited
   All rights reserved.
   Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
       * Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.
       * Neither the name of the Linaro nor the
         names of its contributors may be used to endorse or promote products
         derived from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/* Assumptions:
 *
 * ARMv8-a, AArch64
 * Unaligned accesses
 *
 */
#include <private/bionic_asm.h>

#define dstin		x0
#define val		w1
#define count		x2
#define tmp1		x3
#define tmp1w		w3
#define tmp2		x4
#define tmp2w		w4
#define zva_len_x	x5
#define zva_len		w5
#define zva_bits_x	x6
#define A_l		x7
#define A_lw		w7
#define dst		x8
#define tmp3		x9
#define tmp3w		w9
#define tmp4            x10

ENTRY(__memset_aarch64_nt)
    mov    dst, dstin		/* Preserve return value.  */
    and    A_lw, val, #255
    orr    A_lw, A_lw, A_lw, lsl #8
    orr    A_lw, A_lw, A_lw, lsl #16
    orr    A_l, A_l, A_l, lsl #32
.Ltail_maybe_long:
    cmp    count, #64
    b.hs   .LisValZero		/* Case : Size greater than or equal to 64 bytes */
.Ltail_maybe_tiny:
    cmp    count, #15
    b.ls   .Ltail15tiny		/* Case : Size is less than or equal to 15 bytes */
.Ltail63:				/* Size is between 16 to 63 bytes */

/* Size 16 - 63. Do 4x16 bytes wrire. Based on the size, adjust the offsets to cover full range of 16 - 63 bytes */
    add    tmp4, dst, count
    mov    tmp1, 16
    and    tmp1, tmp1, count, lsr 1
    sub    tmp2, tmp4, tmp1
    add    tmp3, dst, tmp1

/* 	1st Store : First 16 bytes
 *	2nd Store : For size is less than 32, first 16 bytes. For size greater than 32,17-32 bytes.
 *	3rd Store : For size is less than 32, last 16 bytes. For size greater than 32, write 16 bytes from address:[last address-32].
 *	4th Store : Last 16 bytes
 */
    stp    A_l, A_l, [dst]
    stp    A_l, A_l, [tmp3]
    stp    A_l, A_l, [tmp2, -16]
    stp    A_l, A_l, [tmp4, -16]
    ret
.Ltail15tiny:				/* Size is less than or equal to 15 bytes */
/* Size < 16. Do 4x4 bytes wrire. Based on the size, adjust the offsets to cover full range of 1-15 bytes */
    cmp    count, 4			/* Check for Size < 4 bytes */
    b.lo   .tiny_4B			/* Case : Size is less than 4 bytes */
    add    tmp4, dst, count
    lsr    tmp1, count, 3
    sub    tmp2, tmp4, tmp1, lsl 2
    str    A_lw, [dst]
    str    A_lw, [dst, tmp1, lsl 2]
    str    A_lw, [tmp2, -4]
    str    A_lw, [tmp4, -4]
    ret
.tiny_4B:				/* Size is less 4 bytes */
    cbz    count, .tiny_end		/* Check if size is 0 */
    add    tmp4, dst, count
    lsr    tmp1, count, 1
    strb   A_lw, [dst]
    strb   A_lw, [dst, tmp1]
    strb   A_lw, [tmp4, -1]
.tiny_end:
    ret
    /* Critical loop.  Start at a new cache line boundary.  Assuming
     * 64 bytes per line, this ensures the entire loop is in one line.  */
    .p2align 6
.LisValZero:
    cbnz   A_lw, .Lnot_short
    /* For value = 0, if size is greater than BUF_SIZE_FOR_ZVA bytes,
     * use DC ZVA to set 0, else use STP instructions to set 0 	*/
    cmp    count, #512			/* For size greater than 512 bytes, use DC ZVA, it is better than STP */
    b.hi   .Lzero_mem
.Lnot_short:
    cmp    count, #98304
    /* Use non-temporal instruction if count > 98304 (96KB) . Non Temporal Store is beneficial is size is greater that 96 KB*/
    b.hi   .Lnot_short_nt
    neg    tmp2, dst
    ands   tmp2, tmp2, #15
    b.eq   2f
    /* Bring DST to 128-bit (16-byte) alignment.  We know that there's
     * more than that to set, so we simply store 16 bytes and advance by
     * the amount required to reach alignment.  */
    sub    count, count, tmp2
    stp    A_l, A_l, [dst]
    add    dst, dst, tmp2
    /* There may be less than 63 bytes to go now.  */
    cmp    count, #63
    b.ls   .Ltail_maybe_tiny
2:
    sub    dst, dst, #16		/* Pre-bias.  */
    sub    count, count, #64
1:
    stp    A_l, A_l, [dst, #16]
    stp    A_l, A_l, [dst, #32]
    stp    A_l, A_l, [dst, #48]
    stp    A_l, A_l, [dst, #64]
    add    dst, dst, #64
    subs   count, count, #64
    b.hs   1b
    add    dst, dst, #16
    add    count, count, #64
    cbnz   count, .Ltail_maybe_tiny
    ret
.Lnot_short_nt:
    neg    tmp2, dst
    ands   tmp2, tmp2, #15
    b.eq   2f
    /* Bring DST to 128-bit (16-byte) alignment.  We know that there's
     * more than that to set, so we simply store 16 bytes and advance by
     * the amount required to reach alignment.  */
    sub    count, count, tmp2
    stnp   A_l, A_l, [dst]
    add    dst, dst, tmp2
    /* There may be less than 63 bytes to go now.  */
    cmp    count, #63
    b.ls   .Ltail_maybe_tiny
2:
    sub    dst, dst, #16		/* Pre-bias.  */
    sub    count, count, #64
1:
    stnp   A_l, A_l, [dst, #16]
    stnp   A_l, A_l, [dst, #32]
    stnp   A_l, A_l, [dst, #48]
    stnp   A_l, A_l, [dst, #64]
    add    dst, dst, #64
    subs   count, count, #64
    b.hs   1b
    add    dst, dst, #16
    add    count, count, #64			/* Count is negative now as it is pre decreament in 2: Revert it to have exact count before branching to Ltail_maybe_tiny */
    cbnz   count, .Ltail_maybe_tiny
    ret
.Lzero_mem:
    mov    A_l, #0
    neg    tmp2, dst
    ands   tmp2, tmp2, #15
    b.eq   1f
    sub    count, count, tmp2
    stp    A_l, A_l, [dst]
    add    dst, dst, tmp2
    cmp    count, #63
    b.ls   .Ltail_maybe_tiny
1:
    /* For zeroing small amounts of memory, it's not worth setting up
     * the line-clear code.  */
    mrs    tmp1, dczid_el0
    tbnz   tmp1, #4, .Lnot_short
    mov    tmp3w, #4
    and    zva_len, tmp1w, #15	/* Safety: other bits reserved.  */
    lsl    zva_len, tmp3w, zva_len
.Lzero_by_line:
    /* Compute how far we need to go to become suitably aligned.  We're
     * already at quad-word alignment.  */
    cmp    count, zva_len_x
    b.lo   .Lnot_short		/* Not enough to reach alignment.  */
    sub	   zva_bits_x, zva_len_x, #1
    neg    tmp2, dst
    ands   tmp2, tmp2, zva_bits_x
    b.eq   1f			/* Already aligned.  */
    /* Not aligned, check that there's enough to copy after alignment.  */
    sub    tmp1, count, tmp2
    cmp    tmp1, #64
    ccmp   tmp1, zva_len_x, #0, hs	/* NZCV=0b0000 */
    b.lo   .Lnot_short
    /* We know that there's at least 64 bytes to zero and that it's safe
     * to overrun by 64 bytes.  */
    mov    count, tmp1
2:
    stp    A_l, A_l, [dst]
    stp    A_l, A_l, [dst, #16]
    stp    A_l, A_l, [dst, #32]
    subs   tmp2, tmp2, #64
    stp    A_l, A_l, [dst, #48]
    add    dst, dst, #64
    b.ge   2b
    /* We've overrun a bit, so adjust dst downwards.  */
    add    dst, dst, tmp2
1:
    sub    count, count, zva_len_x
3:
    dc     zva, dst
    add    dst, dst, zva_len_x
    subs   count, count, zva_len_x
    b.hs   3b
    ands   count, count, zva_bits_x
    b.ne   .Ltail_maybe_long
    ret
END(__memset_aarch64_nt)

NOTE_GNU_PROPERTY()
