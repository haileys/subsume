CC=i386-elf-gcc
LD=i386-elf-ld
NASM=nasm
KOBJS= \
	src/debug.o \
	src/interrupt.o \
	src/isrs.o \
	src/kernel.o \
	src/mm.o \
	src/start.o \
	src/task.o \

msdos.img: msdos-base.img subsume.com
	cp msdos-base.img msdos.img
	MTOOLSRC=mtoolsrc mcopy subsume.com c:/

subsume.com: subsume.asm subsumek.bin
	$(NASM) -I src -f bin -o $@ $<

subsumek.bin: linker.ld $(KOBJS)
	$(LD) -nostdlib -o $@ -T linker.ld $(KOBJS)

src/%.o: src/%.c src/*.h
	$(CC) -o $@ -Os -Wall -Wextra -pedantic -ffreestanding -nostdinc -nostdlib -c $<

src/%.o: src/%.asm src/consts.asm
	$(NASM) -I src -f elf32 -o $@ $<

.PHONY: clean
clean:
	rm -f msdos.img *.com *.bin src/*.o
