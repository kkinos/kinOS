bits 64
section .text

%macro define_syscall 2
global Syscall%1
Syscall%1:
    mov rax, %2
    mov r10, rcx
    syscall
    ret
%endmacro

define_syscall LogString,           0x80000000
define_syscall PutString,           0x80000001
define_syscall Exit,                0x80000002
define_syscall GetCurrentTick,      0x80000003
define_syscall CreateTimer,         0x80000004
define_syscall OpenFile,            0x80000005
define_syscall ReadFile,            0x80000006
define_syscall DemandPages,         0x80000007
define_syscall MapFile,             0x80000008
define_syscall CreateNewTask,       0x80000009
define_syscall CopyToTaskBuffer,    0x8000000a
define_syscall SetArgument,         0x8000000b
define_syscall FindServer,          0x8000000c
define_syscall OpenReceiveMessage,  0x8000000d
define_syscall ClosedReceiveMessage,0x8000000e
define_syscall SendMessage,         0x8000000f
define_syscall WritePixel,          0x80000010
define_syscall GetFrameBufferWidth, 0x80000011
define_syscall GetFrameBufferHeight,0x80000012
define_syscall CopyToFrameBuffer,   0x80000013
define_syscall ReadVolumeImage,     0x80000014
define_syscall ReadKernelLog,       0x80000015
define_syscall WriteKernelLog,      0x80000016







