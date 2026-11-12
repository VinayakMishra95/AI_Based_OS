NASM    = nasm
CC      = clang --target=i386-elf
LD      = ld.lld
QEMU    = qemu-system-i386

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -Iinclude -fno-pic -fno-pie -m32 -mno-sse -mno-sse2 -mno-mmx
LDFLAGS = -T linker.ld -m elf_i386 --oformat binary

BUILD   = build

C_SRCS  = src/kernel.c \
          src/lib/string.c \
          src/arch/gdt.c \
          src/arch/idt.c \
          src/drivers/timer.c \
          src/drivers/serial.c \
          src/drivers/keyboard.c \
          src/drivers/mouse.c \
          src/drivers/ata.c \
          src/mm/memory.c \
          src/fs/ramfs.c \
          src/proc/task.c \
          src/gui/font8x16.c \
          src/gui/fb.c \
          src/gui/desktop.c

ASM_OBJS = $(BUILD)/idt_asm.o
C_OBJS   = $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRCS))

.PHONY: all run run-bridge bridge clean

all: $(BUILD)/os.img

$(BUILD)/boot.bin: boot/boot.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/stage2.bin: boot/stage2.asm
	@mkdir -p $(BUILD)
	$(NASM) -f bin $< -o $@

$(BUILD)/idt_asm.o: src/arch/idt_asm.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(C_OBJS) $(ASM_OBJS) linker.ld
	$(LD) $(LDFLAGS) $(C_OBJS) $(ASM_OBJS) -o $@

$(BUILD)/os.img: $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	cat $(BUILD)/boot.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin > $@
	truncate -s 1440K $@

run: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$(BUILD)/os.img -m 64M -display cocoa,zoom-to-fit=on -serial stdio

run-bridge: $(BUILD)/os.img
	$(QEMU) -drive format=raw,file=$(BUILD)/os.img -m 64M -display cocoa,zoom-to-fit=on -serial tcp:127.0.0.1:4444,server,nowait

bridge:
	python3 tools/ai_bridge.py

clean:
	rm -rf $(BUILD)
