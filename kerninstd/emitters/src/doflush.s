.seg "text"
	.align 4
	.global flush_a_little
	
flush_a_little:
	/* input param:	 register in o0 contains address */
	retl
	flush %o0
