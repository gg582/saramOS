CC = gcc
AS = gcc
LD = ld

CFLAGS = -ffreestanding -mcmodel=kernel -mno-red-zone -fno-pie -fno-stack-protector -m64 -Wall -Wextra -O2
ASFLAGS = -m64 -c
LDFLAGS = -n -T linker.ld -nostdlib -static -no-pie

OBJS = start.o kernel.o serial.o

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o kernel.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -f *.o kernel.elf

run: kernel.elf
	qemu-system-x86_64 -kernel kernel.elf -nographic -serial stdio
