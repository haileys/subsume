CC=i386-elf-gcc
LD=i386-elf-ld
NASM=nasm
KOBJS=src/kernel.o src/mm.o src/interrupt.o

msdos.img: msdos-base.img subsume.com
	cp msdos-base.img msdos.img
	MTOOLSRC=mtoolsrc mcopy subsume.com c:/

subsume.com: subsume.asm subsumek.bin
	$(NASM) -f bin -o $@ $<

subsumek.bin: linker.ld $(KOBJS)
	$(LD) -o $@ -T linker.ld -nostdlib $(KOBJS)

src/%.o: src/%.c src/*.h
	$(CC) -o $@ -Os -Wall -Wextra -pedantic -Werror -ffreestanding -nostdinc -nostdlib -c $<

src/%.o: src/%.asm src/consts.asm
	$(NASM) -I src -f elf32 -o $@ $<

.PHONY: clean
clean:
	rm -f msdos.img *.com *.bin src/*.o
