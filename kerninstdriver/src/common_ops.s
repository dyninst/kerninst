	.seg "text"
	.align 4


	.global flush_addr_from_dcache
	.global get_disp_level
	.global get_cpu_impl_num
	/* the .type...#function stuff is needed or else they won't appear as
		type STT_FUNC in the symbol table! */
	.type flush_addr_from_dcache, #function
	.type get_disp_level, #function
	.type get_cpu_impl_num, #function

flush_addr_from_dcache:
	/* input is in o0 */
	sll	%o0, 18, %o0 /* not sllx -- to zero out high 32 bits */
	srl	%o0, 23, %o0
	sll	%o0, 5, %o0
	stxa    %g0, [%o0] 0x47 /* ASI_DCACHE_TAG */
	membar #Sync
	retl
	nop
get_disp_level:
	save	%sp, -0xb0, %sp /* not a leaf function, calls splhi() */
	call	splhi
	rdpr	%pil, %i1 /* delay slot: save current PIL */
	rdpr	%pil, %i0 /* the value to return */
	wrpr	%i1, %pil /* restore original PIL */
	ret
	restore
get_cpu_impl_num:
	/* read the version register (to tell ultra-3 from ultra-2, ...) */
	rdpr	%ver, %o0
	sllx	%o0, 16, %o0 /* zero-out the highest 16 bits */
	retl
	srlx	%o0, 48, %o0 /* return the next 16 bits */

