all: ipl.elf

ipl.elf: start.o linker.ld
	$(LD) -T linker.ld start.o -o $@

start.o : start.S
	$(AS) $< -o $@

.PHONY: clean
clean:
	rm -f *.elf *.o
