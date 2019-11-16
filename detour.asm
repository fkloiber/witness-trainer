push rax
push rbx
push rcx
push rdx
push rdi

mov rdi, [rip + GetPlayer]
call rdi
mov rdi, [rip + Read]
mov ebx, [rax + 0x1]
mov ecx, [rax + 0x1]
mov edx, [rax + 0x1]
mov [rdi + 0x1], ebx
mov [rdi + 0x1], ecx
mov [rdi + 0x1], edx

mov rbx, [rip + Theta]
mov rcx, [rip + Phi]
mov ebx, [rbx]
mov ecx, [rcx]
mov [rdi + 0x1], ebx
mov [rdi + 0x1], ecx

mov rdi, [rip + Write]
xor rdx, rdx
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz Exit
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz SkipX
mov ebx, [rdi + 0x1]
mov [rax + 0x1], ebx
SkipX:
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz SkipY
mov ebx, [rdi + 0x1]
mov [rax + 0x1], ebx
SkipY:
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz SkipZ
mov ebx, [rdi + 0x1]
mov [rax + 0x1], ebx
SkipZ:
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz SkipTheta
mov rcx, [rip + Theta]
mov ebx, [rdi + 0x1]
mov [rcx], ebx
SkipTheta:
cmp byte ptr [rdi + 0x1], 0
mov [rdi + 0x1], dl
jz SkipPhi
mov rcx, [rip + Phi]
mov ebx, [rdi + 0x1]
mov [rcx], ebx
SkipPhi:

Exit:
pop rdi
pop rdx
pop rcx
pop rbx
pop rax
jmp [rip+0]
.quad 0xcccccccccccccccc

GetPlayer:
.quad 0xcccccccccccccccc
Theta:
.quad 0xcccccccccccccccc
Phi:
.quad 0xcccccccccccccccc
Read:
.quad 0xcccccccccccccccc
Write:
.quad 0xcccccccccccccccc
