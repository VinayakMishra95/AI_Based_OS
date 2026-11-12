; Stage 2 Bootloader (loaded at 0x7E00)
; - Enables A20 line
; - Queries VBE for 1024x768x32bpp (fallback 800x600x32bpp) linear framebuffer
; - Saves BootInfo struct at 0x9000
; - Switches to 32-bit Protected Mode and jumps to kernel at 0x10000

[BITS 16]
[ORG 0x7E00]

%define BOOT_INFO_ADDR  0x9000
%define VBE_INFO_BUF    0x9200
%define VBE_MODE_BUF    0x9400
%define KERNEL_ENTRY    0x10000

stage2_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; Enable A20 via Fast A20 Gate (port 0x92)
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    ; Query VBE Controller Info (INT 10h AX=4F00h)
    mov di, VBE_INFO_BUF
    mov dword [di], 'VBE2'
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne vbe_failed

    ; Pointer to video mode list is at VBE_INFO_BUF + 14 (offset, segment)
    mov si, [VBE_INFO_BUF + 14]
    mov ax, [VBE_INFO_BUF + 16]
    mov fs, ax

    ; First try 1024x768x32
    mov word [target_w], 1024
    mov word [target_h], 768
    call find_vbe_mode
    cmp word [found_mode], 0
    jne set_vbe_mode

    ; Fallback to 800x600x32
    mov word [target_w], 800
    mov word [target_h], 600
    mov si, [VBE_INFO_BUF + 14]
    mov ax, [VBE_INFO_BUF + 16]
    mov fs, ax
    call find_vbe_mode
    cmp word [found_mode], 0
    jne set_vbe_mode

vbe_failed:
    mov si, msg_vbe_err
    call print16
.hang:
    hlt
    jmp .hang

; Searches VBE mode list in FS:SI for target_w x target_h x 32bpp with LFB
find_vbe_mode:
    mov word [found_mode], 0
.loop:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je .done
    add si, 2

    push si
    push fs
    mov ax, 0x4F01
    mov di, VBE_MODE_BUF
    int 0x10
    pop fs
    pop si
    cmp ax, 0x004F
    jne .loop

    ; Check attributes: bit 0 (supported) + bit 4 (graphics) + bit 7 (LFB) = 0x91
    mov ax, [VBE_MODE_BUF]
    and ax, 0x0091
    cmp ax, 0x0091
    jne .loop

    ; Check width, height, bpp
    mov ax, [VBE_MODE_BUF + 18]
    cmp ax, [target_w]
    jne .loop

    mov ax, [VBE_MODE_BUF + 20]
    cmp ax, [target_h]
    jne .loop

    mov al, [VBE_MODE_BUF + 25]
    cmp al, 32
    jne .loop

    ; Found matching mode in CX!
    mov [found_mode], cx
.done:
    ret

set_vbe_mode:
    ; Populate BootInfo struct at 0x9000
    mov eax, [VBE_MODE_BUF + 40]        ; physbase (LFB physical address)
    mov [BOOT_INFO_ADDR + 0], eax

    xor eax, eax
    mov ax, [VBE_MODE_BUF + 18]         ; width
    mov [BOOT_INFO_ADDR + 4], eax

    mov ax, [VBE_MODE_BUF + 20]         ; height
    mov [BOOT_INFO_ADDR + 8], eax

    mov ax, [VBE_MODE_BUF + 16]         ; pitch (bytes per scanline)
    mov [BOOT_INFO_ADDR + 12], eax

    xor eax, eax
    mov al, [VBE_MODE_BUF + 25]         ; bpp
    mov [BOOT_INFO_ADDR + 16], eax

    ; Switch to selected VBE mode with Linear Framebuffer bit (0x4000)
    mov ax, 0x4F02
    mov bx, [found_mode]
    or bx, 0x4000
    int 0x10
    cmp ax, 0x004F
    jne vbe_failed

    ; Load 32-bit GDT and enter Protected Mode
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:pm_entry

print16:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print16
.done:
    ret

target_w:   dw 1024
target_h:   dw 768
found_mode: dw 0
msg_vbe_err: db "VBE 32bpp mode not found!", 13, 10, 0

; Temporary 32-bit GDT for Protected Mode switch
align 8
gdt_start:
    dq 0x0000000000000000               ; Null descriptor
    dq 0x00CF9A000000FFFF               ; Code: base=0, limit=4GB, 32-bit, exec/read
    dq 0x00CF92000000FFFF               ; Data: base=0, limit=4GB, 32-bit, read/write
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32]
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000                    ; Stack below 640KB conventional memory

    ; Pass BootInfo pointer (0x9000) to kmain(BootInfo*)
    push dword BOOT_INFO_ADDR
    call KERNEL_ENTRY

.hang:
    cli
    hlt
    jmp .hang

times (8 * 512) - ($ - $$) db 0
