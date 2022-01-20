.seg "text"
	.align 4
	.global get_popc
	.global get_ffs

get_popc:
	/* Input param:	register in %o0
           Result is written to %o0, too */
	retl
	popc %o0, %o0
