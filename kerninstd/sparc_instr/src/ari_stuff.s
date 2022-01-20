.seg "text"
	.align 4

	.global ull_identity

	/* the .type...#function stuff is needed or else they won't appear as
		type STT_FUNC in the symbol table! */
	.type ull_identity,#function


ull_identity:	
	/* input args in o0 and o1 */
	/* under the assumption that the g++/egcs compiler still puts unsigned long long
	   values into 2 regs, let's stress this assumption by clearing the high
	   32 bits of o0 and o1 (which should already be clear) */
	sllx %o0, 32, %o0
	sllx %o1, 32, %o1
	srlx %o0, 32, %o0
	srlx %o1, 32, %o1
	retl
	nop
