	.seg "text"
	.align 4

	.global kerninst_reserve_cell
	.global kerninst_chain_cell
	.global kerninst_inc_call_count
	/* the .type...#function stuff is needed or else they won't appear as
		type STT_FUNC in the symbol table! */
	.type kerninst_reserve_cell, #function

kerninst_reserve_cell:
	/* (unsigned *)&free_pos is in %o0, NUM_TOTAL is in %o1 */
	ld	[%o0], %o2
1:		
	add	%o2, 1, %o3

	cmp	%o3, %o1
	bge,a,pn %icc, 2f
	mov	%g0, %o3
				
	cas	[%o0], %o2, %o3
	cmp	%o3, %o2
	bne,a,pn %icc, 1b			
	mov	%o3, %o2
2:		
	retl		
	mov	%o3, %o0

kerninst_inc_call_count:	
	/* (unsigned *)&call_count is in %o0 */
	ld	[%o0], %o2
3:		
	add	%o2, 1, %o3
	cas	[%o0], %o2, %o3
	cmp	%o3, %o2
	bne,a,pn %icc, 3b			
	mov	%o3, %o2
	retl
	nop		
	
kerninst_chain_cell:
	/* (kptr_t*)&current->next is in %o0, (kptr_t*)&new->next is in %o1,
	   (kptr_t*)&new is in %o2 */
	ldx	[%o0], %o3
4:		
	stx	%o3, [%o1]
	mov	%o2, %o4
	casx	[%o0], %o3, %o4
	cmp	%o3, %o4
	bne,a,pn %icc, 4b
	mov	%o4, %o3
	retl
	nop
	
