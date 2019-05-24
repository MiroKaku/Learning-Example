include ksamd64.inc
EXTERN InstrumentationCallback:NEAR

.CODE

InstrumentationCallbackShim PROC
	
	; https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention

	push rax
	push rcx
	push RBX
	push RBP
	push RDI
	push RSI
	push RSP
	push R12
	push R13
	push R14
	push R15 

	; homespace (rcx, rdx, r8, r9)
	sub rsp, 20h
	mov rdx, rax ; return value
	mov rcx, r10 ; return address
	call InstrumentationCallback
	add rsp, 20h

	pop R15 
	pop R14
	pop R13
	pop R12
	pop RSP
	pop RSI
	pop RDI
	pop RBP
	pop RBX
	pop rcx
	pop rax

    jmp R10
InstrumentationCallbackShim ENDP
 
END