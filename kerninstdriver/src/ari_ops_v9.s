.seg "text"
	.align 4

	.global ari_get_tick_raw
	.global ari_get_stick_raw
	.global ari_get_pcr_raw
	.global ari_set_pcr_raw
	.global ari_get_pic_raw
	.global ari_set_pic_raw
	.global ari_disable_tick_protection
        .global ari_raise_pil
        .global ari_set_pil

	/* the .type...#function stuff is needed or else they won't appear as
		type STT_FUNC in the symbol table! */
	.type ari_get_tick_raw,#function
	.type ari_get_stick_raw,#function
	.type ari_get_pcr_raw,#function
	.type ari_set_pcr_raw,#function
	.type ari_disable_tick_protection,#function
        .type ari_raise_pil,#function
        .type ari_set_pil,#function
	
ari_get_tick_raw:
	rd %tick, %o0
	retl
	nop

ari_get_stick_raw:
	rd %asr24, %o0
	retl
	nop

ari_get_pcr_raw:
	/* PCR register is %asr16 */
	rd %asr16, %o0
	retl
	nop

ari_set_pcr_raw:
	/* input is in o0 */
	wr %g0, %o0, %asr16
	retl
	nop

ari_get_pic_raw:
	/* PIC register is %asr17 */
	rd %asr17, %o0
	retl
	nop

ari_set_pic_raw:
	/* inputs are in o0 and o1 due to sparcv8plus abi convention */
	wr %g0, %o0, %asr17
	retl
	nop

ari_disable_tick_protection:
	/* Note that on a multiprocessor, this would have to be issued on 
	   each cpu */
	/* check the high bit (bit 63).  If it's zero then protection is
	   already turned off */
	rdpr %tick, %o0
	srlx %o0, 63, %o0
	cmp %o0, 0
	be 1f
	nop
	/* Okay, bit 63 was 1.  Let's clear it.  Remember that the wrpr
	   instruction is an XOR operation.  We'll use %o1 as the mask. */
	add %g0, 1, %o1
	sllx %o1, 63, %o1
	/* Okay, now %o1 equals 0x800000000000000. So doing %tick = %o0 xor %o1
	   will toggle the high bit of %tick and leave the rest alone, which 
	   is what we want.  Notice the race condition, however: %o0 holds 
	   the old value of %tick (just a few cycles old, hopefully) */
	rd %tick, %o0
	wrpr %o0, %o1, %tick   /* %tick <- %o0 xor %o1 */
	/* Note the race condition (by now, o0's tick value is a bit
	   behind schedule, so time can go backwards!) */
1:	retl
	nop

ari_raise_pil:
        /* input (level to set to) is in %o0 */
        rdpr    %pil, %o1              /* read current level */ 
        cmp     %o1, %o0               /* is PIL higher than desired level? */
        bge     2f                     /* if so, return */
        nop
        wrpr    %g0, %o0, %pil         /* set new level */
2:
        retl
        mov     %o1, %o0               /* return old level */

ari_set_pil:
        /* input (level to set to) is in %o0 */
        wrpr    %g0, %o0, %pil         /* set new level */
        retl
        nop
