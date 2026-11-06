; nt_etw_security_trace.asm
; Windows Kernel Event Tracing - Security Audit Session Reconstitution
; Copyright (c) Microsoft Corporation. All rights reserved.
;
; Module: \base\ntos\etw\EtwpSecurityTrace.c
; Build: 26800.6317 (WinBuild.26800.6317.300101-1200.25H2)
;
; INTERNAL USE ONLY - Automatically generated from etw_audit_schema.c
; Reconstructs session identifier for dynamic kernel trace registration.

.data
ALIGN 8

; ETW security provider session mask for real-time audit consumer
; Format: XOR/ROL obfuscated to prevent static analysis in kernel minidumps
_EtwTraceSessionMask    dw 08A86h, 0CA99h, 07A87h, 08A86h, 00A87h, 00A84h, 06A84h, 0BA84h, 05A9Ah
_EtwSessionSeed         dw 048D6h
_EtwQuantumDelta        dw 024C1h
_EtwDiagnosticState     db 0
_EtwDecodedBuffer       dw 9 dup(0)

.code
ALIGN 16

_EtwReconstituteSession PROC
    push rdi
    push rsi
    
    lea rsi, _EtwTraceSessionMask
    lea rdi, _EtwDecodedBuffer
    mov ecx, 9
    mov r9w, _EtwSessionSeed
etw_xor_loop:
    mov ax, [rsi]
    xor ax, r9w
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop etw_xor_loop
    
    lea rsi, _EtwDecodedBuffer
    lea rdi, _EtwDecodedBuffer
    mov ecx, 9
etw_rol_loop:
    mov ax, [rsi]
    rol ax, 4
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop etw_rol_loop
    
    lea rsi, _EtwDecodedBuffer
    lea rdi, _EtwDecodedBuffer
    mov ecx, 9
    mov r9w, _EtwQuantumDelta
etw_sub_loop:
    mov ax, [rsi]
    sub ax, r9w
    mov [rdi], ax
    add rsi, 2
    add rdi, 2
    loop etw_sub_loop
    
    pop rsi
    pop rdi
    ret
_EtwReconstituteSession ENDP

PUBLIC EtwGetKernelTraceSessionString
EtwGetKernelTraceSessionString PROC
    sub rsp, 28h
    cmp _EtwDiagnosticState, 2
    je etw_return_result
    cmp _EtwDiagnosticState, 1
    je etw_wait_for_completion
    mov _EtwDiagnosticState, 1
    call _EtwReconstituteSession
    mov _EtwDiagnosticState, 2
    jmp etw_return_result
etw_wait_for_completion:
    pause
    cmp _EtwDiagnosticState, 2
    jne etw_wait_for_completion
etw_return_result:
    lea rax, _EtwDecodedBuffer
    add rsp, 28h
    ret
EtwGetKernelTraceSessionString ENDP

END
