; Stage 1 Bootloader (MBR, 512 bytes)
; Loads Stage 2 + Kernel from disk into memory and jumps to Stage 2 (0x7E00)

[BITS 16]
[ORG 0x7C00]

%define STAGE2_ADDR     0x7E00
%define KERNEL_LOAD_SEG 0x1000      ; 0x1000:0x0000 = 0x10000
%define STAGE2_SECTORS  8           ; 4 KB for stage2
%define KERNEL_SECTORS  160         ; 80 KB for kernel (plenty for our C GUI OS)

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_boot
    call print_str

    ; Check LBA extensions support (INT 13h AH=41h)
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; 1. Load Stage 2 (starts at LBA 1, 8 sectors -> 0x0000:0x7E00)
    mov word [dap_count], STAGE2_SECTORS
    mov word [dap_offset], STAGE2_ADDR
    mov word [dap_seg], 0x0000
    mov dword [dap_lba_low], 1
    mov dword [dap_lba_high], 0
    call lba_read

    ; 2. Load Kernel (starts at LBA 9, 120 sectors -> 0x1000:0x0000 = 0x10000)
    ; Read in two chunks of 80 sectors to stay safely under 64KB transfer limits
    mov word [dap_count], 80
    mov word [dap_offset], 0x0000
    mov word [dap_seg], KERNEL_LOAD_SEG
    mov dword [dap_lba_low], 1 + STAGE2_SECTORS
    mov dword [dap_lba_high], 0
    call lba_read

    mov word [dap_count], 80
    mov word [dap_offset], 0x0000
    mov word [dap_seg], 0x1A00          ; 0x10000 + 80*512 = 0x1A000
    mov dword [dap_lba_low], 1 + STAGE2_SECTORS + 80
    mov dword [dap_lba_high], 0
    call lba_read

    ; Jump to Stage 2
    mov dl, [boot_drive]
    jmp 0x0000:STAGE2_ADDR

lba_read:
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc disk_error
    ret

disk_error:
    mov si, msg_err
    call print_str
.hang:
    hlt
    jmp .hang

print_str:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_str
.done:
    ret

boot_drive: db 0
msg_boot:   db "AetherOS Booting...", 13, 10, 0
msg_err:    db "Disk read error!", 13, 10, 0

align 4
dap:
    db 0x10             ; size of DAP
    db 0x00             ; unused
dap_count:
    dw 0                ; number of sectors
dap_offset:
    dw 0                ; offset
dap_seg:
    dw 0                ; segment
dap_lba_low:
    dd 0                ; LBA low 32 bits
dap_lba_high:
    dd 0                ; LBA high 32 bits

times 510 - ($ - $$) db 0
dw 0xAA55
