.data
ALIGN 8
_EncodedData    dw 0769Ah, 0569Ah, 0669Bh, 026A4h, 076A4h, 046A5h, 0B698h, 05698h, 0169Fh
_XorKey         dw 037C5h
_SubConst       dw 15A2h
_State          db 0
_DecodedBuffer  dw 9 dup(0)
.code
ALIGN 16
_Decode PROC
    push rdi
    push rsi
    lea rsi, _EncodedData
    lea rdi, _DecodedBuffer
    mov ecx, 9
    mov r9w, _XorKey
xor_loop:
    mov ax, [rsi]
    xor ax, r9w
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop xor_loop
    lea rsi, _DecodedBuffer
    lea rdi, _DecodedBuffer
    mov ecx, 9
rol_loop:
    mov ax, [rsi]
    rol ax, 4
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop rol_loop
    lea rsi, _DecodedBuffer
    lea rdi, _DecodedBuffer
    mov ecx, 9
    mov r9w, _SubConst
sub_loop:
    mov ax, [rsi]
    sub ax, r9w
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop sub_loop
    pop rsi
    pop rdi
    ret
_Decode ENDP
PUBLIC MmGetPoolDiagnosticString
MmGetPoolDiagnosticString PROC
    sub rsp, 28h
    cmp _State, 2
    je return_result
    cmp _State, 1
    je wait_for_completion
    mov _State, 1
    call _Decode
    mov _State, 2
    jmp return_result
wait_for_completion:
    pause
    cmp _State, 2
    jne wait_for_completion
return_result:
    lea rax, _DecodedBuffer      
    add rsp, 28h
    ret
MmGetPoolDiagnosticString ENDP
END